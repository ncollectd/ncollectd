/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "ncollectd.h"

typedef struct {
    double quantile;
    double value;
} summary_quantile_t;

typedef struct summary {
    double sum;
    uint64_t count;
    size_t num;
    summary_quantile_t quantiles[];
} summary_t;

summary_t *summary_clone(summary_t *s);

void summary_destroy(summary_t *s);

summary_t *summary_new(void);

summary_t *summary_quantile_append(summary_t *s, double quantile, double value);
