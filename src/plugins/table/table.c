// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2009 Sebastian Harl
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>

#include "plugin.h"
#include "libutils/common.h"

typedef struct {
    char *key;
    int value_from;
} tbl_label_t;

typedef struct {
    char *metric_prefix;
    char *metric;
    int metric_from;
    metric_type_t type;
    char *help;
    label_set_t labels;
    tbl_label_t *labels_from;
    int labels_from_num;
    int value_from;
    double scale;
    double shift;
} tbl_result_t;

typedef struct {
    char *file;
    char *sep;
    int skip_lines;
    char *metric_prefix;
    label_set_t labels;
    tbl_result_t *results;
    int results_num;
    int max_colnum;
} tbl_t;

static void tbl_result_setup(tbl_result_t *res)
{
    memset(res, 0, sizeof(*res));
    res->metric_from = -1;
    res->value_from = -1;
}

static void tbl_result_free(tbl_result_t *res)
{
    if (res == NULL)
        return;

    free(res->metric_prefix);
    free(res->metric);
    free(res->help);
    label_set_reset(&res->labels);

    for (int i = 0; i < res->labels_from_num; i++)
        free(res->labels_from[i].key);
    free(res->labels_from);
}

static void tbl_free(void *arg)
{
    tbl_t *tbl = arg;

    if (tbl == NULL)
        return;

    free(tbl->file);
    free(tbl->sep);
    free(tbl->metric_prefix);
    label_set_reset(&tbl->labels);

    /* (tbl->results == NULL) -> (tbl->results_num == 0) */
    assert((tbl->results != NULL) || (tbl->results_num == 0));
    for (int i = 0; i < tbl->results_num; ++i)
        tbl_result_free(tbl->results + i);
    free(tbl->results);

    free(tbl);
}

static int tbl_result_dispatch(tbl_t *tbl, tbl_result_t *res, char **fields,
                               __attribute__((unused)) int fields_num)
{
    assert(res->value_from < fields_num);
    metric_t m = {0};

    if (res->type == METRIC_TYPE_GAUGE) {
        double gauge;
        if (parse_double(fields[res->value_from], &gauge) != 0)
            return -1;
        m.value = VALUE_GAUGE((res->scale * gauge) + res->shift);
    } else if (res->type == METRIC_TYPE_COUNTER) {
        uint64_t counter;
        if (parse_uinteger(fields[res->value_from], &counter) != 0)
            return -1;
        m.value = VALUE_COUNTER(counter);
    }

    metric_family_t fam = {0};

    fam.type = res->type;
    fam.help = res->help;

    strbuf_t buf = STRBUF_CREATE;

    int status = 0;
    if (tbl->metric_prefix != NULL)
        status |= strbuf_putstr(&buf, tbl->metric_prefix);
    if (res->metric_prefix != NULL)
        status |= strbuf_putstr(&buf, res->metric_prefix);

    if (res->metric_from >= 0) {
        assert(res->metric_from < (int)fields_num);
        status |= strbuf_putstr(&buf, fields[res->metric_from]);
    } else {
        status |= strbuf_putstr(&buf, res->metric);
    }

    if (status != 0) {
        PLUGIN_ERROR("Cannot format metric name.");
        strbuf_destroy(&buf);
    }

    fam.name = buf.ptr;


    for (size_t i = 0; i < tbl->labels.num; i++) {
        metric_label_set(&m, tbl->labels.ptr[i].name, tbl->labels.ptr[i].value);
    }

    for (size_t i = 0; i < res->labels.num; i++) {
        metric_label_set(&m, res->labels.ptr[i].name, res->labels.ptr[i].value);
    }

    if (res->labels_from_num >= 0) {
        for (int i = 0; i < res->labels_from_num; ++i) {
            assert(res->labels_from[i].value_from < fields_num);
            metric_label_set(&m, res->labels_from[i].key, fields[res->labels_from[i].value_from]);
        }
    }

    metric_family_metric_append(&fam, m);
    metric_reset(&m, fam.type);

    plugin_dispatch_metric_family(&fam, 0);

    strbuf_destroy(&buf);

    return 0;
}

