// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2013 Florian Forster
// SPDX-FileContributor: Florian Forster <octo at collectd.org>

#include "ncollectd.h"
#include "libutils/random.h"
#include "libutils/time.h"

#include <pthread.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static bool have_seed;
static unsigned short seed[3];

static void cdrand_seed(void)
{
    cdtime_t t;

    if (have_seed)
        return;

    t = cdtime();

    seed[0] = (unsigned short)t;
    seed[1] = (unsigned short)(t >> 16);
    seed[2] = (unsigned short)(t >> 32);

    have_seed = true;
}

double cdrand_d(void)
{
    double r;

    pthread_mutex_lock(&lock);
    cdrand_seed();
    /* coverity[DC.WEAK_CRYPTO] */
    r = erand48(seed);
    pthread_mutex_unlock(&lock);

    return r;
}

uint32_t cdrand_u(void)
{
    long r;

    pthread_mutex_lock(&lock);
    cdrand_seed();
    /* coverity[DC.WEAK_CRYPTO] */
    r = jrand48(seed);
    pthread_mutex_unlock(&lock);

    return (uint32_t)r;
}

long cdrand_range(long min, long max)
{
    long range;
    long r;

    range = 1 + max - min;

    r = (long)(0.5 + (cdrand_d() * range));
    r += min;

    return r;
}

void cdrand(uint8_t *dst, size_t size)
{
    size_t remain = size % sizeof(uint32_t);

    pthread_mutex_lock(&lock);
    cdrand_seed();

    size_t rsize = size - remain;
    size_t n = 0;
    while (n < rsize) {
        /* coverity[DC.WEAK_CRYPTO] */
        uint32_t r = (uint32_t) jrand48(seed);
        memcpy(dst, &r, sizeof(r));
        dst += sizeof(r);
        n += sizeof(r);
    }

    if (remain > 0) {
        /* coverity[DC.WEAK_CRYPTO] */
        uint32_t r = (uint32_t) jrand48(seed);
        memcpy(dst, &r, remain);
    }

    pthread_mutex_unlock(&lock);
}
