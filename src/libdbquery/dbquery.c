// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2008,2009 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

#include "ncollectd.h"
#include "plugin.h"
#include "libutils/common.h"
#include "libutils/config.h"
#include "libutils/strbuf.h"

#include "libdbquery/dbquery.h"

struct db_result_s;
typedef struct db_result_s db_result_t;
struct db_result_s {
    metric_type_t type;
    char *type_from;
    char *metric;
    char *metric_from;
    char *metric_prefix;
    char *help;
    char *help_from;
    label_set_t labels;
    label_set_t labels_from;
    char *value_from;

    db_result_t *next;
};

struct db_query_s
{
    char *name;
    char *statement;
    char *metric_prefix;
    label_set_t labels;
    unsigned int min_version;
    unsigned int max_version;

    db_result_t *results;
};

struct db_result_preparation_area_s
{
    size_t type_pos;
    char *type_str;
    size_t metric_pos;
    char *metric_str;
    size_t help_pos;
    char *help_str;
    size_t *labels_pos;
    char **labels_buffer;
    size_t value_pos;
    char *value_str;

    struct db_result_preparation_area_s *next;
};
typedef struct db_result_preparation_area_s db_result_preparation_area_t;

struct db_query_preparation_area_s
{
    size_t column_num;
    char *metric_prefix;
    label_set_t labels;
    char *db_name;
    void *user_data;

    db_result_preparation_area_t *result_prep_areas;
};

static int db_config_set_uint(unsigned int *ret_value, config_item_t *ci)
{

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_NUMBER)) {
        PLUGIN_WARNING("The '%s' config option needs exactly one numeric argument.", ci->key);
        return -1;
    }

    double tmp = ci->values[0].value.number;
    if ((tmp < 0.0) || (tmp > ((double)UINT_MAX))) {
        PLUGIN_WARNING("The value given for the '%s' option is out of range.", ci->key);
        return -ERANGE;
    }

    *ret_value = (unsigned int)(tmp + .5);
    return 0;
}

static int db_result_submit(db_result_t *r, db_result_preparation_area_t *r_area,
                             db_query_t const *q, db_query_preparation_area_t *q_area)
{
    assert(r != NULL);

    metric_family_t fam = {0};

    if (r->type_from != NULL) {
        if (!strcasecmp(r_area->type_str, "gauge"))
            fam.type = METRIC_TYPE_GAUGE;
        else if (!strcasecmp(r_area->type_str, "counter"))
            fam.type = METRIC_TYPE_COUNTER;
        else {
            PLUGIN_ERROR("Parse type '%s' as 'gauge', 'counter' failed", r_area->type_str);
            return -1;
        }
    } else {
        fam.type = r->type;
    }

    strbuf_t buf = STRBUF_CREATE;

    if (q_area->metric_prefix != NULL)
        strbuf_putstr(&buf, q_area->metric_prefix);
    if (q->metric_prefix != NULL)
        strbuf_putstr(&buf, q->metric_prefix);
    if (r->metric_prefix != NULL)
        strbuf_putstr(&buf, r->metric_prefix);

    if (r->metric_from != NULL) {
        strbuf_putstr(&buf, r_area->metric_str);
    } else {
        strbuf_putstr(&buf, r->metric);
    }

    fam.name = buf.ptr;

    if (r->help_from != NULL) {
        fam.help = r_area->help_str;
    } else if (r->help != NULL) {
        fam.help = r->help;
    }

    metric_t m = {0};

    for (size_t i = 0; i < q_area->labels.num; i++) {
        metric_label_set(&m, q_area->labels.ptr[i].name, q_area->labels.ptr[i].value);
    }

    for (size_t i = 0; i < q->labels.num; i++) {
        metric_label_set(&m, q->labels.ptr[i].name, q->labels.ptr[i].value);
    }

    for (size_t i = 0; i < r->labels.num; i++) {
        metric_label_set(&m, r->labels.ptr[i].name, r->labels.ptr[i].value);
    }

    for (size_t i = 0; i < r->labels_from.num; i++) {
        metric_label_set(&m, r->labels_from.ptr[i].name, r_area->labels_buffer[i]);
    }

    int ret = 0;
    char *value_str = r_area->value_str;
    if (fam.type == METRIC_TYPE_GAUGE) {
        double value;
        if (0 != parse_double(value_str, &value)) {
            PLUGIN_ERROR("Parsing '%s' as gauge failed.", value_str);
            errno = EINVAL;
            ret = -1;
        }
        m.value.gauge.float64 = value;
    } else if (fam.type == METRIC_TYPE_COUNTER) {
        uint64_t value;
        if (0 != parse_uinteger(value_str, &value)) {
            PLUGIN_ERROR("Parsing '%s' as counter failed.", value_str);
            errno = EINVAL;
            ret = -1;
        }
        m.value.counter.uint64 = value;
    }

    if (ret == 0) {
        metric_family_metric_append(&fam, m);
        plugin_dispatch_metric_family(&fam, 0);
    }

    metric_reset(&m, fam.type);
    metric_family_metric_reset(&fam);
    strbuf_destroy(&buf);
    return ret;
}

