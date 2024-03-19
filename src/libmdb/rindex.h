/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "libutils/strlist.h"
#include "libmetric/label_set.h"
#include "libmetric/metric_match.h"
#include "libmdb/metric_id.h"


typedef struct {
    char *lvalue;
    metric_id_set_t ids;
} rindex_label_value_t;

typedef struct {
    char *lname;
    htable_t values;  /* rindex_label_value_t */
    metric_id_set_t ids;
} rindex_label_t;

typedef struct {
    char *name;
    htable_t labels; /* rindex_label_t */
    metric_id_set_t ids;
} rindex_name_t;

typedef struct {
    htable_t name_table;   /* rindex_name_t */
} rindex_t;

int rindex_init(rindex_t *rindex);

int rindex_destroy(rindex_t *rindex);

int rindex_insert(rindex_t *rindex, metric_id_t id, const char *metric, const label_set_t *label);

int rindex_search(rindex_t *rindex, metric_id_set_t *result, const metric_match_t *match);

strlist_t *rindex_get_metrics(rindex_t *rindex);

strlist_t *rindex_get_metric_labels(rindex_t *rindex, char *metric);

strlist_t *rindex_get_metric_label_value(rindex_t *rindex, char *metric, char *label);
