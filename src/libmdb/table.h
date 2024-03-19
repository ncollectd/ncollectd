/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "libutils/strbuf.h"

typedef enum {
    TABLE_STYLE_SIMPLE,
    TABLE_STYLE_BOLD,
    TABLE_STYLE_BORDER_BOLD,
    TABLE_STYLE_DOUBLE,
    TABLE_STYLE_BORDER_DOUBLE,
    TABLE_STYLE_ROUND,
    TABLE_STYLE_ASCII
} table_style_type_t;

typedef struct {
    strbuf_t *buf;
    table_style_type_t style;
    size_t col;
    size_t *col_size;
    size_t ncols;
    size_t spc;
} table_t;

void table_init(table_t *tbl, strbuf_t *buf, table_style_type_t style,
                              size_t *col_size, size_t ncols, size_t spc);

int table_begin(table_t *tbl);

int table_header_begin(table_t *tbl);

int table_header_cell(table_t *tbl, const char *str);

int table_header_end(table_t *tbl);

int table_row_begin(table_t *tbl);

int table_row_cell(table_t *tbl, const char *str);

int table_row_end(table_t *tbl);

int table_table_end(table_t *tbl);