static void db_result_finish_result(db_result_t const *r,
                                     db_result_preparation_area_t *prep_area)
{
    if ((r == NULL) || (prep_area == NULL))
        return;

    free(prep_area->labels_pos);
    prep_area->labels_pos = NULL;
    free(prep_area->labels_buffer);
    prep_area->labels_buffer = NULL;
}

static int db_result_handle_result(db_result_t *r,
                                   db_query_preparation_area_t *q_area,
                                   db_result_preparation_area_t *r_area,
                                   db_query_t const *q,
                                   char **column_values)
{
    assert(r && q_area && r_area);

    if (r->type_from != NULL)
        r_area->type_str = column_values[r_area->type_pos];

    if (r->metric_from != NULL)
        r_area->metric_str = column_values[r_area->metric_pos];

    if (r->help_from != NULL)
        r_area->help_str = column_values[r_area->help_pos];

    for (size_t i = 0; i < r->labels_from.num; i++)
        r_area->labels_buffer[i] = column_values[r_area->labels_pos[i]];

    r_area->value_str = column_values[r_area->value_pos];

    return db_result_submit(r, r_area, q, q_area);
}

static int db_result_prepare_result(db_result_t const *r,
                                    db_result_preparation_area_t *prep_area,
                                    char **column_names, size_t column_num)
{
    if ((r == NULL) || (prep_area == NULL))
        return -EINVAL;

#ifdef NCOLLECTD_DEBUG
    assert(prep_area->type_str == NULL);
    assert(prep_area->metric_pos == 0);
    assert(prep_area->metric_str == NULL);
    assert(prep_area->help_pos == 0);
    assert(prep_area->help_str == NULL);
    assert(prep_area->labels_pos == NULL);
    assert(prep_area->labels_buffer == NULL);
    assert(prep_area->value_pos == 0);
    assert(prep_area->value_str == NULL);
#endif

    int status = 0;

    if (r->labels_from.num > 0) {
        prep_area->labels_pos = calloc(r->labels_from.num, sizeof(*prep_area->labels_pos));
        if (prep_area->labels_pos == NULL) {
            PLUGIN_ERROR("calloc failed.");
            status = -ENOMEM;
            goto error;
        }

        prep_area->labels_buffer = calloc(r->labels_from.num, sizeof(*prep_area->labels_buffer));
        if (prep_area->labels_buffer == NULL) {
            PLUGIN_ERROR("calloc failed.");
            status = -ENOMEM;
            goto error;
        }
    }

    /* Determine the position of the type column */
    if (r->type_from != NULL) {
        size_t i;
        for (i = 0; i < column_num; i++) {
            if (strcasecmp(r->type_from, column_names[i]) == 0) {
                prep_area->type_pos = i;
                break;
            }
        }
        if (i >= column_num) {
            PLUGIN_ERROR("Column '%s' could not be found.", r->type_from);
            status = -ENOENT;
            goto error;
        }
    }

    /* Determine the position of the metric column */
    if (r->metric_from != NULL) {
        size_t i;
        for (i = 0; i < column_num; i++) {
            if (strcasecmp(r->metric_from, column_names[i]) == 0) {
                prep_area->metric_pos = i;
                break;
            }
        }
        if (i >= column_num) {
            PLUGIN_ERROR("Column '%s' could not be found.", r->type_from);
            status = -ENOENT;
            goto error;
        }
    }

    /* Determine the position of the metric column {{{ */
    if (r->help_from != NULL) {
        size_t i;
        for (i = 0; i < column_num; i++) {
            if (strcasecmp(r->help_from, column_names[i]) == 0) {
                prep_area->help_pos = i;
                break;
            }
        }
        if (i >= column_num) {
            PLUGIN_ERROR("Column '%s' could not be found.", r->type_from);
            status = -ENOENT;
            goto error;
        }
    }

    /* Determine the position of the labels column */
    for (size_t i = 0; i < r->labels_from.num; i++) {
        size_t j;

        for (j = 0; j < column_num; j++) {
            if (strcasecmp(r->labels_from.ptr[i].value, column_names[j]) == 0) {
                prep_area->labels_pos[i] = j;
                break;
            }
        }

        if (j >= column_num) {
            PLUGIN_ERROR("Column '%s' could not be found.", r->labels_from.ptr[i].value);
            status = -ENOENT;
            goto error;
        }
    }

    /* Determine the position of the value column */
    size_t i;
    for (i = 0; i < column_num; i++) {
        if (strcasecmp(r->value_from, column_names[i]) == 0) {
            prep_area->value_pos = i;
            break;
        }
    }
    if (i >= column_num) {
        PLUGIN_ERROR("Column '%s' could not be found.", r->value_from);
        status = -ENOENT;
        goto error;
    }

    return 0;
error:
    db_result_finish_result(r, prep_area);
    return status;
}

