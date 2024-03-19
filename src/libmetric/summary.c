// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

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
