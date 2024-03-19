// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "libmetric/label_set.h"
#include "libmdb/value.h"

static inline mql_value_t *mql_value (mql_value_e kind)
{
    mql_value_t *value = calloc(1, sizeof(*value));
    if (value == NULL) {
        // FIXME
        return NULL;
    }

    value->kind = kind;
    return value;
}

void mql_value_free(mql_value_t *value)
{
    if (value == NULL)
        return;

    switch (value->kind) {
        case MQL_VALUE_NONE:
            break;
        case MQL_VALUE_SERIES:
            if (value->series.ptr != NULL) {
                for (size_t i = 0; i < value->series.size ; i++) {
                    mql_serie_t *serie = &value->series.ptr[i];
                    free(serie->metric.name);
                    label_set_reset(&serie->metric.labels);
                    free(serie->points.ptr);
                }
            }
            break;
        case MQL_VALUE_SAMPLES:
            if (value->samples.ptr != NULL) {
                for (size_t i = 0; i < value->samples.size ; i++) {
                    mql_sample_t *sample = &value->samples.ptr[i];
                    free(sample->metric.name);
                    label_set_reset(&sample->metric.labels);
                }
            }
            break;
        case MQL_VALUE_SCALAR:
            break;
    }

    free(value);
}

mql_value_t *mql_value_series(void)
{
    mql_value_t *value = mql_value(MQL_VALUE_SERIES);
    if (value == NULL)
        return NULL;

    return value;
}

int mql_value_series_add(mql_value_t *value,  mql_serie_t *src_serie)
{
    mql_serie_t *tmp = realloc(value->series.ptr, sizeof(*value->series.ptr) *
                                                  (value->series.size + 1));
    if (tmp == NULL) {
        // FIXME
        return -1;
    }

    value->series.ptr = tmp;
    mql_serie_t *serie = &value->series.ptr[value->series.size];
    value->series.size++;

    memset(serie, 0, sizeof(*serie));

    if (src_serie->metric.name != NULL)
        serie->metric.name = strdup(src_serie->metric.name);
    label_set_clone(&serie->metric.labels, src_serie->metric.labels);

    if (src_serie->points.size > 0) {
        serie->points.size = src_serie->points.size;
        serie->points.ptr = malloc(sizeof(*serie->points.ptr) * serie->points.size);
        if (serie->points.ptr == NULL) {
            // FIXME
            return -1;
        }
        memcpy(serie->points.ptr, src_serie->points.ptr,
               sizeof(*serie->points.ptr) * serie->points.size);
    } else {
        serie->points.size = 0;
        serie->points.ptr = NULL;
    }

    return 0;
}

mql_value_t *mql_value_samples(void)
{
    mql_value_t *value = mql_value(MQL_VALUE_SAMPLES);
    if (value == NULL)
        return NULL;

    return value;
}

mql_value_t *mql_value_samples_dup(mql_value_t *src_value, bool drop_family)
{
    if (src_value == NULL)
        return NULL;
    if (src_value->kind != MQL_VALUE_SAMPLES)
        return NULL;

    mql_value_t *value = mql_value(MQL_VALUE_SAMPLES);
    if (value == NULL)
        return NULL;

    if (src_value->samples.size > 0) {
        value->samples.ptr = calloc(src_value->samples.size, sizeof(*src_value->samples.ptr));
        if (value->samples.ptr == NULL) {
            mql_value_free(value);
            return NULL;
        }
        value->samples.size =    src_value->samples.size;
        for (size_t i = 0 ; i < src_value->samples.size; i++) {
            mql_sample_t *sample = &value->samples.ptr[i];
            mql_sample_t *src_sample = &src_value->samples.ptr[i];
            if (!drop_family) {
                if (src_sample->metric.name != NULL)
                    sample->metric.name = strdup(src_sample->metric.name);
            }
            label_set_clone(&sample->metric.labels, src_sample->metric.labels);

            sample->point.value = src_sample->point.value;
            sample->point.timestamp = src_sample->point.timestamp;
        }
    }

    return value;
}


int mql_value_samples_add(mql_value_t *value, mql_sample_t *src_sample)
{
    mql_sample_t *tmp = realloc(value->samples.ptr, sizeof(*value->samples.ptr) *
                                                    (value->samples.size + 1));
    if (tmp == NULL) {
        return -1;
    }
    value->samples.ptr = tmp;

    mql_sample_t *sample = &value->samples.ptr[value->samples.size];
    value->samples.size++;
    memset(sample, 0, sizeof(*sample));

    sample->metric.name = strdup(src_sample->metric.name);
    label_set_clone(&sample->metric.labels, src_sample->metric.labels);

    sample->point.timestamp = src_sample->point.timestamp;
    sample->point.value = src_sample->point.value;

    return 0;
}

mql_value_t *mql_value_scalar(int64_t timestamp, double scalar)
{
    mql_value_t *value = mql_value(MQL_VALUE_SCALAR);
    if (value == NULL)
        return NULL;

    value->scalar.timestamp = timestamp;
    value->scalar.value = scalar;
    return value;
}

int mql_value_dump(mql_value_t *value)
{
    switch (value->kind) {
        case MQL_VALUE_NONE:
            break;
        case MQL_VALUE_SERIES:
            break;
        case MQL_VALUE_SAMPLES:
            break;
        case MQL_VALUE_SCALAR:
            fprintf(stderr,"scalar: %f\n", value->scalar.value);
            break;
    }
    return 0;
}
