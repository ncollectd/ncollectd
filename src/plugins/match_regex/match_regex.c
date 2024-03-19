// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2006-2014 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2009 Aman Gupta
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Aman Gupta <aman at tmm1.net>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/llist.h"
#include "libutils/time.h"

#include <regex.h>

typedef struct {
    char *key;
    int value_from;
} metric_label_from_t;

struct match_regex_metric_s {
    char *regex;
    char *excluderegex;

    char *metric;
    char *metric_prefix;
    ssize_t metric_from;
    match_metric_type_t type;
    char *help;

    label_set_t labels;

    metric_label_from_t *labels_from;
    size_t labels_from_num;

    ssize_t value_from;
    ssize_t time_from;

    regex_t cregex;
    regex_t cexcluderegex;

    struct match_regex_metric_s *next;
};

typedef struct match_regex_metric_s match_regex_metric_t;

typedef struct {
    char *metric_prefix;
    label_set_t labels;
    match_regex_metric_t *metrics;
} match_regex_t;

static cdtime_t match_regex_parse_time(char const *tbuf)
{
    double t;
    char *endptr = NULL;

    errno = 0;
    t = strtod(tbuf, &endptr);
    if ((errno != 0) || (endptr == NULL) || (endptr[0] != 0))
        return cdtime();

    return DOUBLE_TO_CDTIME_T(t);
}

static char *match_regex_substr(const char *str, regmatch_t *re_match)
{
    int begin = re_match->rm_so;
    int end = re_match->rm_eo;

    if ((begin < 0) || (end < 0) || (begin >= end))
        return NULL;
    if ((size_t)end > (strlen(str) + 1)) {
        PLUGIN_ERROR("'end' points after end of string.");
        return NULL;
    }

    size_t ret_len = end - begin;
    char *ret = malloc(ret_len + 1);
    if (ret == NULL) {
        PLUGIN_ERROR("malloc failed.");
        return NULL;
    }

    sstrncpy(ret, str + begin, ret_len + 1);
    return ret;
}

static void match_regex_metric_destroy(match_regex_metric_t *regex_metric)
{
    if (regex_metric == NULL)
        return;

    regfree(&regex_metric->cregex);
    if (regex_metric->excluderegex != NULL)
        regfree(&regex_metric->cexcluderegex);

    free(regex_metric->regex);
    free(regex_metric->excluderegex);
    free(regex_metric->metric);
    free(regex_metric->metric_prefix);
    free(regex_metric->help);
    label_set_reset(&regex_metric->labels);
    free(regex_metric);
}

static void match_regex_destroy(void *user_data)
{
    match_regex_t *regex = user_data;
    if (regex == NULL)
        return;

    match_regex_metric_t *regex_metric = regex->metrics;
    while (regex_metric != NULL) {
        match_regex_metric_t *next = regex_metric->next;
        match_regex_metric_destroy(regex_metric);
        regex_metric = next;
    }

    free(regex->metric_prefix);
    label_set_reset(&regex->labels);

    free(regex);
}


