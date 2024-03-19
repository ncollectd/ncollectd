/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "libmetric/metric.h"
#include "libmetric/metric_match.h"
#include "libutils/time.h"
#include "libmdb/metric_id.h"
#include "libmdb/strlist.h"
#include "libmdb/family_metric_list.h"
#include "libmdb/family.h"
#include "libmdb/series_list.h"

typedef enum {
    MDB_VALUE_TYPE_GAUGE_FLOAT64 = 0,
    MDB_VALUE_TYPE_GAUGE_INT64,
    MDB_VALUE_TYPE_COUNTER_UINT64,
    MDB_VALUE_TYPE_COUNTER_FLOAT64,
    MDB_VALUE_TYPE_BOOL,
    MDB_VALUE_TYPE_INFO,
} mdb_value_type_t;

typedef struct {
    mdb_value_type_t type;
    union {
        double   float64;
        uint64_t uint64;
        int64_t  int64;
        bool     boolean;
    };
} mdb_value_t;

#define MDB_VALUE_GAUGE_FLOAT64(d) \
    (mdb_value_t){.type = MDB_VALUE_TYPE_GAUGE_FLOAT64, .float64 = (d)}
#define MDB_VALUE_GAUGE_INT64(d) \
    (mdb_value_t){.type = MDB_VALUE_TYPE_GAUGE_INT64, .int64 = (d)}
#define MDB_VALUE_COUNTER_UINT64(d) \
    (mdb_value_t){.type = MDB_VALUE_TYPE_COUNTER_UINT64, .uint64 = (d)}
#define MDB_VALUE_COUNTER_FLOAT64(d) \
    (mdb_value_t){.type = MDB_VALUE_TYPE_COUNTER_FLOAT64, .float64 = (d)}
#define MDB_VALUE_BOOL(d) \
    (mdb_value_t){.type = MDB_VALUE_TYPE_BOOL, .boolean = (d)}
#define MDB_VALUE_INFO \
    (mdb_value_t){.type = MDB_VALUE_TYPE_INFO}

typedef struct {
    char *path;
} mdb_config_t;

struct mdb_s;
typedef struct mdb_s mdb_t;

int mdb_config(mdb_t *mdb, mdb_config_t *config);

mdb_t *mdb_alloc(void);

void mdb_free(mdb_t *mdb);

int mdb_init(mdb_t *mdb);

int mdb_load_index(mdb_t *mdb);

int mdb_load_data(mdb_t *mdb);

int mdb_replay_journal(mdb_t *mdb);

int mdb_shutdown(mdb_t *mdb);

int mdb_insert_metric_id(mdb_t *mdb, metric_id_t id, cdtime_t time, cdtime_t interval,
                                     mdb_value_t value);

int mdb_insert_metric(mdb_t *mdb, const char *metric, const label_set_t *labels,
                                  cdtime_t time, cdtime_t interval, mdb_value_t value);

int mdb_insert_metric_family(mdb_t *mdb, metric_family_t *fam);

int mdb_delete_metric(mdb_t *mdb, const char *metric, label_set_t *labels);

int mdb_delete_metric_match(mdb_t *mdb, const metric_match_t *match);

mdb_family_metric_list_t *mdb_get_metric_family(mdb_t *mdb); /* match */

strlist_t *mdb_get_metrics(mdb_t *mdb); /* match */

mdb_series_list_t *mdb_get_series(mdb_t *mdb);

strlist_t *mdb_get_metric_label(mdb_t *mdb, char *metric);

strlist_t *mdb_get_metric_label_value(mdb_t *mdb, char *metric, char *label);


#if 0
mdb_stmt_t *mdb_query_prepare(mdb_t *mdb, const char *query);

mdb_query_exec(mdb_t *mdb, const char *query, cdtime_t start, cdtime_t end, cdtime_t step);

mdb_query_exec_prepared(mdb_t *mdb, mdb_stmt_t *stmt, cdtime_t start, cdtime_t end, cdtime_t step);

void mdb_stmt_free(mdb_stmt_t *stmt);

int mdb_stmt_dump(mdb_stmt_t *stmt, FILE *stream);

mql_value_t *mql_eval(mql_eval_ctx_t *ctx, mql_node_t *node);

#endif

mdb_series_list_t *mdb_fetch(mdb_t *mdb, const metric_match_t *match, cdtime_t time);

mdb_series_list_t *mdb_fetch_range(mdb_t *mdb, const metric_match_t *match,
                                   cdtime_t start, cdtime_t end, cdtime_t step);

