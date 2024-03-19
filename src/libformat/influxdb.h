/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

typedef enum {
    FMT_INFLUXDB_SEC,
    FMT_INFLUXDB_MSEC,
    FMT_INFLUXDB_USEC,
    FMT_INFLUXDB_NSEC,
} fmt_influxdb_t;

int influxdb_metric(strbuf_t *buf, metric_family_t const *fam, metric_t const *m,
                                   fmt_influxdb_t resolution);

int influxdb_metric_family(strbuf_t *buf, metric_family_t const *fam, fmt_influxdb_t resolution);
