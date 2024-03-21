// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"

#include <pthread.h>

#include "libutils/time.h"
#include "libutils/strbuf.h"
#include "libutils/dtoa.h"
#include "libmdb/index.h"
#include "libmdb/rindex.h"
#include "libmdb/family.h"
#include "libmdb/storage.h"
#include "libmdb/mdb.h"

struct mdb_s {
    pthread_mutex_t lock_family;
    mdb_family_t family;
    pthread_mutex_t lock_rindex;
    rindex_t rindex;
    pthread_mutex_t lock_index;
    mdb_index_t index;
    pthread_mutex_t lock_storage;
    storage_t storage;
};

int mdb_config(mdb_t *mdb, mdb_config_t *config)
{
    if (mdb == NULL)
        return -1;

    if (config == NULL)
        return -1;

    return 0;
}

mdb_t *mdb_alloc(void)
{
    mdb_t *mdb = calloc(1, sizeof(*mdb));
    if (mdb == NULL)
        return NULL;

    pthread_mutex_init(&mdb->lock_family, NULL);
    pthread_mutex_init(&mdb->lock_rindex, NULL);
    pthread_mutex_init(&mdb->lock_index, NULL);
    pthread_mutex_init(&mdb->lock_storage, NULL);

    return mdb;
}

int mdb_init(mdb_t *mdb)
{
    if (mdb == NULL)
        return -1;

    family_init(&mdb->family);
    rindex_init(&mdb->rindex);
    index_init(&mdb->index);
    storage_init(&mdb->storage);

    return 0;
}

int mdb_load_index(mdb_t *mdb)
{
    if (mdb == NULL)
        return -1;
    return 0;
}

int mdb_load_data(mdb_t *mdb)
{
    if (mdb == NULL)
        return -1;
    return 0;
}

int mdb_replay_journal(mdb_t *mdb)
{
    if (mdb == NULL)
        return -1;
    return 0;
}

int mdb_shutdown(mdb_t *mdb)
{
    if (mdb == NULL)
        return -1;

//    pthread_mutex_lock(&mdb->lock_family);
//    pthread_mutex_lock(&mdb->lock_rindex);
//    pthread_mutex_lock(&mdb->lock_index);

    return 0;
}

void mdb_free(mdb_t *mdb)
{
    pthread_mutex_lock(&mdb->lock_family);
    family_destroy(&mdb->family);
    pthread_mutex_unlock(&mdb->lock_family);
    pthread_mutex_destroy(&mdb->lock_family);

    pthread_mutex_lock(&mdb->lock_rindex);
    rindex_destroy(&mdb->rindex);
    pthread_mutex_unlock(&mdb->lock_rindex);
    pthread_mutex_destroy(&mdb->lock_rindex);

    pthread_mutex_lock(&mdb->lock_index);
    index_destroy(&mdb->index, &mdb->storage);
    pthread_mutex_unlock(&mdb->lock_index);
    pthread_mutex_destroy(&mdb->lock_index);

    pthread_mutex_lock(&mdb->lock_storage);
    storage_destroy(&mdb->storage);
    pthread_mutex_unlock(&mdb->lock_storage);
    pthread_mutex_destroy(&mdb->lock_storage);

    free(mdb);
}

#if 0
#define CHAR_GS 0x1D
#define CHAR_RS 0x1E

static int mdb_metric_serialize(strbuf_t *buf, const char *metric, const char *metric_suffix,
                                const label_set_t *labels1, const label_set_t *labels2)
{
    int status = strbuf_putstr(buf, metric);
    if (metric_suffix != NULL)
        status |= strbuf_putstr(buf, metric_suffix);

    if (labels1 != NULL) {
        for (size_t n = 0 ; n  < labels1->num; n++) {
            label_pair_t *pair = &labels1->ptr[n];
            status |= strbuf_putchar(buf, CHAR_GS);
            status |= strbuf_putstr(buf, pair->name);
            status |= strbuf_putchar(buf, CHAR_RS);
            status |= strbuf_putstr(buf, pair->value);
        }
    }

    if (labels2 != NULL) {
        for (size_t n = 0 ; n  < labels2->num; n++) {
            label_pair_t *pair = &labels2->ptr[n];
            status |= strbuf_putchar(buf, CHAR_GS);
            status |= strbuf_putstr(buf, pair->name);
            status |= strbuf_putchar(buf, CHAR_RS);
            status |= strbuf_putstr(buf, pair->value);
        }
    }

    return status;
}
#endif

