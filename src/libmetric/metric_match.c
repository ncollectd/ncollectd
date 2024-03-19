// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "libutils/strbuf.h"
#include "libmetric/metric.h"
#include "libmetric/metric_chars.h"
#include "libmetric/metric_match.h"

#include <regex.h>

void metric_match_pair_free(metric_match_pair_t *pair)
{
    if (pair == NULL)
        return;

    free(pair->name);
    switch (pair->op) {
        case METRIC_MATCH_OP_NONE:
            break;
        case METRIC_MATCH_OP_EQL:
        case METRIC_MATCH_OP_NEQ:
            free(pair->value.string);
            break;
        case METRIC_MATCH_OP_EQL_REGEX:
        case METRIC_MATCH_OP_NEQ_REGEX:
            regfree(pair->value.regex);
            free(pair->value.regex);
            break;
        case METRIC_MATCH_OP_EXISTS:
            break;
        case METRIC_MATCH_OP_NEXISTS:
            break;
    }

    free(pair);
}

metric_match_pair_t *metric_match_pair_alloc(const char *name, metric_match_op_t op, char *value)
{
    if (name == NULL)
        return NULL;

    metric_match_pair_t *pair = calloc(1, sizeof(*pair));
    if (pair == NULL)
        return NULL;

    pair->name = strdup(name);
    pair->op = op;

    switch(op) {
    case METRIC_MATCH_OP_NONE:
        metric_match_pair_free(pair);
        return NULL;
        break;
    case METRIC_MATCH_OP_EQL:
    case METRIC_MATCH_OP_NEQ:
        if ((value == NULL) || (strlen(value) == 0)) {
            metric_match_pair_free(pair);
            return NULL;
        }

        pair->value.string = strdup(value);
        if (pair->value.string == NULL) {
            metric_match_pair_free(pair);
            return NULL;
        }
        break;
    case METRIC_MATCH_OP_EQL_REGEX:
    case METRIC_MATCH_OP_NEQ_REGEX:
        pair->value.regex = calloc(1, sizeof(*(pair->value.regex)));
        if (pair->value.regex == NULL) {
            metric_match_pair_free(pair);
            return NULL;
        }
        int status = regcomp(pair->value.regex, value, REG_EXTENDED|REG_NOSUB);
        if (status != 0) {
            metric_match_pair_free(pair);
            return NULL;
        }
        break;
    case METRIC_MATCH_OP_EXISTS:

        break;
    case METRIC_MATCH_OP_NEXISTS:
        break;
    }

    return pair;
}

void metric_match_set_free(metric_match_set_t *match)
{
    if (match == NULL)
        return;

    for (size_t i = 0; i < match->num; i++) {
        metric_match_pair_free(match->ptr[i]);
    }
    free(match->ptr);
    free(match);
}

metric_match_set_t *metric_match_set_alloc(void)
{
    metric_match_set_t *match = calloc(1, sizeof(*match));
    if (match == NULL)
        return NULL;
    return match;
}

int metric_match_set_append(metric_match_set_t *match, metric_match_pair_t *pair)
{
    if ((match == NULL) || (pair == NULL))
        return EINVAL;

    metric_match_pair_t **tmp = realloc(match->ptr, sizeof(*tmp)*(match->num + 1));
    if (tmp == NULL)
        return ENOMEM;

    match->ptr = tmp;
    match->ptr[match->num] = pair;
    match->num++;
    return 0;
}

int metric_match_set_add(metric_match_set_t *match, const char *name,
                         metric_match_op_t op, char *value)
{
    if ((match == NULL) || (name == NULL))
        return EINVAL;

    metric_match_pair_t *pair = metric_match_pair_alloc(name, op, value);
    if (pair == NULL)
        return EINVAL;

    int status = metric_match_set_append(match, pair);
    if (status != 0) {
        metric_match_pair_free(pair);
        return status;
    }
    return 0;
}

