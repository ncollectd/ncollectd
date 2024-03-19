/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "libmetric/metric.h"
#include "libmdb/table.h"

typedef struct {
    char *name;
    char *unit;
    char *help;
    metric_type_t type;
} mdb_family_metric_t;

typedef struct {
    size_t num;
    mdb_family_metric_t *ptr;
} mdb_family_metric_list_t;

void mdb_family_metric_list_free(mdb_family_metric_list_t *list);

int mdb_family_metric_list_append(mdb_family_metric_list_t *list, char *name, metric_type_t type,
                                                                char *unit, char *help);

mdb_family_metric_list_t *mdb_family_metric_list_parse(const char *data, size_t len);

int mdb_family_metric_list_to_json(mdb_family_metric_list_t *list, strbuf_t *buf, bool pretty);

int mdb_family_metric_list_to_yaml(mdb_family_metric_list_t *list, strbuf_t *buf);

int mdb_family_metric_list_to_text(mdb_family_metric_list_t *list, strbuf_t *buf);

int mdb_family_metric_list_to_table(mdb_family_metric_list_t *list, table_style_type_t style, strbuf_t *buf);