int mdb_insert_metric_id(mdb_t *mdb, __attribute__((unused)) metric_id_t id,
                                     __attribute__((unused)) cdtime_t time,
                                     __attribute__((unused)) cdtime_t interval,
                                     __attribute__((unused)) mdb_value_t value)
{
    if (mdb == NULL)
        return -1;
    return 0;
}

int mdb_insert_metric(mdb_t *mdb, const char *metric, const label_set_t *labels,
                                  cdtime_t time, cdtime_t interval, mdb_value_t value)
{
    if (mdb == NULL)
        return -1;

    storage_id_t *sid = NULL;

    pthread_mutex_lock(&mdb->lock_index);
    index_metric_t *idx = index_find(&mdb->index, metric, labels);
    if (idx == NULL) {
        idx = index_insert(&mdb->index, metric, labels);
        if (idx == NULL) {
            pthread_mutex_unlock(&mdb->lock_index);
            return -1;
        }

        // FIXME
        storage_id_init(&mdb->storage, &idx->sid, interval);

        sid = &idx->sid;

        pthread_mutex_lock(&mdb->lock_rindex);
        int status = rindex_insert(&mdb->rindex, idx->id, metric, labels);
        if (status != 0) {
            pthread_mutex_unlock(&mdb->lock_index);
            pthread_mutex_unlock(&mdb->lock_rindex);
            return -1;
        }
        pthread_mutex_unlock(&mdb->lock_rindex);
    } else {
        sid = &idx->sid;
    }
    pthread_mutex_unlock(&mdb->lock_index);

    if (sid == NULL)
        return 0;

    storage_insert(&mdb->storage, sid, time, interval, value);

    return 0;
}

static int mdb_insert_metric_internal(mdb_t *mdb, const char *metric, const char *metric_suffix,
                                      const label_set_t *labels1, const label_set_t *labels2,
                                      cdtime_t time, cdtime_t interval, mdb_value_t value)
{
    size_t lsize = 0;
    lsize = (labels1 != NULL ? labels1->num : 0) + (labels2 != NULL ? labels2->num : 0);

    if (metric_suffix == NULL) {
        if (labels2 == NULL)
            return mdb_insert_metric(mdb, metric, labels1, time, interval, value);

        if (lsize == 0)
            return mdb_insert_metric(mdb, metric, NULL, time, interval, value);

        label_pair_t pairs[lsize];
        label_set_t labels = {.num = lsize, .ptr = pairs};

        size_t n = 0;
        if (labels1 != NULL) {
            for (size_t i = 0 ; i  < labels1->num; i++) {
                label_pair_t *pair = &labels1->ptr[i];
                labels.ptr[n].name = pair->name;
                labels.ptr[n].value = pair->value;
                n++;
            }
        }

        if (labels2 != NULL) {
            for (size_t i = 0 ; i  < labels2->num; i++) {
                label_pair_t *pair = &labels2->ptr[i];
                labels.ptr[n].name = pair->name;
                labels.ptr[n].value = pair->value;
                n++;
            }
            label_set_qsort(&labels);
        }

        return mdb_insert_metric(mdb, metric, &labels, time, interval, value);
    }

    char buffer[4096] = "";
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);

    int status = strbuf_putstr(&buf, metric);
    if (metric_suffix != NULL)
        status |= strbuf_putstr(&buf, metric_suffix);

    if (status != 0) {
        // FIXME
    }

    if (labels2 == NULL)
        return mdb_insert_metric(mdb, buf.ptr, labels1, time, interval, value);

    if (lsize == 0)
        return mdb_insert_metric(mdb, buf.ptr, NULL, time, interval, value);

    label_pair_t pairs[lsize];
    label_set_t labels = {.num = lsize, .ptr = pairs};

    size_t n = 0;
    if (labels1 != NULL) {
        for (size_t i = 0 ; i  < labels1->num; i++) {
            label_pair_t *pair = &labels1->ptr[i];
            labels.ptr[n].name = pair->name;
            labels.ptr[n].value = pair->value;
            n++;
        }
    }

    if (labels2 != NULL) {
        for (size_t i = 0 ; i  < labels2->num; i++) {
            label_pair_t *pair = &labels2->ptr[i];
            labels.ptr[n].name = pair->name;
            labels.ptr[n].value = pair->value;
            n++;
        }
        label_set_qsort(&labels);
    }

    return mdb_insert_metric(mdb, buf.ptr, &labels, time, interval, value);
}

