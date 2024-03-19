/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include <stdlib.h>

typedef uint32_t metric_id_t;

typedef struct {
    uint32_t alloc;
    uint32_t num;
    metric_id_t *ptr;
} metric_id_set_t;

typedef struct {
    uint32_t alloc;
    uint32_t num;
    metric_id_set_t **ptr;
} metric_id_set_list_t;

static inline uint32_t metric_id_size(metric_id_set_t *set)
{
    return set->num;
}

static inline uint32_t metric_id_set_avail(metric_id_set_t *set)
{
    return set->alloc ? set->num - set->alloc : 0;
}

int metric_id_set_resize(metric_id_set_t *set, uint32_t need);

int metric_id_set_insert(metric_id_set_t *set, metric_id_t id);

int metric_id_set_cmp(void const *a, void const *b);

static inline bool metric_id_set_search(metric_id_set_t *set, metric_id_t id)
{
    metric_id_t *ret = bsearch(&id, set->ptr, set->num, sizeof(*set->ptr), metric_id_set_cmp);
    return ret != NULL ? true : false;
}

static inline int metric_id_set_append(metric_id_set_t *set, metric_id_t id)
{
    if (metric_id_set_avail(set) < 1) {
        if (metric_id_set_resize(set, 1) < 0)
            return ENOMEM;
    }
    set->ptr[set->num] = id;
    set->num++;
    return 0;
}

static inline void metric_id_set_sort(metric_id_set_t *set)
{
    qsort(set->ptr, set->num, sizeof(*set->ptr), metric_id_set_cmp);
}

static inline void metric_id_set_swap(metric_id_set_t *seta, metric_id_set_t *setb)
{
    uint32_t alloc = seta->alloc;
    seta->alloc = setb->alloc;
    setb->alloc = alloc;

    uint32_t num = seta->num;
    seta->num = setb->num;
    setb->num = num;

    metric_id_t *ptr = seta->ptr;
    seta->ptr = setb->ptr;
    setb->ptr = ptr;
}

int metric_id_set_add(metric_id_set_t *set, metric_id_t id);

int metric_id_set_clone(metric_id_set_t *dst, metric_id_set_t *src);

int metric_id_set_union(metric_id_set_t *dst, metric_id_set_t *a, metric_id_set_t *b);

int metric_id_set_intersect(metric_id_set_t *dst, metric_id_set_t *a, metric_id_set_t *b);

int metric_id_set_difference(metric_id_set_t *dst, metric_id_set_t *a, metric_id_set_t *b);

static inline void metric_id_set_reset(metric_id_set_t *set)
{
    if (set == NULL)
        return;
    set->num = 0;
}

static inline void metric_id_set_destroy(metric_id_set_t *set)
{
    if (set == NULL)
        return;

    free(set->ptr);
    set->ptr = NULL;
    set->num = 0;
    set->alloc = 0;
}

static inline int metric_id_set_list_avail(metric_id_set_list_t *list)
{
    return list->alloc ? list->num - list->alloc : 0;
}

int metric_id_set_list_add(metric_id_set_list_t *list, metric_id_set_t *set);

int metric_id_set_list_reset(metric_id_set_list_t *list);

int metric_id_set_list_union(metric_id_set_t *dst, metric_id_set_list_t *list);

int metric_id_set_list_intersect(metric_id_set_t *dst, metric_id_set_list_t *list);