int metric_match_add(metric_match_t *match, const char *name, metric_match_op_t op, char *value)
{
    if ((match == NULL) || (name == NULL))
        return EINVAL;

    if (strcmp("__name__", name) == 0) {
        if (match->name == NULL) {
            match->name = metric_match_set_alloc();
            if (match->name == NULL)
                return -1;
        }
        return metric_match_set_add(match->name, name, op, value);
    }

    if (match->labels == NULL) {
        match->labels = metric_match_set_alloc();
        if (match->labels == NULL)
            return -1;
    }

    return metric_match_set_add(match->labels, name, op, value);
}

void metric_match_reset(metric_match_t *match)
{
    if (match == NULL)
        return;

    if (match->name != NULL) {
        metric_match_set_free(match->name);
        match->name = NULL;
    }

    if (match->labels != NULL) {
        metric_match_set_free(match->labels);
        match->labels = NULL;
    }
}

static int parse_label_value(strbuf_t *buf, const char **inout)
{
    const char *ptr = *inout;

    if (ptr[0] != '"')
        return EINVAL;
    ptr++;

    while (ptr[0] != '"') {
        size_t valid_len = strcspn(ptr, "\\\"\n");
        if (valid_len != 0) {
            strbuf_printn(buf, ptr, valid_len);
            ptr += valid_len;
            continue;
        }

        if ((ptr[0] == 0) || (ptr[0] == '\n')) {
            return EINVAL;
        }

        assert(ptr[0] == '\\');
        if (ptr[1] == 0)
            return EINVAL;

        char tmp[2] = {ptr[1], 0};
        if (tmp[0] == 'n') {
            tmp[0] = '\n';
        } else if (tmp[0] == 'r') {
            tmp[0] = '\r';
        } else if (tmp[0] == 't') {
            tmp[0] = '\t';
        }

        strbuf_print(buf, tmp);

        ptr += 2;
    }

    assert(ptr[0] == '"');
    ptr++;
    *inout = ptr;
    return 0;
}

int metric_match_unmarshal(metric_match_t *match, char const *str)
{
    int ret = 0;
    char const *ptr = str;

    size_t name_len = metric_valid_len(ptr);
    if (name_len != 0) {
        char name[name_len + 1];
        strncpy(name, ptr, name_len);
        name[name_len] = 0;
        ptr += name_len;

        if (match->name == NULL) {
            match->name = metric_match_set_alloc();
            if (match->name == NULL)
                return -1;
        }

        int status = metric_match_set_add(match->name, "__name__", METRIC_MATCH_OP_EQL, name);
        if (status != 0)
            return status;

        /* metric name without labels */
        if ((ptr[0] == '\0') || (ptr[0] == ' '))
            return 0;
    }

    if (ptr[0] != '{')
        return EINVAL;

    strbuf_t value = STRBUF_CREATE;
    while ((ptr[0] == '{') || (ptr[0] == ',')) {
        ptr++;

        size_t key_len = label_valid_name_len(ptr);
        if (key_len == 0) {
            ret = EINVAL;
            break;
        }

        char key[key_len + 1];
        strncpy(key, ptr, key_len);
        key[key_len] = 0;
        ptr += key_len;

        metric_match_op_t op;
        if (ptr[0] == '=') {
            op = METRIC_MATCH_OP_EQL;
            ptr++;
            if (ptr[0] == '~') {
                op = METRIC_MATCH_OP_EQL_REGEX;
                ptr++;
            }
        } else if (ptr[0] == '!') {
            ptr++;
            if (ptr[0] == '~') {
                op = METRIC_MATCH_OP_NEQ_REGEX;
                ptr++;
            } else if (ptr[0] == '=') {
                op = METRIC_MATCH_OP_NEQ;
                ptr++;
            } else {
                ret = EINVAL;
                break;
            }
        } else {
            ret = EINVAL;
            break;
        }

        strbuf_reset(&value);
        int status = parse_label_value(&value, &ptr);
        if (status != 0) {
            ret = status;
            break;
        }

        if (value.ptr == NULL) {
            ret = -1;
            break;
        }

        if (value.ptr[0] == '\0') {
            if (op == METRIC_MATCH_OP_EQL) {
                op = METRIC_MATCH_OP_NEXISTS;
            } else if (op == METRIC_MATCH_OP_NEQ) {
                op = METRIC_MATCH_OP_EXISTS;
            }
        }

        if ((key_len == strlen("__name__")) && !strcmp("__name__", key)) {
            if (match->name == NULL) {
                match->name = metric_match_set_alloc();
                if (match->name == NULL) {
                    ret = -1;
                    break;
                }
            }
            status = metric_match_set_add(match->name, key, op, value.ptr);
            if (status != 0) {
                ret = status;
                break;
            }
        } else {
            if (match->labels == NULL) {
                match->labels = metric_match_set_alloc();
                if (match->labels == NULL) {
                    ret = -1;
                    break;
                }
            }
            status = metric_match_set_add(match->labels, key, op, value.ptr);
            if (status != 0) {
                ret = status;
                break;
            }
        }
    }
    strbuf_destroy(&value);

    if (ret != 0)
        return ret;

    if (ptr[0] != '}')
        return EINVAL;

    return 0;
}

