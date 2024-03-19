// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"
#include "libutils/dtoa.h"
#include "libxson/tree.h"
#include "jsonpath.h"

typedef struct {
    char *path;
    jsonpath_item_t *expr;
} metric_path_t;

typedef struct {
    char *key;
    metric_path_t path;
} metric_label_path_t;

struct match_jsonpath_metric_s {
    metric_path_t path;
    char *metric_prefix;
    char *metric;
    metric_path_t metric_path;
    metric_path_t metric_root_path;
    match_metric_type_t type;
    char *help;
    metric_path_t help_path;
    metric_path_t help_root_path;
    label_set_t labels;
    size_t labels_path_num;
    metric_label_path_t *labels_path;
    size_t labels_root_path_num;
    metric_label_path_t *labels_root_path;
    metric_path_t value_path;
    metric_path_t value_root_path;
    metric_path_t time_path;
    metric_path_t time_root_path;
    struct match_jsonpath_metric_s *next;
};
typedef struct match_jsonpath_metric_s match_jsonpath_metric_t;

typedef struct {
    char *metric_prefix;
    label_set_t labels;
    match_jsonpath_metric_t *metrics;
} match_jsonpath_t;

static char *metric_path_match_string(metric_path_t *mpath, json_value_t *val)
{
    json_value_list_t vresult = {0};
    jsonpath_exec_result_t eresult = jsonpath_exec(mpath->expr, val, true, &vresult);
    if (eresult == JSONPATH_EXEC_RESULT_NOT_FOUND) {
        json_value_list_destroy(&vresult);
        return NULL;
    }

    if (eresult == JSONPATH_EXEC_RESULT_ERROR) {
        json_value_list_destroy(&vresult);
        return NULL;
    }

    if (vresult.list != NULL) {
        json_value_list_destroy(&vresult);
        return NULL;
    }

    if (vresult.singleton != NULL) {
        json_value_t *value = vresult.singleton;
        switch(value->type) {
        case JSON_TYPE_NULL:
        case JSON_TYPE_OBJECT:
        case JSON_TYPE_ARRAY:
            break;
        case JSON_TYPE_STRING: {
             char *str = strdup(value->string);
             json_value_list_destroy(&vresult);
             return str;
        }    break;
        case JSON_TYPE_NUMBER: {
            char number[DTOA_MAX];
            dtoa(value->number, number, sizeof(number));
             json_value_list_destroy(&vresult);
            return strdup(number);
        }   break;
        case JSON_TYPE_TRUE:
            json_value_list_destroy(&vresult);
            return strdup("true");
            break;
        case JSON_TYPE_FALSE:
            json_value_list_destroy(&vresult);
            return strdup("false");
            break;
        }
    }

    json_value_list_destroy(&vresult);
    return NULL;
}

static cdtime_t metric_path_match_time(metric_path_t *mpath, json_value_t *val)
{
    json_value_list_t vresult = {0};
    jsonpath_exec_result_t eresult = jsonpath_exec(mpath->expr, val, true, &vresult);
    if (eresult == JSONPATH_EXEC_RESULT_NOT_FOUND) {
        json_value_list_destroy(&vresult);
        return 0;
    }

    if (eresult == JSONPATH_EXEC_RESULT_ERROR) {
        json_value_list_destroy(&vresult);
        return 0;
    }

    if (vresult.list != NULL) {
        json_value_list_destroy(&vresult);
        return 0;
    }

    if (vresult.singleton != NULL) {
        json_value_t *value = vresult.singleton;
        switch(value->type) {
        case JSON_TYPE_NULL:
        case JSON_TYPE_OBJECT:
        case JSON_TYPE_ARRAY:
        case JSON_TYPE_TRUE:
        case JSON_TYPE_FALSE:
            break;
        case JSON_TYPE_STRING: {
            double number = 0;
            int status = strtodouble(value->string, &number);
            if (status == 0)
                return DOUBLE_TO_CDTIME_T(number);
            return 0;
        }   break;
        case JSON_TYPE_NUMBER: {
            double number = value->number;
            json_value_list_destroy(&vresult);
            return DOUBLE_TO_CDTIME_T(number);
        }   break;
        }
        return 0;
    }

    json_value_list_destroy(&vresult);
    return 0;
}