int mdb_insert_metric_family(mdb_t *mdb, metric_family_t *fam)
{
    if (fam == NULL)
        return EINVAL;

    pthread_mutex_lock(&mdb->lock_family);
    int status = family_getsert(&mdb->family, fam);
    pthread_mutex_unlock(&mdb->lock_family);
    if (status != 0) {
        // FIXME
    }


    for (size_t i = 0; i < fam->metric.num; i++) {
        metric_t *m = &fam->metric.ptr[i];

        switch(fam->type) {
        case METRIC_TYPE_UNKNOWN: {
            mdb_value_t value = {0};
            if (m->value.unknown.type == UNKNOWN_FLOAT64) {
                value = MDB_VALUE_GAUGE_FLOAT64(m->value.unknown.float64);
            } else {
                value = MDB_VALUE_GAUGE_INT64(m->value.unknown.int64);
            }
            status |= mdb_insert_metric_internal(mdb, fam->name, NULL, &m->label, NULL,
                                                      m->time, m->interval, value);
        }   break;
        case METRIC_TYPE_GAUGE: {
            mdb_value_t value;
            if (m->value.gauge.type == GAUGE_FLOAT64) {
                value = MDB_VALUE_GAUGE_FLOAT64(m->value.gauge.float64);
            } else {
                value = MDB_VALUE_GAUGE_INT64(m->value.gauge.int64);
            }
            status |= mdb_insert_metric_internal(mdb, fam->name, NULL, &m->label, NULL,
                                                      m->time, m->interval, value);
        }   break;
        case METRIC_TYPE_COUNTER: {
            mdb_value_t value = {0};
            if (m->value.counter.type == COUNTER_UINT64) {
                value = MDB_VALUE_COUNTER_UINT64(m->value.counter.uint64);
            } else {
                value = MDB_VALUE_COUNTER_FLOAT64(m->value.counter.float64);
            }
            status |= mdb_insert_metric_internal(mdb, fam->name, "_total", &m->label, NULL,
                                                      m->time, m->interval, value);
        }   break;
        case METRIC_TYPE_STATE_SET:
            for (size_t j = 0; j < m->value.state_set.num; j++) {
                label_pair_t label_pair = {.name = fam->name,
                                           .value = m->value.state_set.ptr[j].name};
                label_set_t label_set = {.num = 1, .ptr = &label_pair};
                mdb_value_t value = MDB_VALUE_BOOL(m->value.state_set.ptr[j].enabled ? 1 : 0);

                status |= mdb_insert_metric_internal(mdb, fam->name, NULL,
                                                          &m->label, &label_set,
                                                          m->time, m->interval, value);
            }
            break;
        case METRIC_TYPE_INFO:
            status |= mdb_insert_metric_internal(mdb, fam->name, "_info",
                                                      &m->label, &m->value.info,
                                                      m->time, m->interval, MDB_VALUE_INFO);
            break;
        case METRIC_TYPE_SUMMARY: {
            summary_t *s = m->value.summary;

            for (int j = s->num - 1; j >= 0; j--) {
                char quantile[DTOA_MAX];

                dtoa(s->quantiles[j].quantile, quantile, sizeof(quantile));

                label_pair_t label_pair = {.name = "quantile", .value = quantile};
                label_set_t label_set = {.num = 1, .ptr = &label_pair};
                status |= mdb_insert_metric_internal(mdb, fam->name, NULL,
                                                     &m->label, &label_set, m->time, m->interval,
                                                     MDB_VALUE_GAUGE_FLOAT64(s->quantiles[j].value));
            }

            status |= mdb_insert_metric_internal(mdb, fam->name, "_count", &m->label, NULL,
                                                 m->time, m->interval,
                                                 MDB_VALUE_GAUGE_INT64(s->count));
            status |= mdb_insert_metric_internal(mdb, fam->name, "_sum", &m->label, NULL,
                                                 m->time, m->interval,
                                                 MDB_VALUE_GAUGE_FLOAT64(s->sum));
        }   break;
        case METRIC_TYPE_HISTOGRAM:
        case METRIC_TYPE_GAUGE_HISTOGRAM: {
            histogram_t *h = m->value.histogram;

            for (int j = h->num - 1; j >= 0; j--) {
                char le[DTOA_MAX];
                dtoa(h->buckets[j].maximum, le, sizeof(le));

                label_pair_t label_pair = {.name = "le", .value = le};
                label_set_t label_set = {.num = 1, .ptr = &label_pair};
                status |= mdb_insert_metric_internal(mdb, fam->name, "_bucket",
                                             &m->label, &label_set, m->time, m->interval,
                                             MDB_VALUE_GAUGE_INT64(h->buckets[j].counter));
            }

            status |= mdb_insert_metric_internal(mdb, fam->name,
                                         fam->type == METRIC_TYPE_HISTOGRAM ? "_count" : "_gcount",
                                         &m->label, NULL, m->time, m->interval,
                                         MDB_VALUE_GAUGE_INT64(histogram_counter(h)));
            status |= mdb_insert_metric_internal(mdb, fam->name,
                                         fam->type == METRIC_TYPE_HISTOGRAM ?  "_sum" : "_gsum",
                                         &m->label, NULL, m->time, m->interval,
                                         MDB_VALUE_GAUGE_FLOAT64(histogram_sum(h)));
        }   break;
        }

        if (status != 0)
            return status;
    }

    return 0;
}

