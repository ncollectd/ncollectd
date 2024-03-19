/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "libmetric/metric.h"

typedef enum {
    FMT_OPENTSDB_SEC,
    FMT_OPENTSDB_MSEC,
} fmt_opentsdb_t;

int opentsdb_telnet_metric(strbuf_t *buf, metric_family_t const *fam, metric_t const *m,
                                          int ttl, fmt_opentsdb_t resolution);

int opentsdb_telnet_metric_family(strbuf_t *buf, metric_family_t const *fam, int ttl,
                                                 fmt_opentsdb_t resolution);
