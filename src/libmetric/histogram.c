// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libmetric/histogram.h"

histogram_t *histogram_new_linear(size_t num_buckets, double size)
{
    if (num_buckets == 0 || size <= 0) {
        errno = EINVAL;
        return NULL;
    }

    num_buckets += 1;
    histogram_t *h = calloc(1, sizeof(histogram_t)+sizeof(histogram_bucket_t)*num_buckets);
    if (h == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    h->num = num_buckets;

    h->buckets[0].maximum = INFINITY;
    for (size_t i=1; i < num_buckets; i++) {
        h->buckets[i].maximum = (num_buckets - i) * size;
    }

    return h;
}

static double histogram_pow(double mantissa, size_t exponent)
{
    if (mantissa == 0) {
        if (exponent == 0)
            return 1;
        else
            return 0;
    }

    double ret = 1;

    while (1) {
        if (exponent & 1)
            ret *= mantissa;
        if ((exponent >>= 1) == 0)
            break;
        mantissa *= mantissa;
    }
    return ret;
}

histogram_t *histogram_new_exp(size_t num_buckets, double base, double factor)
{
    if (num_buckets == 0 || base <= 1 || factor <= 0) {
        errno = EINVAL;
        return NULL;
    }

    num_buckets += 1;
    histogram_t *h = calloc(1, sizeof(histogram_t)+sizeof(histogram_bucket_t)*num_buckets);
    if (h == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    h->num = num_buckets;

    h->buckets[0].maximum = INFINITY;
    for (size_t i=1; i < num_buckets; i++) {
        h->buckets[i].maximum = factor * histogram_pow(base, num_buckets - i);
    }

    return h;
}

histogram_t *histogram_new_custom(size_t array_size, double *custom_buckets_boundaries)
{
    for (size_t i = 0; i < array_size; i++) {
        double previous_boundary = 0;
        if (i > 0)
            previous_boundary = custom_buckets_boundaries[i - 1];
        if (custom_buckets_boundaries[i] <= previous_boundary) {
            errno = EINVAL;
            return NULL;
        }
    }

    if (array_size > 0 && custom_buckets_boundaries[array_size - 1] == INFINITY) {
        errno = EINVAL;
        return NULL;
    }

    size_t num_buckets = array_size + 1;
    histogram_t *h = calloc(1, sizeof(histogram_t)+sizeof(histogram_bucket_t)*num_buckets);
    if (h == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    h->num = num_buckets;

    h->buckets[0].maximum = INFINITY;
    for (size_t i=1; i < num_buckets; i++) {
        h->buckets[i].maximum = custom_buckets_boundaries[array_size - i];
    }

    return h;
}

void histogram_destroy(histogram_t *h)
{
    if (h == NULL)
        return;

    free(h);
}

histogram_t *histogram_clone(histogram_t *h)
{
    if (h == NULL) {
        errno = EINVAL;
        return NULL;
    }

    histogram_t *nh = malloc(sizeof(histogram_t) + sizeof(histogram_bucket_t)*h->num);
    if (nh == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    memcpy(nh, h, sizeof(histogram_t) + sizeof(histogram_bucket_t)*h->num);

    return nh;
}

int histogram_update(histogram_t *h, double gauge)
{
    if (h == NULL || gauge < 0)
        return EINVAL;

    h->buckets[0].counter++;
    for (size_t i=1; i < h->num; i++) {
        if (h->buckets[i].maximum < gauge)
            break;
        h->buckets[i].counter++;
    }
    h->sum += gauge;

    return 0;
}

int histogram_reset(histogram_t *h)
{
    if (h == NULL)
        return EINVAL;

    h->sum = 0;
    for (size_t i = 0; i < h->num; i++) {
        h->buckets[i].counter = 0;
    }

    return 0;
}

histogram_t *histogram_new(void)
{
    histogram_t *h = calloc(1, sizeof(histogram_t)+sizeof(histogram_bucket_t));
    if (h == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    h->num = 1;
    h->buckets[0].maximum = INFINITY;

    return h;
}

static int histogram_bucket_cmp(void const *a, void const *b)
{
    const histogram_bucket_t *bucket_a = a;
    const histogram_bucket_t *bucket_b = b;
    return bucket_a->maximum < bucket_b->maximum;
}

histogram_t *histogram_bucket_append(histogram_t *h, double maximum, uint64_t counter)
{
    if (maximum == INFINITY) {
        h->buckets[0].counter = counter;
        return h;
    }

    size_t num = h->num;

    histogram_t *nh = realloc(h, sizeof(histogram_t)+sizeof(histogram_bucket_t)*(num + 1));
    if (nh == NULL) {
        errno = ENOMEM;
        return h;
    }

    nh->buckets[num].maximum = maximum;
    nh->buckets[num].counter = counter;
    nh->num++;

    qsort(nh->buckets + 1, nh->num - 1, sizeof(*nh->buckets), histogram_bucket_cmp);

    return nh;
}

