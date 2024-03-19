// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "libmdb/metric_id.h"

int metric_id_set_cmp(void const *a, void const *b)
{
    metric_id_t const *ida = a;
    metric_id_t const *idb = b;

    if (ida < idb)
        return -1;
    else if (ida > idb)
        return 1;
    else
        return 0;
}

int metric_id_set_resize(metric_id_set_t *set, uint32_t need)
{
    if (need < 8)
        need = 8;
    uint32_t new_size = set->num + need;

    metric_id_t *new_ptr = realloc(set->ptr, sizeof(*set->ptr) * new_size);
    if (new_ptr == NULL)
        return ENOMEM;

    set->ptr = new_ptr;
    set->alloc = new_size;

    return 0;
}

int metric_id_set_insert(metric_id_set_t *set, metric_id_t id)
{
    if (set == NULL)
        return EINVAL;

    if (metric_id_set_avail(set) < 1) {
        if (metric_id_set_resize(set, 1) != 0)
            return ENOMEM;
    }

    if (set->num == 0) {
        set->ptr[set->num++] = id;
        return 0;
    }

    if (id > set->ptr[set->num-1]) {
        set->ptr[set->num++] = id;
        return 0;
    }

    uint32_t idx = 0;
    for (idx = 0; idx < set->num; idx++) {
        if (set->ptr[idx] >= id)
            break;
    }

    if (set->ptr[idx] == id)
        return 0;

    memmove(set->ptr + idx + 1, set->ptr + idx, sizeof(*set->ptr) * (set->num - idx));
    set->ptr[idx] = id;
    set->num++;

    return 0;
}

int metric_id_set_clone(metric_id_set_t *dst, metric_id_set_t *src)
{
    if ((dst == NULL) || (src == NULL))
        return EINVAL;

    if (src->num == 0)
        return 0;

    if (metric_id_set_avail(dst) < src->num) {
        if (metric_id_set_resize(dst, src->num) != 0)
            return ENOMEM;
    }

    memcpy(dst->ptr, src->ptr, sizeof(*src->ptr) * src->num);
    dst->num = src->num;

    return 0;
}

int metric_id_set_union(metric_id_set_t *dst, metric_id_set_t *a, metric_id_set_t *b)
{
    uint32_t num = a->num + b->num;

    if (metric_id_set_avail(dst) < num) {
        if (metric_id_set_resize(dst, num) != 0)
            return ENOMEM;
    }

    uint32_t i = 0, j = 0, n = 0;
    while ((i < a->num) && (j < b->num)) {
        if (a->ptr[i] < b->ptr[j]) {
            dst->ptr[n++] = a->ptr[i++];
        } else if (b->ptr[j] < a->ptr[i]) {
            dst->ptr[n++] = b->ptr[j++];
        } else { /* if a->ptr[i] == b->ptr[j] */
            dst->ptr[n++] = b->ptr[j++];
            i++;
        }
    }
    while (i < a->num) {
        memcpy(dst->ptr + n, a->ptr + i, sizeof(*dst->ptr) * (a->num - i));
        n += a->num - i;
        // dst->ptr[n++] = a->ptr[i++];
    }
    while (j < b->num) {
        memcpy(dst->ptr + n, b->ptr + j, sizeof(*dst->ptr) * (b->num - j));
        n += b->num - j;
        // dst->ptr[n++] = b->ptr[j++];
    }

    dst->num = n;
    return 0;
}

int metric_id_set_intersect(metric_id_set_t *dst, metric_id_set_t *a, metric_id_set_t *b)
{
    uint32_t num = a->num < b->num ? a->num : b->num;

    if (metric_id_set_avail(dst) < num) {
        if (metric_id_set_resize(dst, num) != 0)
            return ENOMEM;
    }

    uint32_t i = 0, j = 0, n = 0;
    while ((i < a->num) && (j < b->num)) {
        if (a->ptr[i] < b->ptr[j]) {
            i++;
        } else if (b->ptr[j] < a->ptr[i]) {
            j++;
        } else { /* a->ptr[i] == b->ptr[j] */
            dst->ptr[n++] = b->ptr[j++];
            i++;
        }
    }

    dst->num = n;
    return 0;
}