static void db_result_free(db_result_t *r)
{
    if (r == NULL)
        return;

    free(r->type_from);
    free(r->metric);
    free(r->metric_from);
    free(r->metric_prefix);
    free(r->help);
    free(r->help_from);
    label_set_reset(&r->labels);
    label_set_reset(&r->labels_from);
    free(r->value_from);

    db_result_free(r->next);

    free(r);
}

static int db_result_create(const char *query_name, db_result_t **r_head, config_item_t *ci)
{
    if (ci->values_num != 0)
        PLUGIN_WARNING("The 'result' block doesn't accept any arguments. Ignoring %i argument%s.",
                       ci->values_num, (ci->values_num == 1) ? "" : "s");

    db_result_t *r = calloc(1, sizeof(*r));
    if (r == NULL) {
        PLUGIN_ERROR("db_result_create: calloc failed.");
        return -1;
    }

    r->type = METRIC_TYPE_UNKNOWN;
    bool type_setted = false;
    /* Fill the 'db_result_t' structure.. */
    int status = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("type", child->key) == 0) {
            type_setted = true;
            status = cf_util_get_metric_type(child, &r->type);
        } else if (strcasecmp("type-from", child->key) == 0) {
            status = cf_util_get_string(child, &r->type_from);
        } else if (strcasecmp("help", child->key) == 0) {
            status = cf_util_get_string(child, &r->help);
        } else if (strcasecmp("help-from", child->key) == 0) {
            status = cf_util_get_string(child, &r->help_from);
        } else if (strcasecmp("metric", child->key) == 0) {
            status = cf_util_get_string(child, &r->metric);
        } else if (strcasecmp("metric-from", child->key) == 0) {
            status = cf_util_get_string(child, &r->metric_from);
        } else if (strcasecmp("metric-prefix", child->key) == 0) {
            status = cf_util_get_string(child, &r->metric_prefix);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &r->labels);
        } else if (strcasecmp("label-from", child->key) == 0) {
            status = cf_util_get_label(child, &r->labels_from);
        } else if (strcasecmp("value-from", child->key) == 0) {
            status = cf_util_get_string(child, &r->value_from);
        } else {
            PLUGIN_WARNING("Query '%s': Option '%s' not allowed here.", query_name, child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    /* Check that all necessary options have been given. */
    if (status == 0) {
        if (r->metric != NULL && r->metric_from != NULL) {
            PLUGIN_WARNING("Only one of 'Metric' or 'MetricFrom' can be used in query '%s'",
                            query_name);
            status = -1;
        }
        if (r->metric == NULL && r->metric_from == NULL) {
            PLUGIN_WARNING("'Metric' or 'MetricFrom' not given in query '%s'", query_name);
            status = -1;
        }
        if (r->metric_prefix != NULL && r->metric_from == NULL) {
            PLUGIN_WARNING("'MetricFrom' not given in query '%s'", query_name);
            status = -1;
        }

        if (r->help != NULL && r->help_from != NULL) {
            PLUGIN_WARNING("Only one of 'Help' or 'HelpFrom' can be used in query '%s'",
                            query_name);
            status = -1;
        }
        if (r->type_from != NULL && type_setted) {
            PLUGIN_WARNING("Only one of 'Type' or 'TypeFrom' can be used in query '%s'",
                            query_name);
            status = -1;
        }
        if (r->value_from == NULL) {
            PLUGIN_WARNING("'ValueFrom' not given for result in query '%s'", query_name);
            status = -1;
        }
    }

    if (status != 0) {
        db_result_free(r);
        return -1;
    }

    /* If all went well, add this result to the list of results. */
    if (*r_head == NULL) {
        *r_head = r;
    } else {
        db_result_t *last;

        last = *r_head;
        while (last->next != NULL)
            last = last->next;

        last->next = r;
    }

    return 0;
}

static void db_query_free_one(db_query_t *q)
{
    if (q == NULL)
        return;

    free(q->name);
    free(q->statement);
    free(q->metric_prefix);
    label_set_reset(&q->labels);

    db_result_free(q->results);

    free(q);
}

int db_query_create(db_query_t ***ret_query_list, size_t *ret_query_list_len, config_item_t *ci,
                     db_query_create_callback_t cb)
{
    if ((ret_query_list == NULL) || (ret_query_list_len == NULL))
        return -EINVAL;

    db_query_t **query_list = *ret_query_list;
    size_t query_list_len = *ret_query_list_len;

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("db_result_create: The 'Query' block needs exactly one string argument.");
        return -1;
    }

    db_query_t *q = calloc(1, sizeof(*q));
    if (q == NULL) {
        PLUGIN_ERROR("db_query_create: calloc failed.");
        return -1;
    }
    q->min_version = 0;
    q->max_version = UINT_MAX;

    int status = cf_util_get_string(ci, &q->name);
    if (status != 0) {
        free(q);
        return status;
    }

    /* Fill the 'db_query_t' structure.. */
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("statement", child->key) == 0) {
            status = cf_util_get_string(child, &q->statement);
        } else if (strcasecmp("result", child->key) == 0) {
            status = db_result_create(q->name, &q->results, child);
        } else if (strcasecmp("min-version", child->key) == 0) {
            status = db_config_set_uint(&q->min_version, child);
        } else if (strcasecmp("max-version", child->key) == 0) {
            status = db_config_set_uint(&q->max_version, child);
        } else if (strcasecmp("metric-prefix", child->key) == 0) {
            status = cf_util_get_string(child, &q->metric_prefix);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &q->labels);
        } else if (cb != NULL) {
            status = (*cb)(q, child);
            if (status != 0)
                PLUGIN_WARNING("The configuration callback failed to handle '%s'.", child->key);
        } else {
            PLUGIN_WARNING("Query '%s': Option '%s' not allowed here.", q->name, child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    /* Check that all necessary options have been given. */
    if (status == 0) {
        if (q->statement == NULL) {
            PLUGIN_WARNING("Query '%s': No 'Statement' given.", q->name);
            status = -1;
        }
        if (q->results == NULL) {
            PLUGIN_WARNING("Query '%s': No (valid) 'Result' block given.", q->name);
            status = -1;
        }
    }

    /* If all went well, add this query to the list of queries within the
     * database structure. */
    if (status == 0) {
        db_query_t **temp = realloc(query_list, sizeof(*query_list) * (query_list_len + 1));
        if (temp == NULL) {
            PLUGIN_ERROR("db_query_create: realloc failed");
            status = -1;
        } else {
            query_list = temp;
            query_list[query_list_len] = q;
            query_list_len++;
        }
    }

    if (status != 0) {
        db_query_free_one(q);
        return -1;
    }

    *ret_query_list = query_list;
    *ret_query_list_len = query_list_len;

    return 0;
}

void db_query_free(db_query_t **query_list, size_t query_list_len)
{
    if (query_list == NULL)
        return;

    for (size_t i = 0; i < query_list_len; i++)
        db_query_free_one(query_list[i]);

    free(query_list);
}

int db_query_pick_from_list_by_name(const char *name, db_query_t **src_list, size_t src_list_len,
                                     db_query_t ***dst_list, size_t *dst_list_len)
{
    if ((name == NULL) || (src_list == NULL) || (dst_list == NULL) || (dst_list_len == NULL)) {
        PLUGIN_ERROR("Invalid argument.");
        return -EINVAL;
    }

    int num_added = 0;
    for (size_t i = 0; i < src_list_len; i++) {
        if (strcasecmp(name, src_list[i]->name) != 0)
            continue;

        size_t tmp_list_len = *dst_list_len;
        db_query_t **tmp_list = realloc(*dst_list, (tmp_list_len + 1) * sizeof(db_query_t *));
        if (tmp_list == NULL) {
            PLUGIN_ERROR("realloc failed.");
            return -ENOMEM;
        }

        tmp_list[tmp_list_len] = src_list[i];
        tmp_list_len++;

        *dst_list = tmp_list;
        *dst_list_len = tmp_list_len;

        num_added++;
    } /* for (i = 0; i < src_list_len; i++) */

    if (num_added <= 0) {
        PLUGIN_ERROR("Cannot find query '%s'. Make sure the <Query> "
                     "block is above the database definition!", name);
        return -ENOENT;
    } else {
        PLUGIN_DEBUG("Added %i versions of query '%s'.", num_added, name);
    }

    return 0;
}

int db_query_pick_from_list(config_item_t *ci, db_query_t **src_list, size_t src_list_len,
                             db_query_t ***dst_list, size_t *dst_list_len)
{
    if ((ci == NULL) || (src_list == NULL) || (dst_list == NULL) || (dst_list_len == NULL)) {
        PLUGIN_ERROR("Invalid argument.");
        return -EINVAL;
    }

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' config option needs exactly one string argument.", ci->key);
        return -1;
    }
    const char *name = ci->values[0].value.string;

    return db_query_pick_from_list_by_name(name, src_list, src_list_len, dst_list, dst_list_len);
}