static int tbl_parse_line(tbl_t *tbl, char *line)
{
    char *fields[tbl->max_colnum + 1];
    int i = 0;

    char *ptr = line;
    char *saveptr = NULL;
    while ((fields[i] = strtok_r(ptr, tbl->sep, &saveptr)) != NULL) {
        ptr = NULL;
        i++;

        if (i > tbl->max_colnum)
            break;
    }

    if (i <= tbl->max_colnum) {
        PLUGIN_WARNING("Not enough columns in line (expected at least %d, got %d).",
                       tbl->max_colnum + 1, i);
        return -1;
    }

    for (i = 0; i < tbl->results_num; ++i) {
        if (tbl_result_dispatch(tbl, tbl->results + i, fields, STATIC_ARRAY_SIZE(fields)) != 0) {
            PLUGIN_ERROR("Failed to dispatch result.");
            continue;
        }
    }
    return 0;
}

static int tbl_read_table(user_data_t *user_data)
{
    if (user_data == NULL)
        return -1;

    tbl_t *tbl = user_data->data;

    FILE *fh = fopen(tbl->file, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Failed to open file \"%s\": %s.", tbl->file, STRERRNO);
        return -1;
    }

    char buf[4096];
    buf[sizeof(buf) - 1] = '\0';
    int line = 0;
    while (fgets(buf, sizeof(buf), fh) != NULL) {
        if (buf[sizeof(buf) - 1] != '\0') {
            buf[sizeof(buf) - 1] = '\0';
            PLUGIN_WARNING("Table %s: Truncated line: %s", tbl->file, buf);
        }
        line++;
        if (line <= tbl->skip_lines)
            continue;

        if (tbl_parse_line(tbl, buf) != 0) {
            PLUGIN_WARNING("Table %s: Failed to parse line: %s", tbl->file, buf);
            continue;
        }
    }

    if (ferror(fh) != 0) {
        PLUGIN_ERROR("Failed to read from file \"%s\": %s.", tbl->file, STRERRNO);
        fclose(fh);
        return -1;
    }

    fclose(fh);
    return 0;
}

static int tbl_config_append_label(tbl_label_t **var, int *len, config_item_t *ci)
{
    if (ci->values_num != 2) {
        PLUGIN_ERROR("'%s' expects two arguments.", ci->key);
        return -1;
    }
    if ((CONFIG_TYPE_STRING != ci->values[0].type) ||
        (CONFIG_TYPE_NUMBER != ci->values[1].type)) {
        PLUGIN_ERROR("'%s' expects a string and a numerical argument.", ci->key);
        return -1;
    }

    tbl_label_t *tmp = realloc(*var, ((*len) + 1) * sizeof(**var));
    if (tmp == NULL) {
        PLUGIN_ERROR("realloc failed: %s.", STRERRNO);
        return -1;
    }
    *var = tmp;

    tmp[*len].key = strdup(ci->values[0].value.string);
    if (tmp[*len].key == NULL) {
        PLUGIN_ERROR("strdup failed.");
        return -1;
    }

    tmp[*len].value_from = ci->values[1].value.number;

    *len = (*len) + 1;
    return 0;
}

