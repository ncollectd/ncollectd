// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2019-2020 Google LLC
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manoj Srivastava <srivasta at google.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "log.h"
#include "libutils/common.h"
#include "libmetric/label_set.h"
#include "libmetric/metric_chars.h"

static int label_pair_compare(void const *a, void const *b)
{
    return strcmp(((label_pair_t const *)a)->name, ((label_pair_t const *)b)->name);
}

label_pair_t *label_set_read(label_set_t labels, char const *name)
{
    if (name == NULL) {
        errno = EINVAL;
        return NULL;
    }

    struct {
        char const *name;
        char const *value;
    } label = {
        .name = name,
    };

    label_pair_t *ret = bsearch(&label, labels.ptr, labels.num,
                                sizeof(*labels.ptr), label_pair_compare);
    if (ret == NULL) {
        errno = ENOENT;
        return NULL;
    }

    return ret;
}

char *label_ndup_value_unescape(const char *value, size_t nv)
{
    if (value == NULL)
        return NULL;

    char *new_value = malloc(nv + 1);
    if (new_value == NULL) {
        ERROR("malloc failed.");
        return NULL;
    }

    char *str = new_value;
    const char *end = value + nv;
    while(value < end) {
        if (*value == '\0')
            break;

        if (*value == '\\') {
            value++;
            if (value >= end)
                break;
            switch (*value) {
            case '\0':
                *str = '\0';
                return new_value;
                break;
            case '\\':
                *str = '\\';
                break;
            case '\"':
                *str = '\"';
                break;
            case 'n':
                *str = '\n';
                break;
            case 'r':
                *str = '\r';
                break;
            case 't':
                *str = '\t';
                break;
            default:
                *str = *value;
                break;
            }
            str++;
        } else {
            *str = *value;
            str++;
        }
        value++;
    }

    *str = '\0';
    return new_value;
}

int _label_set_add(label_set_t *labels, bool overwrite, bool unescape,
                    const char *name, size_t sn, const char *value, size_t sv)
{
    if ((labels == NULL) || (name == NULL))
        return EINVAL;

    size_t lower = 0;
    size_t upper = labels->num;
    while (lower < upper) {
        size_t idx = (lower + upper) / 2;

        int cmp = sstrncmp(name, sn, labels->ptr[idx].name, strlen(labels->ptr[idx].name));
        if (cmp < 0) {
            upper = idx;
        } else if (cmp > 0) {
            lower = idx + 1;
        } else {
            if ((value == NULL) || (value[0] == '\0')) {
                free(labels->ptr[idx].name);
                free(labels->ptr[idx].value);
                if (idx != (labels->num - 1))
                    memmove(labels->ptr + idx, labels->ptr + (idx + 1),
                            sizeof(*labels->ptr) * (labels->num - (idx + 1)));
                labels->num--;
                if (labels->num == 0) {
                    free(labels->ptr);
                    labels->ptr = NULL;
                    return 0;
                }

                label_pair_t *tmp = realloc(labels->ptr, sizeof(*labels->ptr) * labels->num);
                if (tmp == NULL) {
                    ERROR("realloc failed.");
                    return -1;
                }
                labels->ptr = tmp;

                return 0;
            }
            if (overwrite) {
                char *new_value = NULL;
                if (unescape)
                    new_value = label_ndup_value_unescape(value, sv);
                else
                    new_value = sstrndup(value, sv);
                if (new_value == NULL) {
                    ERROR("strdup failed.");
                    return -1;
                }
                free(labels->ptr[idx].value);
                labels->ptr[idx].value = new_value;
            }
            return 0;
        }
    }

    size_t idx = lower;

    if ((value == NULL) || (value[0] == '\0'))
        return 0;

    if (!label_check_name(name, sn))
        return EINVAL;

    errno = 0;
    label_pair_t *tmp = realloc(labels->ptr, sizeof(labels->ptr[0]) * (labels->num + 1));
    if (tmp == NULL) {
        ERROR("realloc failed.");
        return -1;
    }
    labels->ptr = tmp;

    char *lname = sstrndup(name, sn);

    char *lvalue = NULL;
    if (unescape)
        lvalue = label_ndup_value_unescape(value, sv);
    else
        lvalue = sstrndup(value, sv);

    if ((lname == NULL) || (lvalue == NULL)) {
        ERROR("strdup failed.");
        free(lname);
        free(lvalue);
        return -ENOMEM;
    }

    labels->num++;

    if (idx != (labels->num - 1))
        memmove(labels->ptr + (idx + 1), labels->ptr + idx,
                sizeof(*labels->ptr) * (labels->num - (idx + 1)));

    labels->ptr[idx].name = lname;
    labels->ptr[idx].value = lvalue;

    return 0;
}

