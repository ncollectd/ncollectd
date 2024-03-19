// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libmdb/storage.h"

int storage_init(storage_t *storage)
{
    if (storage == NULL)
        return -1;

    storage->mem.length = 6;
//    storage->mem.length = 300;
    storage->type = STORAGE_TYPE_MEMORY;

    return 0;
}

void storage_destroy(storage_t *storage)
{
    if (storage == NULL)
        return;

    return;
}

void storage_id_destroy(storage_t *storage, storage_id_t *sid)
{
    if ((storage == NULL) || (sid == NULL))
        return;

    if (storage->type == STORAGE_TYPE_MEMORY) {
        free(sid->entry);
        sid->entry = NULL;
    }
}

int storage_id_init(storage_t *storage, storage_id_t *sid, cdtime_t interval)
{
    if ((storage == NULL) || (sid == NULL))
        return 0;

    if (storage->type == STORAGE_TYPE_MEMORY) {
        size_t length = storage->mem.length;

        storage_memory_entry_t *entry = calloc(1, sizeof(*entry) +
                                                  length * sizeof(storage_memory_point_t));
        if (entry == NULL)
            return 0;

        entry->last_time = 0;
        entry->last_update = 0;
        entry->interval = interval;
        entry->head = 0;
        entry->tail = 0;
        entry->num = 0;

        for (size_t i=0; i < length; i++) {
            entry->points[i].time = 0;
            entry->points[i].value = NAN;
        }

        sid->entry = entry;

        return 0;
    }

    return 0;
}

static int storage_memory_insert(storage_memory_t *mem, storage_memory_entry_t *entry,
                                 cdtime_t time,
                                 __attribute__((unused)) cdtime_t interval, mdb_value_t value)
{
    if ((mem == NULL) || (entry == NULL))
        return -1;

    double dval = 0.0;

    switch(value.type) {
    case MDB_VALUE_TYPE_GAUGE_FLOAT64:
        dval = value.float64;
        break;
    case MDB_VALUE_TYPE_GAUGE_INT64:
        dval = value.int64;
        break;
    case MDB_VALUE_TYPE_COUNTER_UINT64:
        dval = value.uint64;
        break;
    case MDB_VALUE_TYPE_COUNTER_FLOAT64:
        dval = value.float64;
        break;
    case MDB_VALUE_TYPE_BOOL:
        dval = value.boolean ? 1.0 : 0.0;
        break;
    case MDB_VALUE_TYPE_INFO:
        dval = 1.0;
        break;
    }

    assert(mem->length > 0);
    size_t length = mem->length;

    entry->points[entry->tail].time = time;
    entry->points[entry->tail].value = dval;

    entry->last_update = time;

    entry->tail++;
    if (entry->tail >= length)
        entry->tail %= length;

    entry->num++;
    if (entry->num >= length) {
        entry->num = length;
        entry->head++;
        if (entry->head >= length)
            entry->head %= length;
    }

    return 0;
}

static int storage_memory_fetch(storage_memory_t *mem, storage_memory_entry_t *entry,
                                mdb_series_t *series,
                                __attribute__((unused)) cdtime_t time)
{
    if ((mem == NULL) || (entry == NULL) || (series == NULL))
        return -1;

    return 0;
}

static int storage_memory_fetch_range(storage_memory_t *mem, storage_memory_entry_t *entry,
                                      mdb_series_t *series,
                                      __attribute__((unused)) cdtime_t start,
                                      __attribute__((unused)) cdtime_t end,
                                      __attribute__((unused)) cdtime_t step)
{
    if ((mem == NULL) || (entry == NULL) || (series == NULL))
        return -1;
#if 0
    cdtime_t st_start = entry->points[entry->head].time;
    size_t n_end = entry->head + entry->num;
    if (n_end >= mem->length)
        n_end %= mem->length;
    cdtime_t st_end =  entry->points[n_end].time;

    if (end < st_start)
        return 0;
    if (start > st_end)
        return 0;

    cdtime_t start_ts = series->points[0].timestamp;

    size_t n = 0;
    while(n < entry->num)  {
        size_t n = entry->heap + ts;
        if (n > mem->length)
            n %= mem->length;

        if (entry->points[n].time < start_ts)
            break;

        n++;
    }

    if (n >= entry->num)
        break;

    for (size_t i = 0; i < series->num ; i++) {
        cdtime_t ts = series->points[i].timestamp;




    }

#endif
    return 0;
}

int storage_insert(storage_t *storage, storage_id_t *sid,
                   cdtime_t time, cdtime_t interval, mdb_value_t value)
{
    if ((storage == NULL) || (sid == NULL))
        return -1;

    if (storage->type == STORAGE_TYPE_MEMORY)
        return storage_memory_insert(&storage->mem, sid->entry, time, interval, value);

    return 0;
}

int storage_fetch(storage_t *storage, storage_id_t *sid, mdb_series_t *series, cdtime_t time)
{
    if ((storage == NULL) || (sid == NULL))
        return -1;

    if (storage->type == STORAGE_TYPE_MEMORY)
        return storage_memory_fetch(&storage->mem, sid->entry, series, time);

    return 0;
}

int storage_fetch_range(storage_t *storage, storage_id_t *sid,
                        mdb_series_t *series, cdtime_t start, cdtime_t end, cdtime_t step)
{
    if ((storage == NULL) || (sid == NULL))
        return -1;

    if (storage->type == STORAGE_TYPE_MEMORY)
        return storage_memory_fetch_range(&storage->mem, sid->entry, series, start, end, step);

    return 0;
}