static int tbl_config_result(tbl_t *tbl, config_item_t *ci)
{
    if (ci->values_num != 0) {
        PLUGIN_ERROR("'result' does not expect any arguments.");
        return -1;
    }

    tbl_result_t *res =
            realloc(tbl->results, (tbl->results_num + 1) * sizeof(*tbl->results));
    if (res == NULL) {
        PLUGIN_ERROR("realloc failed: %s.", STRERRNO);
        return -1;
    }

    tbl->results = res;

    res = tbl->results + tbl->results_num;
    tbl_result_setup(res);

    res->scale = 1.0;
    res->shift = 0;

    int status = 0;
    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *c = ci->children + i;

        if (strcasecmp(c->key, "type") == 0) {
            status = cf_util_get_metric_type(c, &res->type);
        } else if (strcasecmp(c->key, "help") == 0) {
            status = cf_util_get_string(c, &res->help);
        } else if (strcasecmp(c->key, "metric") == 0) {
            status = cf_util_get_string(c, &res->metric);
        } else if (strcasecmp(c->key, "metric-from") == 0) {
            status = cf_util_get_int(c, &res->metric_from);
        } else if (strcasecmp(c->key, "metric-prefix") == 0) {
            status = cf_util_get_string(c, &res->metric_prefix);
        } else if (strcasecmp(c->key, "label") == 0) {
            status = cf_util_get_label(c, &res->labels);
        } else if (strcasecmp(c->key, "label-from") == 0) {
            status = tbl_config_append_label(&res->labels_from, &res->labels_from_num, c);
        } else if (strcasecmp(c->key, "value-from") == 0) {
            status = cf_util_get_int(c, &res->value_from);
        } else if (strcasecmp(c->key, "shift") == 0) {
            status = cf_util_get_double(c, &res->shift);
        } else if (strcasecmp(c->key, "scale") == 0) {
            status = cf_util_get_double(c, &res->scale);
        } else {
            PLUGIN_WARNING("Ignoring unknown config key '%s' in 'result'.", c->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        tbl_result_free(res);
        return status;
    }

    if ((res->metric == NULL) && (res->metric_from < 0)) {
        PLUGIN_ERROR("No 'metric' or 'metric-from' option specified for "
                     "'result' in table '%s'.", tbl->file);
        status = -1;
    }

    if ((res->metric != NULL) && (res->metric_from > 0)) {
        PLUGIN_ERROR("Only one of 'metric' or 'metric-from' can be set in "
                     "'Result' in table \"%s\".", tbl->file);
        status = -1;
    }

    if (res->value_from < 0) {
        PLUGIN_ERROR("No 'value-from' option specified for 'result' in table \"%s\".", tbl->file);
        status = -1;
    }

    if (status != 0) {
        tbl_result_free(res);
        return status;
    }

    tbl->results_num++;
    return 0;
}

static int tbl_config_table(config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("'table' expects a single string argument.");
        return 1;
    }

    tbl_t *tbl = calloc(1, sizeof(tbl_t));
    if (tbl == NULL) {
        PLUGIN_ERROR("realloc failed: %s.", STRERRNO);
        return -1;
    }

    tbl->file = sstrdup(ci->values[0].value.string);
    if (tbl->file == NULL) {
        PLUGIN_ERROR("strdup failed");
        tbl_free(tbl);
        return -1;
    }

    cdtime_t interval = 0;

    int status = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *c = ci->children + i;

        if (strcasecmp(c->key, "separator") == 0) {
            status = cf_util_get_string(c, &tbl->sep);
        } else if (strcasecmp(c->key, "skip-lines") == 0) {
            status = cf_util_get_int(c, &tbl->skip_lines);
        } else if (strcasecmp(c->key, "metric-prefix") == 0) {
            status = cf_util_get_string(c, &tbl->metric_prefix);
        } else if (strcasecmp(c->key, "label") == 0) {
            status = cf_util_get_label(c, &tbl->labels);
        } else if (strcasecmp(c->key, "interval") == 0) {
            status = cf_util_get_cdtime(c, &interval);
        } else if (strcasecmp(c->key, "result") == 0) {
            status = tbl_config_result(tbl, c);
        } else {
            PLUGIN_WARNING(" Option '%s' not allowed in 'table' '%s'.", c->key, tbl->file);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        tbl_free(tbl);
        return status;
    }

    if (tbl->sep == NULL) {
        tbl->sep = strdup(" ");
        if (tbl->sep == NULL) {
            PLUGIN_ERROR("Table '%s': failed to strdup separator.", tbl->file);
            tbl_free(tbl);
            return -1;
        }
    } else {
        strunescape(tbl->sep, strlen(tbl->sep) + 1);
    }

    if (tbl->results == NULL) {
        assert(tbl->results_num == 0);
        PLUGIN_ERROR("Table '%s' does not specify any (valid) results.", tbl->file);
        tbl_free(tbl);
        return -1;
    }

    for (int i = 0; i < tbl->results_num; ++i) {
        tbl_result_t *res = tbl->results + i;

        for (int j = 0; j < res->labels_from_num; ++j)
            if (res->labels_from[j].value_from > tbl->max_colnum)
                tbl->max_colnum = res->labels_from[j].value_from;

        if (res->metric_from > tbl->max_colnum)
            tbl->max_colnum = res->metric_from;

        if (res->value_from > tbl->max_colnum)
            tbl->max_colnum = res->value_from;
    }

    return plugin_register_complex_read("table", tbl->file, tbl_read_table, interval,
                                        &(user_data_t){.data=tbl, .free_func=tbl_free});
}

static int tbl_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *c = ci->children + i;

        if (strcasecmp(c->key, "table") == 0) {
            status = tbl_config_table(c);
        } else {
            PLUGIN_ERROR("Unknown config key '%s'.", c->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("table", tbl_config);
}