int label_set_add_set(label_set_t *labels, bool overwrite, label_set_t set)
{
    if ((labels == NULL) || (set.num == 0))
        return EINVAL;

    for (size_t i = 0; i < set.num; i++) {
        char const *name = set.ptr[i].name;
        char const *value = set.ptr[i].value;

        label_set_add(labels, overwrite, name, value);
    }
    return 0;
}

int label_set_rename(label_set_t *labels, char *from, char *to)
{
    label_pair_t *pair_to = label_set_read(*labels, to);
    if (pair_to != NULL)
        return EEXIST;

    label_pair_t *pair_from = label_set_read(*labels, from);
    if (pair_from == NULL)
        return ENOENT;

    char *new_name = strdup(to);
    if (new_name == NULL)
        return ENOMEM;

    free(pair_from->name);
    pair_from->name = new_name;

    qsort(labels->ptr, labels->num, sizeof(*labels->ptr), label_pair_compare);
    return 0;
}

void label_set_reset(label_set_t *labels)
{
    if (labels == NULL)
        return;

    if (labels->ptr != NULL) {
        for (size_t i = 0; i < labels->num; i++) {
            free(labels->ptr[i].name);
            free(labels->ptr[i].value);
        }
        free(labels->ptr);
        labels->ptr = NULL;
    }

    labels->num = 0;
}

 //__attribute__((stack_protect))
int label_set_clone(label_set_t *dest, label_set_t src)
{
    if (src.num == 0)
        return 0;

    if (dest->ptr != NULL)
        label_set_reset(dest);

    dest->ptr = calloc(src.num, sizeof(dest->ptr[0]));
    if (dest->ptr == NULL)
        return ENOMEM;

    dest->num = src.num;

    size_t n = 0;
    for (size_t i = 0; i < src.num; i++) {
        if ((src.ptr[i].name == NULL) || (src.ptr[i].name[0] == '\0'))
            continue;
        if ((src.ptr[i].value == NULL) || (src.ptr[i].value[0] == '\0'))
            continue;
        dest->ptr[n].name = strdup(src.ptr[i].name);
        dest->ptr[n].value = strdup(src.ptr[i].value);
        if ((dest->ptr[n].name == NULL) || (dest->ptr[n].value == NULL)) {
            ERROR("strdup failed.");
            /* make gcc analyzer happy */
            if (dest->ptr[n].name != NULL) {
                free(dest->ptr[n].name);
                dest->ptr[n].name = NULL;
            }
            if (dest->ptr[n].value != NULL) {
                free(dest->ptr[n].value);
                dest->ptr[n].value = NULL;
            }
            label_set_reset(dest);
            return ENOMEM;
        }
        n++;
    }

    dest->num = n;

    qsort(dest->ptr, dest->num, sizeof(*dest->ptr), label_pair_compare);

    return 0;
}

size_t label_set_strlen(label_set_t *labels)
{
    size_t len = 0;

    for (size_t i = 0; i < labels->num ; i++) {
        len += strlen(labels->ptr[i].name) + 2 + strlen(labels->ptr[i].value) + 1;
        const char *c = labels->ptr[i].value;
        while (*c != '\0') {
            switch(*c) {
            case '"':
                len++;
                break;
            case '\\':
                len++;
                break;
            case '\n':
                len++;
                break;
            case '\r':
                len++;
                break;
            case '\t':
                len++;
                break;
            }
            c++;
        }
    }

    if (labels->num > 0)
        len += labels->num - 1;

    return  len;
}

int label_set_cmp(label_set_t *l1, label_set_t *l2)
{
    if (l1->num < l2->num) {
        return -1;
    }

    if (l1->num > l2->num) {
        return 1;
    }

    int cmp = 0;
    for (size_t i = 0; i < l1->num ; i++) {
        cmp = strcmp(l1->ptr[i].name, l2->ptr[i].name);
        if (cmp != 0)
            return cmp;

        cmp = strcmp(l1->ptr[i].value, l2->ptr[i].value);
        if (cmp != 0)
            return cmp;
    }

    return 0;
}

int label_set_qsort(label_set_t *labels)
{
    qsort(labels->ptr, labels->num, sizeof(*labels->ptr), label_pair_compare);
    return 0;
}