static int match_jsonpath_match_metric(match_metric_family_set_t *set,
                                       match_jsonpath_t *jp, match_jsonpath_metric_t *jp_metric,
                                       json_value_t *root, json_value_t *current)
{
    strbuf_t buf = STRBUF_CREATE;

    char *value = NULL;
    if (jp_metric->value_path.expr != NULL) {
        value = metric_path_match_string(&jp_metric->value_path, current);
    } else if (jp_metric->value_root_path.expr != NULL) {
        value = metric_path_match_string(&jp_metric->value_root_path, root);
    }

    if (value == NULL)
        return -1;

    if (jp->metric_prefix != NULL)
        strbuf_putstr(&buf, jp->metric_prefix);
    if (jp_metric->metric_prefix != NULL)
        strbuf_putstr(&buf, jp_metric->metric_prefix);

    if (jp_metric->metric != NULL) {
        strbuf_putstr(&buf, jp_metric->metric);
    } else if (jp_metric->metric_path.expr != NULL) {
        char *str = metric_path_match_string(&jp_metric->metric_path, current);
        if (str == NULL) {
            free(value);
            strbuf_destroy(&buf);
            return -1;
        }
        strbuf_putstr(&buf, str);
        free(str);
    } else if (jp_metric->metric_root_path.expr != NULL) {
        char *str = metric_path_match_string(&jp_metric->metric_root_path, root);
        if (str == NULL) {
            free(value);
            strbuf_destroy(&buf);
            return -1;
        }
        strbuf_putstr(&buf, str);
        free(str);
    }

    char *help = NULL;
    if (jp_metric->help != NULL) {
        help = strdup(jp_metric->help);
    } else if (jp_metric->help_path.expr != NULL) {
        help = metric_path_match_string(&jp_metric->help_path, current);
    } else if (jp_metric->help_root_path.expr != NULL) {
        help = metric_path_match_string(&jp_metric->help_root_path, root);
    }

    label_set_t mlabel = {0};

    for (size_t i = 0; i < jp->labels.num; i++) {
        label_set_add(&mlabel, true, jp->labels.ptr[i].name, jp->labels.ptr[i].value);
    }

    for (size_t i = 0; i < jp_metric->labels.num; i++) {
        label_set_add(&mlabel, true, jp_metric->labels.ptr[i].name,
                                     jp_metric->labels.ptr[i].value);
    }

    for (size_t i = 0; i < jp_metric->labels_path_num; i++) {
        char *str = metric_path_match_string(&jp_metric->labels_path[i].path, current);
        if (str != NULL) {
            label_set_add(&mlabel, true, jp_metric->labels_path[i].key, str);
            free(str);
        }
    }

    for (size_t i = 0; i < jp_metric->labels_root_path_num; i++) {
        char *str = metric_path_match_string(&jp_metric->labels_root_path[i].path, current);
        if (str != NULL) {
            label_set_add(&mlabel, true, jp_metric->labels_root_path[i].key, str);
            free(str);
        }

    }

    cdtime_t t = 0;
    if (jp_metric->time_path.expr != NULL)
        t = metric_path_match_time(&jp_metric->time_path, current);
    else if (jp_metric->time_root_path.expr != NULL)
        t = metric_path_match_time(&jp_metric->time_root_path, root);

    plugin_match_metric_family_set_add(set, buf.ptr, help, NULL, jp_metric->type,
                                       &mlabel, value, t);

    label_set_reset(&mlabel);
    strbuf_destroy(&buf);
    free(help);
    free(value);

    return 0;
}

static int match_jsonpath_match(match_metric_family_set_t *set, char *buffer, void *user_data)
{
    match_jsonpath_t *jp = user_data;
    if (jp == NULL)
        return -1;

    if ((set == NULL) || (buffer == NULL))
        return -1;

    char error[256] = {'\0'};
    json_value_t *root = json_tree_parser(buffer, error, sizeof(error));
    if (root == NULL) {
        PLUGIN_ERROR("Error parsing json: %s", error);
        return -1;
    }

    match_jsonpath_metric_t *jp_metric = NULL;
    for (jp_metric = jp->metrics; jp_metric != NULL; jp_metric = jp_metric->next) {

        json_value_list_t vresult = {0};
        jsonpath_exec_result_t eresult = jsonpath_exec(jp_metric->path.expr, root, true, &vresult);
        if (eresult == JSONPATH_EXEC_RESULT_NOT_FOUND) {
            json_value_list_destroy(&vresult);
            continue;
        }

        if (eresult == JSONPATH_EXEC_RESULT_ERROR) {
            json_value_list_destroy(&vresult);
            continue;
        }

        if (vresult.singleton != NULL) {
            match_jsonpath_match_metric(set, jp, jp_metric, root, vresult.singleton);
        } else if (vresult.list != NULL) {
            jsonpath_list_cell_t *cell;
            foreach(cell, vresult.list) {
                match_jsonpath_match_metric(set, jp, jp_metric, root, cell->ptr_value);
            }
        }

        json_value_list_destroy(&vresult);
    }

    json_value_free(root);

    return 0;
}

static void metric_path_destroy(metric_path_t *mpath)
{
    if (mpath == NULL)
        return;
    free(mpath->path);
    jsonpath_item_free(mpath->expr);
}