static inline bool metric_match_value_cmp(metric_match_value_t value, metric_match_op_t op,
                                          char const *name)
{
    switch (op) {
        case METRIC_MATCH_OP_NONE:
            break;
        case METRIC_MATCH_OP_EQL:
            return strcmp(name, value.string) == 0;
            break;
        case METRIC_MATCH_OP_NEQ:
            return strcmp(name, value.string) != 0;
            break;
        case METRIC_MATCH_OP_EQL_REGEX:
            return regexec(value.regex, name, 0, NULL, 0) == 0;
            break;
        case METRIC_MATCH_OP_NEQ_REGEX:
            return regexec(value.regex, name, 0, NULL, 0) != 0;
            break;
        case METRIC_MATCH_OP_EXISTS:
            break;
        case METRIC_MATCH_OP_NEXISTS:
            break;
    }
    return false;
}

static inline bool metric_match_labels_cmp(metric_match_set_t *match_set, const label_set_t *labels)
{
    for (size_t i = 0; i < match_set->num; i++) {
        metric_match_pair_t *match_pair = match_set->ptr[i];
        if (match_pair != NULL) {
            label_pair_t *pair = label_set_read(*labels, match_pair->name);
            if (match_pair->op == METRIC_MATCH_OP_EXISTS) {
                if (pair == NULL)
                    return false;
            } else if (match_pair->op == METRIC_MATCH_OP_NEXISTS) {
                if (pair != NULL)
                    return false;
            } else {
                if (pair == NULL)
                    return false;
                if (metric_match_value_cmp(match_pair->value, match_pair->op,
                                           pair->value) == false)
                    return false;
            }
        }
    }

    return true;
}

static inline bool metric_match_name_cmp(metric_match_set_t *match_set, const char *name)
{
    for (size_t i = 0; i < match_set->num; i++) {
        metric_match_pair_t *match_pair = match_set->ptr[i];
        if (match_pair != NULL) {
            if (match_pair->op == METRIC_MATCH_OP_EXISTS) {
                /* always have name */
            } else if (match_pair->op == METRIC_MATCH_OP_NEXISTS) {
                return false;
            } else if (metric_match_value_cmp(match_pair->value,
                                              match_pair->op, name) == false) {
                return false;
            }
        }
    }

    return true;
}

bool metric_match_cmp(metric_match_t *match, const char *name, const label_set_t *labels)
{
    if (match == NULL)
        return false;

    if (name != NULL) {
        if (match->name != NULL) {
            if (metric_match_name_cmp(match->name, name) == false)
                return false;
        }
    }

    if (labels != NULL) {
        if (match->labels != NULL) {
            if (metric_match_labels_cmp(match->labels, labels) == false)
                return false;
        }
    }

    return true;
}