static int match_regex_match(match_metric_family_set_t *set, char *buffer, void *user_data)
{
    match_regex_t *regex = user_data;
    if (user_data == NULL)
        return -1;

    match_regex_metric_t *regex_metric = regex->metrics;
    while (regex_metric != NULL) {
        regmatch_t re_match[32];

        if (regex_metric->excluderegex != NULL) {
            int status = regexec(&regex_metric->cexcluderegex,
                                 buffer, STATIC_ARRAY_SIZE(re_match), re_match, 0);
            if (status == 0)
                continue;
        }

        int status = regexec(&regex_metric->cregex,
                             buffer, STATIC_ARRAY_SIZE(re_match), re_match, 0);
        if (status != 0)
            continue;

        ssize_t total_matches = 0;
        int buffer_len = strlen(buffer);
        for (size_t re_matches = 0; re_matches < STATIC_ARRAY_SIZE(re_match); re_matches++) {
            if ((re_match[re_matches].rm_so < 0) || (re_match[re_matches].rm_eo < 0))
                break;
            if (re_match[re_matches].rm_so >= re_match[re_matches].rm_eo)
                break;
            if (re_match[re_matches].rm_eo > buffer_len + 1)
                break;
            total_matches++;
        }

        if (regex_metric->value_from >= total_matches)
            continue;
        if ((regex_metric->metric_from >= 0) && (regex_metric->metric_from >= total_matches))
            continue;
        if (regex_metric->labels_from_num > 0) {
            for (size_t i = 0; i < regex_metric->labels_from_num; ++i) {
                if (regex_metric->labels_from[i].value_from  >= total_matches)
                    continue;
            }
        }

        cdtime_t t = 0;
        if (regex_metric->time_from >= 0) {
            if (regex_metric->time_from >= total_matches)
                continue;
            char *lval = match_regex_substr(buffer, &re_match[regex_metric->time_from]);
            if (lval != NULL) {
                t = match_regex_parse_time(lval);
                free(lval);
            }
        }

        strbuf_t buf = STRBUF_CREATE;

        if (regex->metric_prefix != NULL)
            strbuf_print(&buf, regex->metric_prefix);
        if (regex_metric->metric_prefix != NULL)
            strbuf_print(&buf, regex_metric->metric_prefix);

        if (regex_metric->metric_from >= 0) {
            size_t len = re_match[regex_metric->metric_from].rm_eo -
                         re_match[regex_metric->metric_from].rm_so;
            strbuf_putstrn(&buf, buffer+re_match[regex_metric->metric_from].rm_so, len);
        } else {
            strbuf_print(&buf, regex_metric->metric);
        }

        label_set_t mlabel = {0};

        for (size_t i = 0; i < regex->labels.num; i++) {
            label_set_add(&mlabel, true, regex->labels.ptr[i].name, regex->labels.ptr[i].value);
        }

        for (size_t i = 0; i < regex_metric->labels.num; i++) {
            label_set_add(&mlabel, true, regex_metric->labels.ptr[i].name,
                                         regex_metric->labels.ptr[i].value);
        }

        if (regex_metric->labels_from_num > 0) {
            for (size_t i = 0; i < regex_metric->labels_from_num; ++i) {
                char *lval = match_regex_substr(buffer,
                                                &re_match[regex_metric->labels_from[i].value_from]);
                if (lval != NULL) {
                    label_set_add(&mlabel, true, regex_metric->labels_from[i].key, lval);
                    free(lval);
                }
            }
        }

        char *lval = match_regex_substr(buffer, &re_match[regex_metric->time_from]);
        if (lval != NULL) {
            plugin_match_metric_family_set_add(set, buf.ptr, regex_metric->help, NULL,
                                               regex_metric->type, &mlabel, lval, t);
            free(lval);
        }

        label_set_reset(&mlabel);
        strbuf_destroy(&buf);

        regex_metric = regex_metric->next;
    }

    return 0;
}

static int match_regex_config_get_index(config_item_t *ci, ssize_t *ret_index)
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

static int match_regex_config_append_label(metric_label_from_t **var, size_t *len,
                                           config_item_t *ci)
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
  match regex {
      metric-prefix
      label
      metric {
          regex "\\<sshd[^:]*: Invalid user [^ ]+ from\\>"
          type counter inc
      }
  }
*/

