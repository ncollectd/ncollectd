// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "libutils/time.h"
#include "libutils/strbuf.h"
#include "libutils/htable.h"
#include "libmetric/label_set.h"
#include "libmetric/metric.h"
#include "libmdb/metric_id.h"
#include "libmdb/index.h"
#include "libmdb/series_list.h"

#define HTABLE_METRIC_SIZE      256

static void index_metric_id_reset(index_metric_id_t *set)
{
    if (set == NULL)
        return;

    free(set->ptr);
    set->ptr = NULL;
    set->alloc = 0;
    set->num = 0;
}

static inline int index_metric_id_resize(index_metric_id_t *set, size_t need)
{
    size_t new_size = set->num + need;
    if (new_size == 1) {
        new_size = 1024;
    } else {
        new_size = 1 << (64 - __builtin_clzl(new_size - 1));
    }

    index_metric_t **new_ptr = realloc(set->ptr, sizeof(*set->ptr) * new_size);
    if (new_ptr == NULL)
        return ENOMEM;

    set->ptr = new_ptr;
    set->alloc = new_size;

    return 0;
}

static inline int index_metric_id_avail(index_metric_id_t *set)
{
    return set->alloc ? set->num - set->alloc : 0;
}

static int index_metric_alloc(index_metric_id_t *set, metric_id_t *id, index_metric_t **mc)
{
    if (index_metric_id_avail(set) < 1) {
        if (index_metric_id_resize(set, 1) != 0)
            return ENOMEM;
    }

    index_metric_t *m = calloc(1, sizeof(*m));
    if (m == NULL) {
        return ENOMEM;
    }

    set->ptr[set->num] = m;
    *mc = m;
    *id = set->num;
    set->num++;

    return 0;
}

static void index_metric_free(void *arg1, void *arg2)
{
    index_metric_t *mcm = arg1;
    storage_t *st = arg2;

    if (mcm == NULL)
        return;

    free(mcm->name);
    label_set_reset(&mcm->label);
    storage_id_destroy(st, &mcm->sid);
    free(mcm);
}

#define CHAR_GS 0x1D
#define CHAR_RS 0x1E

static inline int index_label_serialize(strbuf_t *buf, const label_set_t *labels)
{
    int status = 0;

    if (labels != NULL) {
        for (size_t n = 0 ; n  < labels->num; n++) {
            label_pair_t *pair = &labels->ptr[n];
            status |= strbuf_putchar(buf, CHAR_GS);
            status |= strbuf_putstr(buf, pair->name);
            status |= strbuf_putchar(buf, CHAR_RS);
            status |= strbuf_putstr(buf, pair->value);
        }
    }

    return status;
}

static int index_insert_cmp(const void *a, const void *b)
{
    const index_metric_t *mcma = (const index_metric_t *)a;
    const index_metric_t *mcmb = (const index_metric_t *)b;

    char buffera[4096] = "";
    strbuf_t bufa = STRBUF_CREATE_STATIC(buffera);
    char bufferb[4096] = "";
    strbuf_t bufb = STRBUF_CREATE_STATIC(bufferb);

    strbuf_putstr(&bufa, mcma->name);
    index_label_serialize(&bufa, &mcma->label);

    strbuf_putstr(&bufb, mcmb->name);
    index_label_serialize(&bufa, &mcmb->label);

    return strcmp(bufa.ptr, bufb.ptr);
}

static int index_find_cmp(const void *a, const void *b)
{
    const char *metric = (const char *)a;
    const index_metric_t *mc = (const index_metric_t *)b;

    char buffer[4096] = "";
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);

    strbuf_putstr(&buf, mc->name);
    index_label_serialize(&buf, &mc->label);

    return strcmp(metric, buf.ptr);
}

index_metric_t *index_find(mdb_index_t *index, const char *metric, const label_set_t *labels)
{
    char buffer[4096] = "";
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);
    strbuf_putstr(&buf, metric);
    index_label_serialize(&buf, labels);

    htable_hash_t hash = htable_hash(buf.ptr, HTABLE_HASH_INIT); // FIXME
    index_metric_t *mcm = htable_find(&index->metric_table, hash, buf.ptr, index_find_cmp);
    if (mcm != NULL)
        return mcm;

    return NULL;
}

index_metric_t *index_insert(mdb_index_t *index, const char *metric, const label_set_t *labels)
{
    char buffer[4096] = "";
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);
    strbuf_putstr(&buf, metric);
    index_label_serialize(&buf, labels);

    htable_hash_t hash = htable_hash(buf.ptr, HTABLE_HASH_INIT); // FIXME
    index_metric_t *mcm = htable_find(&index->metric_table, hash, buf.ptr, index_find_cmp);
    if (mcm != NULL) {
        return mcm;
    }

    metric_id_t id = 0;
    int status = index_metric_alloc(&index->set, &id, &mcm);
    if (status != 0) {
        return NULL;
    }

    // set metric data
    mcm->name = strdup(metric);
    if (mcm->name == NULL)
        return NULL;
    label_set_clone(&mcm->label, *labels);
    mcm->id = id;

    // FIXME mc->interval

    status = htable_add(&index->metric_table, hash, mcm, index_insert_cmp);
    if (status != 0) {
        // ERROR FIXME
        return NULL;
    }

    return mcm;
}

int index_init(mdb_index_t *index)
{
    htable_init(&index->metric_table, HTABLE_METRIC_SIZE);
    return 0;
}

int index_destroy(mdb_index_t *index, storage_t *storage)
{
    htable_destroy(&index->metric_table, index_metric_free, storage);
    index_metric_id_reset(&index->set);
    return 0;
}

mdb_series_list_t *index_get_series(mdb_index_t *index)
{
    mdb_series_list_t *sl = calloc(1, sizeof(*sl));
    if (sl == NULL)
        return NULL;

    sl->num = index->set.num;
    sl->ptr = NULL;

    if (sl->num == 0)
        return sl;

    mdb_series_t *s = calloc(index->set.num, sizeof(*s));
    if (s == NULL) {
        free(sl);
        return NULL;
    }

    sl->ptr = s;

    for (size_t i =0, n=0; i < index->set.alloc; i++) {
        index_metric_t *m = index->set.ptr[i];
        if (m != NULL) {
            s[n].name = (m->name != NULL) ? strdup(m->name) : NULL;
            label_set_clone(&s[n].labels, m->label);

            n++;
            if (n == index->set.num)
                break;
        }
    }

    return sl;
}
