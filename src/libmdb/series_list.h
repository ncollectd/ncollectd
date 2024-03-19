/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "libmetric/label_set.h"
#include "libmdb/table.h"

typedef struct {
    int64_t timestamp;
    double  value;
} mdb_point_t;

typedef struct {
    char *name;
    label_set_t labels;
    size_t num;
    mdb_point_t points[];
} mdb_series_t;

typedef struct {
    size_t num;
    mdb_series_t *ptr;
} mdb_series_list_t;

void mdb_series_list_free(mdb_series_list_t *list);

int mdb_series_list_append(mdb_series_list_t *list, char *name);

mdb_series_list_t *mdb_series_list_parse(const char *data, size_t len);

int mdb_series_list_to_json(mdb_series_list_t *list, strbuf_t *buf, bool pretty);

int mdb_series_list_to_yaml(mdb_series_list_t *list, strbuf_t *buf);

int mdb_series_list_to_text(mdb_series_list_t *list, strbuf_t *buf);

int mdb_series_list_to_table(mdb_series_list_t *list, table_style_type_t style, strbuf_t *buf);