static int match_regex_config_metric(const config_item_t *ci, match_regex_t *regex)
{
    match_regex_metric_t *regex_metric = calloc(1, sizeof(*regex_metric));
    if (regex_metric == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    if (regex->metrics == NULL) {
        regex->metrics = regex_metric;
    } else {
        match_regex_metric_t *ptr = regex->metrics;
        while (ptr->next != NULL)
            ptr = ptr->next;
        ptr->next = regex_metric;
    }

    regex_metric->metric_from = -1;
    regex_metric->value_from = -1;
    regex_metric->time_from = -1;

    int status = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("regex", child->key) == 0) {
            status = cf_util_get_string(child, &regex_metric->regex);
        } else if (strcasecmp("exclude-regex", child->key) == 0) {
            status = cf_util_get_string(child, &regex_metric->excluderegex);
        } else if (strcasecmp("type", child->key) == 0) {
            status = cf_util_get_match_metric_type(child, &regex_metric->type);
        } else if (strcasecmp("metric", child->key) == 0) {
            status = cf_util_get_string(child, &regex_metric->metric);
        } else if (strcasecmp("metric-prefix", child->key) == 0) {
            status = cf_util_get_string(child, &regex_metric->metric_prefix);
        } else if (strcasecmp("help", child->key) == 0) {
            status = cf_util_get_string(child, &regex_metric->help);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &regex_metric->labels);
        } else if (strcasecmp("metric-from", child->key) == 0) {
            status = match_regex_config_get_index(child, &regex_metric->metric_from);
        } else if (strcasecmp("label-from", child->key) == 0) {
            status = match_regex_config_append_label(&regex_metric->labels_from,
                                                     &regex_metric->labels_from_num, child);
        } else if (strcasecmp("value-from", child->key) == 0) {
            status = match_regex_config_get_index(child, &regex_metric->value_from);
        } else if (strcasecmp("time-from", child->key) == 0) {
            status = match_regex_config_get_index(child, &regex_metric->time_from);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0)
        return -1;

    if (regex_metric->regex == NULL) {
        PLUGIN_ERROR("'regex' missing in 'metric' block.");
        return -1;
    }

    if ((regex_metric->metric == NULL) || (regex_metric->metric_from < 0)) {
        PLUGIN_ERROR("'metric' or 'metric-from' missing in 'metric' block.");
        return -1;
    }

    if (regex_metric->type == 0) {
        PLUGIN_ERROR("'type' missing in 'metric' block.");
        return -1;
    }

    if (regex_metric->value_from < 0) {
        PLUGIN_ERROR("'value-from' missing in 'metric' block.");
        return -1;
    }

    status = regcomp(&regex_metric->cregex, regex_metric->regex, REG_EXTENDED | REG_NEWLINE);
    if (status != 0) {
        PLUGIN_ERROR("Compiling the regular expression '%s' failed.", regex_metric->regex);
        return -1;
    }

    if (regex_metric->excluderegex != NULL) {
        status = regcomp(&regex_metric->cexcluderegex, regex_metric->excluderegex, REG_EXTENDED);
        if (status != 0) {
            PLUGIN_ERROR("Compiling the excluding regular expression '%s' failed.",
                         regex_metric->excluderegex);
            return -1;
        }
    }

    return 0;
}

static int match_regex_config(const config_item_t *ci, void **user_data)
{
    *user_data = NULL;

    match_regex_t *regex = calloc(1, sizeof(*regex));
    if (regex == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = 0;
    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *option = ci->children + i;

        if (strcasecmp("metric-prefix", option->key) == 0) {
            status = cf_util_get_string(option, &regex->metric_prefix);
        } else if (strcasecmp("label", option->key) == 0) {
            status = cf_util_get_label(option, &regex->labels);
        } else if (strcasecmp("metric", option->key) == 0) {
            status = match_regex_config_metric(option, regex);
        } else {
            PLUGIN_ERROR("The configuration option '%s' in %s:%d is not allowed here.",
                         option->key, cf_get_file(option), cf_get_lineno(option));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        match_regex_destroy(regex);
        return -1;
    }

    *user_data = regex;

    return 0;
}

void module_register(void)
{
    plugin_match_proc_t proc = {0};

    proc.config = match_regex_config;
    proc.destroy = match_regex_destroy;
    proc.match = match_regex_match;

    plugin_register_match("regex", proc);
}
