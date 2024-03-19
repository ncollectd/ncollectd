/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "libmetric/metric.h"

struct rate_s;
typedef struct rate_s rate_t;

rate_t *rate_alloc(void);

void rate_free(rate_t *rate);

int rate_get(rate_t *rate, char *name, metric_t *m, double *rrate);
