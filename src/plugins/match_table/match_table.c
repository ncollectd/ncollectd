// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2009 Sebastian Harl
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

typedef struct {
    char *key;
    int value_from;
} tbl_label_t;

struct match_table_metric_s {
    char *metric_prefix;
    char *metric;
    int metric_from;
    match_metric_type_t type;
    char *help;
    label_set_t labels;
    tbl_label_t *labels_from;
    int labels_from_num;
    int value_from;
    struct match_table_metric_s *next;
};
typedef struct match_table_metric_s match_table_metric_t;

typedef struct {
    char *sep;
    int skip_lines;
    char *metric_prefix;
    label_set_t labels;
    int results_num;
    int max_colnum;
    match_table_metric_t *metrics;
} match_table_t;

static void match_table_destroy(void *arg)
{
    match_table_t *tbl = arg;
    if (tbl == NULL)
        return;

    match_table_metric_t *tbl_metric = tbl->metrics;
    while (tbl_metric != NULL) {
        match_table_metric_t *next = tbl_metric->next;

        free(tbl_metric->metric_prefix);
        free(tbl_metric->metric);
        free(tbl_metric->help);
        label_set_reset(&tbl_metric->labels);
        for (int i = 0; i < tbl_metric->labels_from_num; i++)
            free(tbl_metric->labels_from[i].key);
        free(tbl_metric->labels_from);
        free(tbl_metric);
        tbl_metric = next;
    }

    free(tbl->sep);
    free(tbl->metric_prefix);
    label_set_reset(&tbl->labels);

    free(tbl);
}

static int match_table_read_metric(match_table_t *tbl, match_table_metric_t *tbl_metric,
                                   match_metric_family_set_t *set,
                                   char **fields, __attribute__((unused)) int fields_num)
{
    assert(tbl_metric->value_from < fields_num);

    strbuf_t buf = STRBUF_CREATE;

    if (tbl->metric_prefix != NULL)
        strbuf_print(&buf, tbl->metric_prefix);
    if (tbl_metric->metric_prefix != NULL)
        strbuf_print(&buf, tbl_metric->metric_prefix);

    if (tbl_metric->metric_from >= 0) {
        assert(tbl_metric->metric_from < (int)fields_num);
        strbuf_print(&buf, fields[tbl_metric->metric_from]);
    } else {
        strbuf_print(&buf, tbl_metric->metric);
    }

    label_set_t mlabel = {0};

    for (size_t i = 0; i < tbl->labels.num; i++) {
        label_set_add(&mlabel, true, tbl->labels.ptr[i].name, tbl->labels.ptr[i].value);
    }

    for (size_t i = 0; i < tbl_metric->labels.num; i++) {
        label_set_add(&mlabel, true, tbl_metric->labels.ptr[i].name,
                                     tbl_metric->labels.ptr[i].value);
    }

    if (tbl_metric->labels_from_num >= 0) {
        for (int i = 0; i < tbl_metric->labels_from_num; ++i) {
            assert(tbl_metric->labels_from[i].value_from < fields_num);
            label_set_add(&mlabel, true, tbl_metric->labels_from[i].key,
                                         fields[tbl_metric->labels_from[i].value_from]);
        }
    }

    plugin_match_metric_family_set_add(set, buf.ptr, tbl_metric->help, NULL, tbl_metric->type,
                                       &mlabel, fields[tbl_metric->value_from], 0);

    label_set_reset(&mlabel);
    strbuf_destroy(&buf);
    return 0;
}

static int match_table_match(match_metric_family_set_t *set, char *buffer, void *user_data)
{
    match_table_t *tbl = user_data;
    if (tbl == NULL)
        return -1;

    size_t buffer_size = strlen(buffer);

    /* Remove newlines at the end of line. */
    while (buffer_size > 0) {
        if ((buffer[buffer_size - 1] == '\n') || (buffer[buffer_size - 1] == '\r')) {
            buffer[buffer_size - 1] = '\0';
            buffer_size--;
        } else {
            break;
        }
    }

    if (buffer_size == 0)
        return 0;

    char *fields[tbl->max_colnum + 1];
    int i = 0;

    char *ptr = buffer;
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

    match_table_metric_t *tbl_metric = tbl->metrics;
    while (tbl_metric != NULL) {
        match_table_read_metric(tbl, tbl_metric, set, fields, STATIC_ARRAY_SIZE(fields));
        tbl_metric = tbl_metric->next;
    }

    return 0;
}

static int match_table_config_append_label(tbl_label_t **var, int *len, config_item_t *ci)
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

