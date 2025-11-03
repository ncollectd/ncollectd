/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "libutils/strbuf.h"
#include "libmetric/metric.h"

typedef struct metric_parser_s metric_parser_t;

metric_parser_t *metric_parser_alloc(char *metric_prefix, label_set_t *labels);

void metric_parser_free(metric_parser_t *mp);

void metric_parser_reset(metric_parser_t *mp);

int metric_parse_line(metric_parser_t *mp, const char *line);

int metric_parse_buffer(metric_parser_t *mp, char *buffer, size_t buffer_len);

typedef int (*dispatch_metric_family_t)(metric_family_t *fam, plugin_filter_t *filter,
                                        cdtime_t time);

int metric_parser_dispatch(metric_parser_t *mp, dispatch_metric_family_t dispatch,
                                                plugin_filter_t *filter, cdtime_t time);
