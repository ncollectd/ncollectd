// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2013 Kris Nielander
// SPDX-FileCopyrightText: Copyright (C) 2013 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Kris Nielander <nielander at fox-it.com>
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

typedef struct {
    char *key;
    int value_from;
} metric_label_from_t;

struct match_csv_metric_s {
    char *metric_prefix;
    char *metric;
    ssize_t metric_from;
    match_metric_type_t type;
    char *help;
    label_set_t labels;
    metric_label_from_t *labels_from;
    size_t labels_from_num;
    ssize_t value_from;
    struct match_csv_metric_s *next;
};
typedef struct match_csv_metric_s match_csv_metric_t;

typedef struct {
    char *metric_prefix;
    label_set_t labels;
    ssize_t time_from;
    char field_separator;
    match_csv_metric_t *metrics;
} match_csv_t;

static cdtime_t match_csv_parse_time(char const *tbuf)
{
    double t;
    char *endptr = NULL;

    errno = 0;
    t = strtod(tbuf, &endptr);
    if ((errno != 0) || (endptr == NULL) || (endptr[0] != 0))
        return cdtime();

    return DOUBLE_TO_CDTIME_T(t);
}

int match_csv_read_metric(match_csv_t *csv, match_csv_metric_t *csv_metric,
                          match_metric_family_set_t *set,
                          char **fields, ssize_t fields_num)
{
    assert(csv_metric->value_from >= 0);
    if (csv_metric->value_from >= fields_num)
        return EINVAL;

    cdtime_t t = 0;
    if (csv->time_from >= 0) {
        if (csv->time_from >= fields_num)
            return EINVAL;
        t = match_csv_parse_time(fields[csv->time_from]);
    }

    strbuf_t buf = STRBUF_CREATE;

    if (csv->metric_prefix != NULL)
        strbuf_print(&buf, csv->metric_prefix);
    if (csv_metric->metric_prefix != NULL)
        strbuf_print(&buf, csv_metric->metric_prefix);

    if (csv_metric->metric_from >= 0) {
        assert(csv_metric->metric_from < fields_num);
        strbuf_print(&buf, fields[csv_metric->metric_from]);
    } else {
        strbuf_print(&buf, csv_metric->metric);
    }

    label_set_t mlabel = {0};

    for (size_t i = 0; i < csv->labels.num; i++) {
        label_set_add(&mlabel, true, csv->labels.ptr[i].name, csv->labels.ptr[i].value);
    }

    for (size_t i = 0; i < csv_metric->labels.num; i++) {
        label_set_add(&mlabel, true, csv_metric->labels.ptr[i].name,
                                     csv_metric->labels.ptr[i].value);
    }

    if (csv_metric->labels_from_num > 0) {
        for (size_t i = 0; i < csv_metric->labels_from_num; ++i) {
            assert(csv_metric->labels_from[i].value_from < fields_num);
            label_set_add(&mlabel, true, csv_metric->labels_from[i].key,
                                         fields[csv_metric->labels_from[i].value_from]);
        }
    }

    plugin_match_metric_family_set_add(set, buf.ptr, csv_metric->help, NULL, csv_metric->type,
                                       &mlabel, fields[csv_metric->value_from], t);

    label_set_reset(&mlabel);
    strbuf_destroy(&buf);
    return 0;
}

static bool match_csv_check_index(ssize_t index, size_t fields_num)
{
    if (index < 0)
        return true;
    else if (((size_t)index) < fields_num)
        return true;

    PLUGIN_ERROR("Request for index %zd when only %" PRIsz " fields are available.",
                 index, fields_num);
    return false;
}


static int match_csv_match(match_metric_family_set_t *set, char *buffer, void *user_data)
{
    match_csv_t *csv = user_data;
    if (csv == NULL)
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

    /* Ignore empty lines. */
    if ((buffer_size == 0) || (buffer[0] == '#'))
        return 0;

    /* Count the number of fields. */
    size_t metrics_num = 1;
    for (size_t i = 0; i < buffer_size; i++) {
        if (buffer[i] == csv->field_separator)
            metrics_num++;
    }

    if (metrics_num == 1) {
        PLUGIN_ERROR("Last line does not contain enough values.");
        return -1;
    }

    /* Create a list of all values */
    char **metrics = calloc(metrics_num, sizeof(*metrics));
    if (metrics == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return ENOMEM;
    }

    char *ptr = buffer;
    metrics[0] = ptr;
    size_t i = 1;
    for (ptr = buffer; *ptr != 0; ptr++) {
        if (*ptr != csv->field_separator)
            continue;

        *ptr = 0;
        metrics[i] = ptr + 1;
        i++;
    }
    assert(i == metrics_num);

    match_csv_metric_t *csv_metric = csv->metrics;
    while (csv_metric != NULL) {
        if (!match_csv_check_index(csv_metric->value_from, metrics_num) ||
            !match_csv_check_index(csv->time_from, metrics_num))
            continue;
        match_csv_read_metric(csv, csv_metric, set, metrics, metrics_num);
        csv_metric = csv_metric->next;
    }

    free(metrics);
    return 0;
}

static void match_csv_destroy(void *user_data)
{
    match_csv_t *csv = user_data;
    if (csv == NULL)
        return;

    match_csv_metric_t *csv_metric = csv->metrics;
    while (csv_metric != NULL) {
        match_csv_metric_t *next = csv_metric->next;
        free(csv_metric->metric_prefix);
        free(csv_metric->metric);
        free(csv_metric->help);
        label_set_reset(&csv_metric->labels);
        for (size_t i = 0; i < csv_metric->labels_from_num; i++)
            free(csv_metric->labels_from[i].key);
        free(csv_metric);
        csv_metric = next;
    }

    free(csv->metric_prefix);
    label_set_reset(&csv->labels);

    free(csv);
}

