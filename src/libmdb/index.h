/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "libmetric/metric.h"
#include "libmetric/metric_match.h"
#include "libutils/htable.h"
#include "libmdb/metric_id.h"
#include "libmdb/series_list.h"
#include "libmdb/storage.h"

typedef struct {
    metric_id_t id;
    char *name;
    label_set_t label;
    storage_id_t sid;
} index_metric_t;

typedef struct {
    metric_id_t alloc;
    metric_id_t num;
    index_metric_t **ptr;
} index_metric_id_t;

typedef struct {
    index_metric_id_t set;
    htable_t metric_table; /* index_metric_t */
} mdb_index_t;


index_metric_t *index_find(mdb_index_t *index, const char *metric, const label_set_t *labels);

index_metric_t *index_insert(mdb_index_t *index, const char *metric, const label_set_t *labels);

int index_init(mdb_index_t *index);

int index_destroy(mdb_index_t *index, storage_t *storage);

mdb_series_list_t *index_get_series(mdb_index_t *index);