static int match_table_config_metric(match_table_t *tbl, config_item_t *ci)
{
    if (ci->values_num != 0) {
        PLUGIN_ERROR("'result' does not expect any arguments.");
        return -1;
    }

    match_table_metric_t *tbl_metric = calloc(1, sizeof(*tbl_metric));
    if (tbl_metric == NULL) {
        PLUGIN_ERROR("calloc failed: %s.", STRERRNO);
        return -1;
    }

    tbl_metric->metric_from = -1;
    tbl_metric->value_from = -1;

    if (tbl->metrics == NULL) {
        tbl->metrics = tbl_metric;
    } else {
        match_table_metric_t *ptr = tbl->metrics;
        while (ptr->next != NULL)
            ptr = ptr->next;
        ptr->next = tbl_metric;
    }


    int status = 0;
    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "type") == 0) {
            status = cf_util_get_match_metric_type(child, &tbl_metric->type);
        } else if (strcasecmp(child->key, "help") == 0) {
            status = cf_util_get_string(child, &tbl_metric->help);
        } else if (strcasecmp(child->key, "metric") == 0) {
            status = cf_util_get_string(child, &tbl_metric->metric);
        } else if (strcasecmp(child->key, "metric-from") == 0) {
            status = cf_util_get_int(child, &tbl_metric->metric_from);
        } else if (strcasecmp(child->key, "metric-prefix") == 0) {
            status = cf_util_get_string(child, &tbl_metric->metric_prefix);
        } else if (strcasecmp(child->key, "label") == 0) {
            status = cf_util_get_label(child, &tbl_metric->labels);
        } else if (strcasecmp(child->key, "label-from") == 0) {
            status = match_table_config_append_label(&tbl_metric->labels_from,
                                                     &tbl_metric->labels_from_num, child);
        } else if (strcasecmp(child->key, "value-from") == 0) {
            status = cf_util_get_int(child, &tbl_metric->value_from);
        } else {
            PLUGIN_WARNING("Ignoring unknown config key '%s' in 'result'.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0)
        return -1;

    if (tbl_metric->metric == NULL && tbl_metric->metric_from < 0) {
        PLUGIN_ERROR("No 'metric' or 'metric-from' option specified in match_table.");
        status = -1;
    }

    if (tbl_metric->metric != NULL && tbl_metric->metric_from > 0) {
        PLUGIN_ERROR("Only one of 'metric' or 'metric-from' can be set in match_table.");
        status = -1;
    }

    if (tbl_metric->value_from < 0) {
        PLUGIN_ERROR("No 'value-from' option specified for 'metric' in match_table");
        status = -1;
    }

    if (status != 0)
        return -1;

    tbl->results_num++;
    return 0;
}

static int match_table_config(const config_item_t *ci, void **user_data)
{
    *user_data = NULL;

    match_table_t *tbl = calloc(1, sizeof(match_table_t));
    if (tbl == NULL) {
        PLUGIN_ERROR("calloc failed: %s.", STRERRNO);
        return -1;
    }

    int status = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *option = ci->children + i;

        if (strcasecmp(option->key, "separator") == 0) {
            status = cf_util_get_string(option, &tbl->sep);
        } else if (strcasecmp(option->key, "skip-lines") == 0) {
            status = cf_util_get_int(option, &tbl->skip_lines);
        } else if (strcasecmp(option->key, "metric-prefix") == 0) {
            status = cf_util_get_string(option, &tbl->metric_prefix);
        } else if (strcasecmp(option->key, "label") == 0) {
            status = cf_util_get_label(option, &tbl->labels);
        } else if (strcasecmp(option->key, "metric") == 0) {
            status = match_table_config_metric(tbl, option);
        } else {
            PLUGIN_WARNING(" Option '%s' not allowed in match_table.", option->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        match_table_destroy(tbl);
        return status;
    }

    if (tbl->sep == NULL) {
        PLUGIN_ERROR("match_table does not specify any separator.");
        status = -1;
    } else {
        strunescape(tbl->sep, strlen(tbl->sep) + 1);
    }

    if (tbl->metrics == NULL) {
        PLUGIN_ERROR("match_table does not specify any (valid) metrics.");
        status = -1;
    }

    if (status != 0) {
        match_table_destroy(tbl);
        return status;
    }

    match_table_metric_t *tbl_metric = tbl->metrics;
    while (tbl_metric != NULL) {
        for (int j = 0; j < tbl_metric->labels_from_num; ++j) {
            if (tbl_metric->labels_from[j].value_from > tbl->max_colnum)
                tbl->max_colnum = tbl_metric->labels_from[j].value_from;
        }

        if (tbl_metric->metric_from > tbl->max_colnum)
            tbl->max_colnum = tbl_metric->metric_from;

        if (tbl_metric->value_from > tbl->max_colnum)
            tbl->max_colnum = tbl_metric->value_from;

        tbl_metric = tbl_metric->next;
    }

    *user_data = tbl;

    return 0;
}

void module_register(void)
{
    plugin_match_proc_t proc = {0};

    proc.config = match_table_config;
    proc.destroy = match_table_destroy;
    proc.match = match_table_match;

    plugin_register_match("table", proc);
}