static int match_csv_config_get_index(config_item_t *ci, ssize_t *ret_index)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_NUMBER)) {
        PLUGIN_WARNING("The '%s' config option needs exactly one integer argument.", ci->key);
        return -1;
    }

    if (ci->values[0].value.number < 0) {
        PLUGIN_WARNING("The '%s' config option must be positive (or zero).", ci->key);
        return -1;
    }

    *ret_index = (ssize_t)ci->values[0].value.number;
    return 0;
}


static int match_csv_config_append_label(metric_label_from_t **var, size_t *len, config_item_t *ci)
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

    metric_label_from_t *tmp = realloc(*var, ((*len) + 1) * sizeof(**var));
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

/*
  match csv {
      MetricPrefix
      Label
      TimeFrom
      FieldSeparator
      metric {
          Type
          Help
          Metric
          MetricFrom
          MetricPrefix
          Label
          LabelFrom
          ValueFrom
          TimeFrom
      }
  }

 */
static int match_csv_config_metric(const config_item_t *ci, match_csv_t *csv)
{
    match_csv_metric_t *csv_metric = calloc(1, sizeof(*csv_metric));
    if (csv_metric == NULL)
        return -1;

    if (csv->metrics == NULL) {
        csv->metrics = csv_metric;
    } else {
        match_csv_metric_t *ptr = csv->metrics;
        while (ptr->next != NULL)
            ptr = ptr->next;
        ptr->next = csv_metric;
    }

    csv_metric->metric_from = -1;
    csv_metric->value_from = -1;
    csv_metric->type = MATCH_METRIC_TYPE_GAUGE;

    int status = 0;
    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *option = ci->children + i;

        if (strcasecmp("type", option->key) == 0) {
            status = cf_util_get_match_metric_type(option, &csv_metric->type);
        } else if (strcasecmp("help", option->key) == 0) {
            status = cf_util_get_string(option, &csv_metric->help);
        } else if (strcasecmp("metric", option->key) == 0) {
            status = cf_util_get_string(option, &csv_metric->metric);
        } else if (strcasecmp("metric-from", option->key) == 0) {
            status = match_csv_config_get_index(option, &csv_metric->metric_from);
        } else if (strcasecmp("metric-prefix", option->key) == 0) {
            status = cf_util_get_string(option, &csv_metric->metric_prefix);
        } else if (strcasecmp("label", option->key) == 0) {
            status = cf_util_get_label(option, &csv_metric->labels);
        } else if (strcasecmp("label-from", option->key) == 0) {
            status = match_csv_config_append_label(&csv_metric->labels_from,
                                                   &csv_metric->labels_from_num, option);
        } else if (strcasecmp("value-from", option->key) == 0) {
            status = match_csv_config_get_index(option, &csv_metric->value_from);
        } else {
            PLUGIN_WARNING("Option `%s' not allowed here.", option->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    while(status == 0) {
        if ((csv_metric->metric == NULL) && (csv_metric->metric_from < 0)) {
            PLUGIN_WARNING("No 'metric' or 'metric-from' option specified ");
            status = -1;
            break;
        }

        if ((csv_metric->metric != NULL) && (csv_metric->metric_from > 0)) {
            PLUGIN_WARNING("Only one of 'metric' or 'metric-from' can be set ");
            status = -1;
            break;
        }

        if (csv_metric->value_from < 0) {
            PLUGIN_WARNING("Option 'value-from' must be set.");
            status = -1;
            break;
        }

        break;
    }

    if (status != 0)
        return -1;

    return 0;
}

static int match_csv_config_get_separator(config_item_t *ci, char *ret_char)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("The '%s' config option needs exactly one string argument.", ci->key);
        return -1;
    }

    size_t len_opt = strlen(ci->values[0].value.string);
    if (len_opt != 1) {
        PLUGIN_WARNING("The '%s' config option must be a single character", ci->key);
        return -1;
    }

    *ret_char = ci->values[0].value.string[0];
    return 0;
}

static int match_csv_config(const config_item_t *ci, void **user_data)
{
    *user_data = NULL;

    match_csv_t *csv = calloc(1, sizeof(*csv));
    if (csv == NULL)
        return -1;

    csv->field_separator = ',';
    csv->time_from = -1;

    int status = 0;
    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *option = ci->children + i;

        if (strcasecmp("metric-prefix", option->key) == 0) {
            status = cf_util_get_string(option, &csv->metric_prefix);
        } else if (strcasecmp("label", option->key) == 0) {
            status = cf_util_get_label(option, &csv->labels);
        } else if (strcasecmp("time-from", option->key) == 0) {
            status = match_csv_config_get_index(option, &csv->time_from);
        } else if (strcasecmp("field-separator", option->key) == 0) {
            status = match_csv_config_get_separator(option, &csv->field_separator);
        } else if (strcasecmp("metric", option->key) == 0) {
            status = match_csv_config_metric(option, csv);
        } else {
            PLUGIN_WARNING("Option '%s' not allowed here.", option->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        match_csv_destroy(csv);
        return -1;
    }

    *user_data = csv;

    return 0;
}

void module_register(void)
{
    plugin_match_proc_t proc = {0};

    proc.config = match_csv_config;
    proc.destroy = match_csv_destroy;
    proc.match = match_csv_match;

    plugin_register_match("csv", proc);
}
