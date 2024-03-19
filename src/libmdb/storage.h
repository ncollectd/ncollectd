/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "libmdb/mdb.h"

typedef struct {
    cdtime_t time;
    double value;
} storage_memory_point_t;

typedef struct {
    cdtime_t last_time;
    cdtime_t last_update;
    cdtime_t interval;
    size_t num;
    size_t head;
    size_t tail;
    storage_memory_point_t points[];
} storage_memory_entry_t;

typedef struct {
    size_t length;
} storage_memory_t;

typedef struct {
    storage_memory_entry_t *entry;
} storage_id_t;

typedef enum {
    STORAGE_TYPE_MEMORY,
    STORAGE_TYPE_DISK
} storage_type_t;

typedef struct {
    storage_type_t type;
    storage_memory_t mem;
} storage_t;

int storage_init(storage_t *storage);

void storage_destroy(storage_t *storage);

void storage_id_destroy(storage_t *storage, storage_id_t *sid);

int storage_id_init(storage_t *storage, storage_id_t *sid, cdtime_t interval);

int storage_insert(storage_t *storage, storage_id_t *sid,
                   cdtime_t time, cdtime_t interval, mdb_value_t value);

int storage_fetch(storage_t *storage, storage_id_t *sid, mdb_series_t *series, cdtime_t time);

int storage_fetch_range(storage_t *storage, storage_id_t *sid,
                        mdb_series_t *series, cdtime_t start, cdtime_t end, cdtime_t step);

