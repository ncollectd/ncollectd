// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libutils/pack.h"
#include "libmetric/summary.h"

void summary_destroy(summary_t *s)
{
    if (s == NULL)
        return;
    free(s);
}

summary_t *summary_clone(summary_t *s)
{
    if (s == NULL) {
        errno = EINVAL;
        return NULL;
    }

    summary_t *ns = malloc(sizeof(summary_t)+sizeof(summary_quantile_t)*s->num);
    if (ns == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    memcpy(ns, s, sizeof(summary_t)+sizeof(summary_quantile_t)*s->num);
    return ns;
}

summary_t *summary_new(void)
{
    summary_t *s = calloc(1, sizeof(summary_t));
    if (s == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    return s;
}

static int summary_quantile_cmp(void const *a, void const *b)
{
    const summary_quantile_t *quantile_a = a;
    const summary_quantile_t *quantile_b = b;
    return quantile_a->quantile > quantile_b->quantile;
}

summary_t *summary_quantile_append(summary_t *s, double quantile, double value)
{
    size_t num = s->num;

    summary_t *ns = realloc(s, sizeof(summary_t)+sizeof(summary_quantile_t)*(num + 1));
    if (ns == NULL) {
        errno = ENOMEM;
        return s;
    }

    ns->quantiles[num].quantile = quantile;
    ns->quantiles[num].value = value;
    ns->num++;

    qsort(ns->quantiles, ns->num, sizeof(*ns->quantiles), summary_quantile_cmp);

    return ns;
}

enum {
    SUMMARY_QUANTILE_ID = 0x10,
    SUMMARY_VALUE_ID    = 0x20
};

enum {
    SUMMARY_SUM_ID       = 0x10,
    SUMMARY_COUNT_ID     = 0x20,
    SUMMARY_QUANTILES_ID = 0x30
};

int summary_pack(buf_t *buf, uint8_t id, summary_t *s)
{
    size_t begin = 0;

    int status = pack_id(buf, id);
    status |= pack_block_begin(buf, &begin);
    status |= pack_double(buf, SUMMARY_SUM_ID, s->sum);
    status |= pack_uint64(buf, SUMMARY_COUNT_ID, s->count);
    status |= pack_size(buf, SUMMARY_QUANTILES_ID, s->num);

    for (size_t i = 0; i < s->num; i++) {
        summary_quantile_t *q = &s->quantiles[i];

        size_t quantile_begin = 0;
        status |= pack_block_begin(buf, &quantile_begin);
        status |= pack_double(buf, SUMMARY_QUANTILE_ID, q->quantile);
        status |= pack_double(buf, SUMMARY_VALUE_ID, q->value);
        status |= pack_block_end(buf, quantile_begin);
    }

    status |= pack_block_end(buf, begin);

    return status;
}

static int summary_quantiles_unpack(rbuf_t *rbuf, uint8_t id, summary_t **sum)
{
    size_t len = 0;
    int status = unpack_size(rbuf, id, &len);
    if (status != 0)
        return status;

    for (size_t i = 0; i < len; i++) {
        rbuf_t srbuf = {0};
        status = unpack_block(rbuf, &srbuf);
        if (status != 0)
            return 1;

        double quantile = 0;
        status |= unpack_id_double(&srbuf, SUMMARY_QUANTILE_ID, &quantile);
        if (status != 0)
            return 1;

        double value = 0;
        status |= unpack_id_double(&srbuf, SUMMARY_VALUE_ID, &value);
        if (status != 0)
            return 1;

        *sum = summary_quantile_append(*sum, quantile, value);

        unpack_avance_block(rbuf, &srbuf);
    }

    return 0;
}

int summary_unpack(rbuf_t *rbuf, summary_t **s)
{
    rbuf_t srbuf = {0};
    int status = unpack_block(rbuf, &srbuf);
    if (status != 0)
        return 1;

    summary_t *sum = summary_new();
    if (sum == NULL)
        return 1;

    while (true) {
        uint8_t id = 0;

        status = unpack_id(&srbuf, &id);
        switch(id & 0xf0) {
        case SUMMARY_SUM_ID:
            status = unpack_double(&srbuf, &sum->sum);
            break;
        case SUMMARY_COUNT_ID:
            status = unpack_uint64(&srbuf, &sum->count);
            break;
        case SUMMARY_QUANTILES_ID:
            status = summary_quantiles_unpack(&srbuf, id, &sum);
            break;
        }

        if (status != 0)
            break;

        if (rbuf_remain(&srbuf) == 0)
            break;
    }

    if (status != 0) {
        summary_destroy(sum);
        return -1;
    }

    *s = sum;

    unpack_avance_block(rbuf, &srbuf);

    return 0;
}