static void metric_label_path_destroy(metric_label_path_t *labels, size_t num)
{
    if (labels == NULL)
        return;

    for (size_t i = 0; i < num; i++) {
        free(labels[i].key);
        metric_path_destroy(&labels[i].path);
    }

    free(labels);
}

static void match_jsonpath_destroy(void *user_data)
{
    match_jsonpath_t *jp = user_data;
    if (jp == NULL)
        return;

    match_jsonpath_metric_t *jp_metric = jp->metrics;
    while(jp_metric != NULL) {
        metric_path_destroy(&jp_metric->path);
        free(jp_metric->metric_prefix);
        free(jp_metric->metric);
        metric_path_destroy(&jp_metric->metric_path);
        metric_path_destroy(&jp_metric->metric_root_path);
        free(jp_metric->help);
        metric_path_destroy(&jp_metric-> help_path);
        metric_path_destroy(&jp_metric->help_root_path);
        label_set_reset(&jp_metric->labels);
        metric_label_path_destroy(jp_metric->labels_path, jp_metric->labels_path_num);
        metric_label_path_destroy(jp_metric->labels_root_path, jp_metric->labels_root_path_num);
        metric_path_destroy(&jp_metric->value_path);
        metric_path_destroy(&jp_metric->value_root_path);
        metric_path_destroy(&jp_metric->time_path);
        metric_path_destroy(&jp_metric->time_root_path);

        match_jsonpath_metric_t *next = jp_metric->next;
        free(jp_metric);
        jp_metric = next;
    }
    free(jp->metric_prefix);
    label_set_reset(&jp->labels);

    free(jp);
}