const char *db_query_get_name(db_query_t *q)
{
    if (q == NULL)
        return NULL;

    return q->name;
}

char *db_query_get_statement(db_query_t *q)
{
    if (q == NULL)
        return NULL;

    return q->statement;
}

void db_query_preparation_area_set_user_data(db_query_preparation_area_t *p, void *user_data)
{
    if (p == NULL)
        return;

    p->user_data = user_data;
}

void *db_query_preparation_area_get_user_data(db_query_preparation_area_t *p)
{
    if (p == NULL)
        return NULL;

    return p->user_data;
}

int db_query_check_version(db_query_t *q, unsigned int version)
{
    if (q == NULL)
        return -EINVAL;

    if ((version < q->min_version) || (version > q->max_version))
        return 0;

    return 1;
}

void db_query_finish_result(db_query_t const *q, db_query_preparation_area_t *prep_area)
{
    if ((q == NULL) || (prep_area == NULL))
        return;

    prep_area->column_num = 0;
    free(prep_area->metric_prefix);
    prep_area->metric_prefix = NULL;
    free(prep_area->db_name);
    prep_area->db_name = NULL;

    db_result_t *r;
    db_result_preparation_area_t *r_area;
    for (r = q->results, r_area = prep_area->result_prep_areas; r != NULL;
             r = r->next, r_area = r_area->next) {
        /* this may happen during error conditions of the caller */
        if (r_area == NULL)
            break;
        db_result_finish_result(r, r_area);
    }
}