int mdb_delete_metric(mdb_t *mdb, __attribute__((unused)) const char *metric,
                                  __attribute__((unused)) label_set_t *labels)
{
    if (mdb == NULL)
        return -1;

    return 0;
}

int mdb_delete_metric_match(mdb_t *mdb, __attribute__((unused)) const metric_match_t *match)
{
    if (mdb == NULL)
        return -1;

    return 0;
}

mdb_family_metric_list_t *mdb_get_metric_family(mdb_t *mdb)
{
    if (mdb == NULL)
        return NULL;

    pthread_mutex_lock(&mdb->lock_family);
    mdb_family_metric_list_t *faml = family_get_list(&mdb->family);
    pthread_mutex_unlock(&mdb->lock_family);
    return faml;
}

strlist_t *mdb_get_metrics(mdb_t *mdb) /* match ¿? */
{
    if (mdb == NULL)
        return NULL;

    pthread_mutex_lock(&mdb->lock_rindex);
    strlist_t *sl = rindex_get_metrics(&mdb->rindex);
    pthread_mutex_unlock(&mdb->lock_rindex);
    return sl;
}

mdb_series_list_t *mdb_get_series(mdb_t *mdb)
{
    if (mdb == NULL)
        return NULL;

    pthread_mutex_lock(&mdb->lock_index);
    mdb_series_list_t *sl = index_get_series(&mdb->index);
    pthread_mutex_unlock(&mdb->lock_index);
    return sl;
}

strlist_t *mdb_get_metric_label(mdb_t *mdb, char *metric)
{
    if ((mdb == NULL) || (metric == NULL))
        return NULL;

    pthread_mutex_lock(&mdb->lock_rindex);
    strlist_t *sl = rindex_get_metric_labels(&mdb->rindex, metric);
    pthread_mutex_unlock(&mdb->lock_rindex);
    return sl;
}

strlist_t *mdb_get_metric_label_value(mdb_t *mdb, char *metric, char *label)
{
    if ((mdb == NULL) || (metric == NULL) || (label == NULL))
        return NULL;

    pthread_mutex_lock(&mdb->lock_rindex);
    strlist_t *sl = rindex_get_metric_label_value(&mdb->rindex, metric, label);
    pthread_mutex_unlock(&mdb->lock_rindex);
    return sl;
}

mdb_series_list_t * mdb_fetch(mdb_t *mdb, __attribute__((unused)) const metric_match_t *match,
                                          __attribute__((unused)) cdtime_t time)
{
    if (mdb == NULL)
        return NULL;

    return NULL;
}

mdb_series_list_t *mdb_fetch_range(mdb_t *mdb, __attribute__((unused)) const metric_match_t *match,
                                               __attribute__((unused)) cdtime_t start,
                                               __attribute__((unused)) cdtime_t end,
                                               __attribute__((unused)) cdtime_t step)
{
    if (mdb == NULL)
        return NULL;

    metric_id_set_t result= {0};

    pthread_mutex_lock(&mdb->lock_rindex);

    int status = rindex_search(&mdb->rindex, &result, match);
    if (status != 0) {
        // FIXME free result
        pthread_mutex_unlock(&mdb->lock_rindex);
        return NULL;
    }

    pthread_mutex_unlock(&mdb->lock_rindex);

    if (result.num == 0) {
        // FIXME free result
        return NULL;
    }

    for (uint32_t i=0; i < result.num; i++) {
//        storage_memory_entry_t *entry,
//        storage_fetch_range(&mdb->storage, sid,
//                            series, start, end, step);
//        result.ptr[i]
    }

    return NULL;
}