int metric_id_set_difference(metric_id_set_t *dst, metric_id_set_t *a, metric_id_set_t *b)
{
    uint32_t num = a->num;
    if (metric_id_set_avail(dst) < num) {
        if (metric_id_set_resize(dst, num) != 0)
            return ENOMEM;
    }

    uint32_t i = 0, j = 0, n = 0;
    while ((i < a->num) && (j < b->num)) {
        if (a->ptr[i] < b->ptr[j]) {
            dst->ptr[n++] = a->ptr[i++];
        } else if (b->ptr[j] < a->ptr[i]) {
            j++;
        } else { /* if arr1[i] == arr2[j] */
            i++;
        }
    }
    while (i < a->num) {
        memcpy(dst->ptr + n, a->ptr + i, sizeof(*dst->ptr) * (a->num - i));
        n += a->num - i;
        // dst->ptr[n++] = a->ptr[i++];
    }

    dst->num = n;
    return 0;
}

static int metric_id_set_list_cmp(void const *a, void const *b)
{
    metric_id_set_t const *seta = a;
    metric_id_set_t const *setb = b;

    if (seta->num < setb->num)
        return -1;
    else if (seta->num > setb->num)
        return 1;
    else
        return 0;
}

int metric_id_set_list_resize(metric_id_set_list_t *list, uint32_t need)
{
    uint32_t new_size = list->num + need;
    if (new_size == 1) {
        new_size = 4;
    } else {
        new_size = 1 << (64 - __builtin_clzl(new_size - 1));
    }

    metric_id_set_t **new_ptr = realloc(list->ptr, sizeof(*list->ptr) * new_size);
    if (new_ptr == NULL)
        return ENOMEM;

    list->ptr = new_ptr;
    list->alloc = new_size;

    return 0;
}

int metric_id_set_list_add(metric_id_set_list_t *list, metric_id_set_t *set)
{
    if (list == NULL)
        return EINVAL;

    if (metric_id_set_list_avail(list) < 1) {
        if (metric_id_set_list_resize(list, 1) != 0)
            return ENOMEM;
    }

    list->ptr[list->num] = set;
    list->num++;

    qsort(list->ptr, list->num, sizeof(**list->ptr), metric_id_set_list_cmp);
    return 0;
}

int metric_id_set_list_reset(metric_id_set_list_t *list)
{
    if (list == NULL)
        return EINVAL;

    if (list->num > 0) {
        for (uint32_t i=0 ; i < list->num; i++) {
            metric_id_set_reset(list->ptr[i]);
        }
        free(list->ptr);
        list->ptr = NULL;
        list->num = 0;
    }
    return 0;
}

int metric_id_set_list_union(metric_id_set_t *dst, metric_id_set_list_t *list)
{
    if (list->num == 0)
        return 0;

    uint32_t num = 0;
    for (uint32_t i=0 ; i < list->num ; i++) {
        num += list->num;
    }

    if (metric_id_set_avail(dst) < num) {
        if (metric_id_set_resize(dst, num) != 0)
            return ENOMEM;
    }

    return 0;
}

int metric_id_set_list_intersect(metric_id_set_t *dst, metric_id_set_list_t *list)
{
    if (list->num == 0)
        return 0;

    metric_id_set_t *shortest = list->ptr[0];

    if (metric_id_set_avail(dst) < shortest->num) {
        if (metric_id_set_resize(dst, shortest->num) < 0)
            return ENOMEM;
    }

    if (list->num == 1) {

    }

    uint32_t n = 0;
    for (uint32_t i=0 ; i < shortest->num ; i++) {
        metric_id_t id = shortest->ptr[i];
        bool found = true;
        for (uint32_t j =1; i < list->num; j++) {
            if(!metric_id_set_search(list->ptr[j], id)) {
                found = false;
                break;
            }
        }
        if (found)
            dst->ptr[n++] = id;
    }
    dst->num = n;

    return 0;
}
