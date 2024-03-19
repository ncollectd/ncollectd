/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "libmetric/label_set.h"

typedef struct {
    char *name;
    label_set_t labels;
} mql_metric_t;

typedef struct {
    int64_t timestamp;
    double  value;
} mql_point_t;

typedef struct {
    mql_metric_t metric;
    mql_point_t point;
} mql_sample_t;

typedef struct {
    size_t size;
    mql_sample_t *ptr;
} mql_value_samples_t;

typedef struct {
    size_t size;
    mql_point_t *ptr;
} mql_points_t;

typedef struct {
    mql_metric_t metric;
    mql_points_t points;
} mql_serie_t;

typedef struct {
    size_t size;
    mql_serie_t *ptr;
} mql_value_series_t;

typedef struct {
    uint64_t timestamp;
    double value;
} mql_value_scalar_t;

typedef enum {
    MQL_VALUE_NONE,
    MQL_VALUE_SCALAR,
    MQL_VALUE_SAMPLES,
    MQL_VALUE_SERIES,
} mql_value_e;

typedef struct {
    mql_value_e kind;
    union {
        mql_value_scalar_t scalar;
        mql_value_samples_t samples;
        mql_value_series_t series;
    };
} mql_value_t;

typedef struct {
    int num;
    mql_value_t **values;
} mql_value_list_t;

void mql_value_free(mql_value_t *value);

mql_value_t *mql_value_series(void);

mql_value_t *mql_value_samples(void);

int mql_value_series_add(mql_value_t *value, mql_serie_t *src_serie);

mql_value_t *mql_value_samples_dup(mql_value_t *src_value, bool drop_family);

int mql_value_samples_add(mql_value_t *value, mql_sample_t *src_sample);

mql_value_t *mql_value_scalar(int64_t timestamp, double scalar);

int mql_value_dump(mql_value_t *value);

#if 0

typedef struct {
    uint64_t timestamp;
    double value;
} mql_value_point_t;

typedef struct {
    size_t num;
    mql_value_point_t *ptr;
} mql_value_points_t;

typedef struct {
    label_set_t labels;
    union {
        mql_value_point_t point;
        mql_value_points_t points;
    };
} mql_value_vector_t;

typedef struct {
} mql_value_vector_t;


typedef struct {
    label_set_t labels;
    union {
        mql_value_point_t point;
        mql_value_points_t points;
    };
} mql_value_serie_t;

typedef struct {
    size_t num;
    mql_value_serie_t *ptr;
} mql_value_vector_t;


typedef enum {
    MQL_VALUE_NONE,
    MQL_VALUE_MATRIX,
    MQL_VALUE_VECTOR,
    MQL_VALUE_SCALAR,
    MQL_VALUE_STRING
} mql_value_e;

typedef struct {
    mql_value_e kind;
    union {
        struct {
        } vector;
        struct {
            uint64_t timestamp;
            double value;
        } scalar;
        struct {
            uint64_t timestamp;
            char *value;
        } string;
    };
} mql_value_t;

#endif