static int config_metric_path(const config_item_t *ci, metric_path_t *mpath)
{
    if (mpath->path != NULL) {
        PLUGIN_ERROR("The '%s' option in %s:%d has already set.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    mpath->path = strdup(ci->values[0].value.string);
    if (mpath->path == NULL) {
        PLUGIN_ERROR("strdup failed");
        return -1;
    }

    // char error[256] = {'\0'};
    mpath->expr = jsonpath_parser(mpath->path);
    if (mpath->expr == NULL) {
        PLUGIN_ERROR("Parsing jsonpath: '%s' failed.", mpath->path);
        free(mpath->path);
        mpath->path = NULL;
        return -1;
    }

    return 0;
}
static int config_metric_path_label_append(const config_item_t *ci, metric_label_path_t **var, size_t *len)
{
    if (ci->values_num != 2) {
        PLUGIN_ERROR("'%s' expects two arguments.", ci->key);
        return -1;
    }

    if ((CONFIG_TYPE_STRING != ci->values[0].type) || (CONFIG_TYPE_STRING != ci->values[1].type)) {
        PLUGIN_ERROR("'%s' expects two strings arguments.", ci->key);
        return -1;
    }

    metric_label_path_t *tmp = realloc(*var, ((*len) + 1) * sizeof(**var));
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

    tmp[*len].path.path = strdup(ci->values[1].value.string);
    if (tmp[*len].path.path == NULL) {
        free(tmp[*len].key);
        PLUGIN_ERROR("strdup failed.");
        return -1;
    }

    tmp[*len].path.expr = jsonpath_parser(tmp[*len].path.path);
    if (tmp[*len].path.expr == NULL) {
        PLUGIN_ERROR("Parsing jsonpath: '%s' failed.", tmp[*len].path.path);
        free(tmp[*len].key);
        free(tmp[*len].path.path);
        return -1;
    }

    *len = (*len) + 1;
    return 0;
}

static int config_match_jsonpath_metric(const config_item_t *ci, match_jsonpath_t *jp)
{
    match_jsonpath_metric_t *jp_metric = calloc(1, sizeof(*jp_metric));
    if (jp_metric == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    if (jp->metrics == NULL) {
        jp->metrics = jp_metric;
    } else {
        match_jsonpath_metric_t *ptr = jp->metrics;
        while (ptr->next != NULL)
            ptr = ptr->next;
        ptr->next = jp_metric;
    }

    jp_metric->type = MATCH_METRIC_TYPE_GAUGE_LAST;

    int status = 0;
    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *option = ci->children + i;

        if (strcasecmp("path", option->key) == 0) {
            status = config_metric_path(option, &jp_metric->path);
        } else if (strcasecmp("type", option->key) == 0) {
            status = cf_util_get_match_metric_type(option, &jp_metric->type);
        } else if (strcasecmp("help", option->key) == 0) {
            status = cf_util_get_string(option, &jp_metric->help);
        } else if (strcasecmp("help-path", option->key) == 0) {
            status = config_metric_path(option, &jp_metric->help_path);
        } else if (strcasecmp("help-root-path", option->key) == 0) {
            status = config_metric_path(option, &jp_metric->help_root_path);
        } else if (strcasecmp("metric", option->key) == 0) {
            status = cf_util_get_string(option, &jp_metric->metric);
        } else if (strcasecmp("metric-path", option->key) == 0) {
            status = config_metric_path(option, &jp_metric->metric_path);
        } else if (strcasecmp("metric-root-path", option->key) == 0) {
            status = config_metric_path(option, &jp_metric->metric_root_path);
        } else if (strcasecmp("metric-prefix", option->key) == 0) {
            status = cf_util_get_string(option, &jp_metric->metric_prefix);
        } else if (strcasecmp("label", option->key) == 0) {
            status = cf_util_get_label(option, &jp_metric->labels);
        } else if (strcasecmp("label-path", option->key) == 0) {
            status = config_metric_path_label_append(option, &jp_metric->labels_path,
                                                             &jp_metric->labels_path_num);
        } else if (strcasecmp("label-root-path", option->key) == 0) {
            status = config_metric_path_label_append(option, &jp_metric->labels_root_path,
                                                             &jp_metric->labels_root_path_num);
        } else if (strcasecmp("value-path", option->key) == 0) {
            status = config_metric_path(option, &jp_metric->value_path);
        } else if (strcasecmp("value-root-path", option->key) == 0) {
            status = config_metric_path(option, &jp_metric->value_root_path);
        } else if (strcasecmp("time-path", option->key) == 0) {
            status = config_metric_path(option, &jp_metric->time_path);
        } else if (strcasecmp("time-root-path", option->key) == 0) {
            status = config_metric_path(option, &jp_metric->time_root_path);
        } else {
            PLUGIN_WARNING("Option `%s' not allowed here.", option->key);
            status = -1;
        }

        if (status != 0) {
            fprintf(stderr, "config_match_jsonpath_metric %s\n", option->key);
            break;
        }
    }

    if (status != 0)
        return -1;

    if (jp_metric->path.expr == NULL) {
        PLUGIN_ERROR("Error missing 'path' in metric block at %s:%d",
                     cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if (((jp_metric->metric != NULL) && (jp_metric->metric_path.expr != NULL)) ||
        ((jp_metric->metric != NULL) && (jp_metric->metric_root_path.expr != NULL)) ||
        ((jp_metric->metric_path.expr != NULL) && (jp_metric->metric_root_path.expr != NULL))) {

        PLUGIN_ERROR("Error 'metric', 'metric-path' and 'metric-root-path' "
                     "are set in metric block at %s:%d", cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if ((jp_metric->metric == NULL) &&
        (jp_metric->metric_path.expr == NULL) && (jp_metric->metric_root_path.expr == NULL)) {

        PLUGIN_ERROR("Error missing 'metric', 'metric-path' or 'metric-root-path' "
                     "in metric block at %s:%d", cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if ((jp_metric->help_path.expr != NULL) && (jp_metric->help_root_path.expr != NULL)) {
        PLUGIN_ERROR("Error 'help-path' and 'help-root-path' "
                     "are set in metric block at %s:%d", cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if ((jp_metric->value_path.expr != NULL) && (jp_metric->value_root_path.expr != NULL)) {
        PLUGIN_ERROR("Error 'value-path' and 'value-root-path' "
                     "are set in metric block at %s:%d", cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if ((jp_metric->value_path.expr == NULL) && (jp_metric->value_root_path.expr == NULL)) {
        PLUGIN_ERROR("Missing 'value-path' or 'value-root-path' in metric %s:%d",
                     cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }


    if ((jp_metric->time_path.expr != NULL) && (jp_metric->time_root_path.expr != NULL)) {
        PLUGIN_ERROR("Error 'time-path' and 'time-root-path' "
                     "are set in metric block at %s:%d", cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    return 0;
}

static int match_jsonpath_config(const config_item_t *ci, void **user_data)
{
    *user_data = NULL;

    match_jsonpath_t *jp = calloc(1, sizeof(*jp));
    if (jp == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *option = ci->children + i;

        if (strcasecmp("metric-prefix", option->key) == 0) {
            status = cf_util_get_string(option, &jp->metric_prefix);
        } else if (strcasecmp("label", option->key) == 0) {
            status = cf_util_get_label(option, &jp->labels);
        } else if (strcasecmp("metric", option->key) == 0) {
            status = config_match_jsonpath_metric(option, jp);
        } else {
            PLUGIN_WARNING("Option '%s' not allowed here.", option->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        match_jsonpath_destroy(jp);
        return -1;
    }

    *user_data = jp;

    return 0;
}

void module_register(void)
{
    plugin_match_proc_t proc = {0};

    proc.config = match_jsonpath_config;
    proc.destroy = match_jsonpath_destroy;
    proc.match = match_jsonpath_match;

    plugin_register_match("jsonpath", proc);
}
