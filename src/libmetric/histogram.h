/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "ncollectd.h"

typedef struct {
    uint64_t counter;
    double maximum;
} histogram_bucket_t;

typedef struct {
    double sum;
    size_t num;
    histogram_bucket_t buckets[];
} histogram_t;

histogram_t *histogram_new(void);

histogram_t *histogram_new_linear(size_t num_buckets, double size);

histogram_t *histogram_new_exp(size_t num_buckets, double base, double factor);

histogram_t *histogram_new_custom(size_t array_size, double *custom_buckets_boundaries);

histogram_t *histogram_clone(histogram_t *h);

int histogram_update(histogram_t *h, double gauge);

void histogram_destroy(histogram_t *h);

int histogram_reset(histogram_t *h);

static inline double histogram_average(histogram_t *h)
{
    uint64_t count = h->num > 0 ? h->buckets[0].counter : 0;

    if (count == 0)
        return NAN;

    return h->sum / (double)count;
}

static inline size_t histogram_buckets(histogram_t *h)
{
    return h->num;
}

static inline double histogram_sum(histogram_t *h)
{
    return h->sum;
}

static inline uint64_t histogram_counter(histogram_t *h)
{
    return h->num > 0 ? h->buckets[0].counter : 0;
}

#define HISTOGRAM_DEFAULT_TIME historgram_new_custom(7, (double[]){0.05, 0.1, 0.2, 0.5, 1, 10, 100})

histogram_t *histogram_bucket_append(histogram_t *h, double maximum, uint64_t counter);