int db_query_handle_result(db_query_t const *q, db_query_preparation_area_t *prep_area,
                            char **column_values)
{
    if ((q == NULL) || (prep_area == NULL))
        return -EINVAL;

    if ((prep_area->column_num < 1) || (prep_area->db_name == NULL)) {
        PLUGIN_ERROR("Query '%s': Query is not prepared; can't handle result.", q->name);
        return -EINVAL;
    }

#ifdef NCOLLECTD_DEBUG
    for (size_t i = 0; i < prep_area->column_num; i++) {
        PLUGIN_DEBUG("db_query_handle_result (%s, %s): column[%" PRIsz "] = %s;",
                     prep_area->db_name, q->name, i, column_values[i]);
    }
#endif

    db_result_preparation_area_t *r_area;
    db_result_t *r;
    int success = 0;
    for (r = q->results, r_area = prep_area->result_prep_areas; r != NULL;
             r = r->next, r_area = r_area->next) {
        int status = db_result_handle_result(r, prep_area, r_area, q, column_values);
        if (status == 0)
            success++;
    }

    if (success == 0) {
        PLUGIN_ERROR("db_query_handle_result (%s, %s): All results failed.",
                     prep_area->db_name, q->name);
        return -1;
    }

    return 0;
}

int db_query_prepare_result(db_query_t const *q, db_query_preparation_area_t *prep_area,
                             char *metric_prefix, label_set_t *labels,
                             const char *db_name, char **column_names, size_t column_num)
{
    if ((q == NULL) || (prep_area == NULL))
        return -EINVAL;

#ifdef NCOLLECTD_DEBUG
    assert(prep_area->column_num == 0);
    assert(prep_area->db_name == NULL);
#endif

    prep_area->column_num = column_num;
    if (labels != NULL)
        label_set_clone(&prep_area->labels, *labels);

    prep_area->db_name = strdup(db_name);
    if (prep_area->db_name == NULL) {
        PLUGIN_ERROR("Query '%s': Prepare failed: Out of memory.", q->name);
        db_query_finish_result(q, prep_area);
        return -ENOMEM;
    }

    if (metric_prefix != NULL) {
        prep_area->metric_prefix = strdup(metric_prefix);
        if (prep_area->metric_prefix == NULL) {
            PLUGIN_ERROR("Query '%s': Prepare failed: Out of memory.", q->name);
            db_query_finish_result(q, prep_area);
            return -ENOMEM;
        }
    }

#ifdef NCOLLECTD_DEBUG
    do {
        for (size_t i = 0; i < column_num; i++) {
            PLUGIN_DEBUG("db_query_prepare_result: query = %s; column[%" PRIsz "] = %s;",
                         q->name, i, column_names[i]);
        }
    } while (0);
#endif

    db_result_preparation_area_t *r_area;
    db_result_t *r;
    for (r = q->results, r_area = prep_area->result_prep_areas; r != NULL;
             r = r->next, r_area = r_area->next) {
        if (!r_area) {
            PLUGIN_ERROR("Query '%s': Invalid number of result preparation areas.", q->name);
            db_query_finish_result(q, prep_area);
            return -EINVAL;
        }

        int status = db_result_prepare_result(r, r_area, column_names, column_num);
        if (status != 0) {
            db_query_finish_result(q, prep_area);
            return status;
        }
    }

    return 0;
}

