// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "log.h"
#include "libutils/time.h"
#include "libutils/avltree.h"
#include "libmetric/metric.h"
#include "libmetric/label_set.h"

typedef struct {
    char *name;
    label_set_t labels;
    cdtime_t time;
    counter_t counter;
} rate_entry_t;

typedef struct {
    c_avl_tree_t *tree;
} rate_t;

static void rate_entry_free(void *ptr)
{
    if (ptr == NULL)
        return;

    rate_entry_t *re = ptr;

    free(re->name);
    label_set_reset(&re->labels);
    free(re);
}

static int rate_entry_compare(const rate_entry_t *a, const rate_entry_t *b)
{
    int cmp = strcmp(a->name, b->name);
    if (cmp != 0)
        return cmp;

    if (a->labels.num != b->labels.num)
        return a->labels.num - b->labels.num;

    for (size_t i=0 ; i < a->labels.num; i++) {
        cmp = strcmp(a->labels.ptr[i].name, b->labels.ptr[i].name);
        if (cmp != 0)
            return cmp;

        cmp = strcmp(a->labels.ptr[i].value, b->labels.ptr[i].value);
        if (cmp != 0)
            return cmp;
    }

    return 0;
}

rate_t *rate_alloc(void)
{
    rate_t *rate = calloc(1, sizeof(*rate));
    if (rate == NULL)
        return NULL;

    rate->tree = c_avl_create((int (*)(const void *, const void *))rate_entry_compare);
    if (rate->tree == NULL) {
        free(rate);
        return NULL;
    }

    return rate;
}

void rate_free(rate_t *rate)
{
    if (rate == NULL)
        return;

    if (rate->tree != NULL) {
        while (true) {
            rate_entry_t *dre = NULL;
            rate_entry_t *kre = NULL;
            int status = c_avl_pick(rate->tree, (void *)&kre, (void *)&dre);
            if (status != 0)
                break;
            rate_entry_free(dre);
        }
        c_avl_destroy(rate->tree);
        rate->tree = NULL;
    }

    free(rate);
}

static uint64_t counter_uint64_diff(uint64_t old_value, uint64_t new_value)
{
    uint64_t diff;

    if (old_value > new_value) {
        if (old_value <= 4294967295U)
            diff = (4294967295U - old_value) + new_value + 1;
        else
            diff = (18446744073709551615ULL - old_value) + new_value + 1;
    } else {
        diff = new_value - old_value;
    }

    return diff;
}

static double counter_float64_diff(double old_value, double new_value)
{
    double diff;

    if (old_value > new_value) {
        // TODO
        diff = new_value;
    } else {
        diff = new_value - old_value;
    }

    return diff;
}

int rate_get(rate_t *rate, char *name, metric_t *m, double *rrate)
{
    if ((rate == NULL) || (name == NULL) || (m == NULL) || (rrate == NULL))
        return -1;

    rate_entry_t *fre = NULL;

    rate_entry_t sre = {0};
    sre.name = name;
    sre.labels.num = m->label.num;
    sre.labels.ptr = m->label.ptr;

    if (c_avl_get(rate->tree, &sre, (void *)&fre) == 0) {
        if (fre == NULL)
            return -1;

        if (fre->time > m->time) {
            NOTICE("Value too old: name = %s; time = %.3f; last update = %.3f;",
                   name, CDTIME_T_TO_DOUBLE(m->time), CDTIME_T_TO_DOUBLE(fre->time));
            return -1;
        }

        if (fre->counter.type != m->value.counter.type) {
            NOTICE("Different counter types");
            return -1;
        }

        double time_diff = CDTIME_T_TO_DOUBLE(m->time - fre->time);

        switch(m->value.counter.type) {
        case COUNTER_UINT64: {
            uint64_t diff = counter_uint64_diff(fre->counter.uint64, m->value.counter.uint64);
            *rrate = (double)diff / time_diff;
        }   break;
        case COUNTER_FLOAT64: {
            double diff = counter_float64_diff(fre->counter.float64, m->value.counter.float64);
            *rrate = diff / time_diff;
        }   break;
        }

        fre->counter = m->value.counter;
        fre->time = m->time;

        return 1;
    }

    rate_entry_t *re = calloc (1, sizeof(*re));
    if (re == NULL) {
        ERROR("calloc failed.");
        return -1;
    }

    re->name = strdup(name);
    if (re->name != NULL) {
        rate_entry_free(re);
        return -1;
    }

    int status = label_set_clone(&re->labels, m->label);
    if (status != 0) {
        rate_entry_free(re);
        return -1;
    }

    fre->counter = m->value.counter;
    fre->time = m->time;

    if (c_avl_insert(rate->tree, re, re) != 0) {
        rate_entry_free(re);
        ERROR("c_avl_insert failed.");
        return -1;
    }

    return 0;
}