db_query_preparation_area_t * db_query_allocate_preparation_area(db_query_t *q)
{
    db_query_preparation_area_t *q_area = calloc(1, sizeof(*q_area));
    if (q_area == NULL)
        return NULL;

    db_result_preparation_area_t **next_r_area = &q_area->result_prep_areas;
    for (db_result_t *r = q->results; r != NULL; r = r->next) {
        db_result_preparation_area_t *r_area;

        r_area = calloc(1, sizeof(*r_area));
        if (r_area == NULL) {
            db_result_preparation_area_t *a = q_area->result_prep_areas;

            while (a != NULL) {
                db_result_preparation_area_t *next = a->next;
                free(a);
                a = next;
            }

            free(q_area);
            return NULL;
        }

        *next_r_area = r_area;
        next_r_area = &r_area->next;
    }

    return q_area;
}

void db_query_delete_preparation_area( db_query_preparation_area_t *q_area)
{
    if (q_area == NULL)
        return;

    db_result_preparation_area_t *r_area = q_area->result_prep_areas;
    while (r_area != NULL) {
        db_result_preparation_area_t *area = r_area;

        r_area = r_area->next;

        free(area->labels_pos);
        free(area->labels_buffer);
        free(area);
    }

    free(q_area->db_name);
    free(q_area->metric_prefix);
    label_set_reset(&q_area->labels);

    free(q_area);
}
