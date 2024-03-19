// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libmdb/table.h"

typedef struct {
    char *ptr;
    size_t len;
} table_char_t;

#define TABLE_CHAR(x) {.ptr = (x), .len = sizeof(x)-1}

typedef struct {
    table_char_t xhl;
    table_char_t ihl;
    table_char_t xvl;
    table_char_t ivl;
    table_char_t crt;
    table_char_t clt;
    table_char_t crb;
    table_char_t clb;
    table_char_t xtl;
    table_char_t xtr;
    table_char_t xtt;
    table_char_t xtb;
    table_char_t ix;
} table_style_t;

static table_style_t table_styles[] = {
    [TABLE_STYLE_SIMPLE] = {
        .xhl = TABLE_CHAR("─"),
        .ihl = TABLE_CHAR("─"),
        .xvl = TABLE_CHAR("│"),
        .ivl = TABLE_CHAR("│"),
        .crt = TABLE_CHAR("┌"),
        .clt = TABLE_CHAR("┐"),
        .crb = TABLE_CHAR("└"),
        .clb = TABLE_CHAR("┘"),
        .xtl = TABLE_CHAR("├"),
        .xtr = TABLE_CHAR("┤"),
        .xtt = TABLE_CHAR("┬"),
        .xtb = TABLE_CHAR("┴"),
        .ix  = TABLE_CHAR("┼"),
    },
    [TABLE_STYLE_BOLD] = {
        .xhl = TABLE_CHAR("━"),
        .ihl = TABLE_CHAR("━"),
        .xvl = TABLE_CHAR("┃"),
        .ivl = TABLE_CHAR("┃"),
        .crt = TABLE_CHAR("┏"),
        .clt = TABLE_CHAR("┓"),
        .crb = TABLE_CHAR("┗"),
        .clb = TABLE_CHAR("┛"),
        .xtl = TABLE_CHAR("┣"),
        .xtr = TABLE_CHAR("┫"),
        .xtt = TABLE_CHAR("┳"),
        .xtb = TABLE_CHAR("┻"),
        .ix  = TABLE_CHAR("╋"),
    },
    [TABLE_STYLE_BORDER_BOLD] = {
        .xhl = TABLE_CHAR("━"),
        .ihl = TABLE_CHAR("─"),
        .xvl = TABLE_CHAR("┃"),
        .ivl = TABLE_CHAR("│"),
        .crt = TABLE_CHAR("┏"),
        .clt = TABLE_CHAR("┓"),
        .crb = TABLE_CHAR("┗"),
        .clb = TABLE_CHAR("┛"),
        .xtl = TABLE_CHAR("┠"),
        .xtr = TABLE_CHAR("┨"),
        .xtt = TABLE_CHAR("┯"),
        .xtb = TABLE_CHAR("┷"),
        .ix  = TABLE_CHAR("┼"),
    },
    [TABLE_STYLE_DOUBLE] = {
        .xhl = TABLE_CHAR("═"),
        .ihl = TABLE_CHAR("═"),
        .xvl = TABLE_CHAR("║"),
        .ivl = TABLE_CHAR("║"),
        .crt = TABLE_CHAR("╔"),
        .clt = TABLE_CHAR("╗"),
        .crb = TABLE_CHAR("╚"),
        .clb = TABLE_CHAR("╝"),
        .xtl = TABLE_CHAR("╠"),
        .xtr = TABLE_CHAR("╣"),
        .xtt = TABLE_CHAR("╦"),
        .xtb = TABLE_CHAR("╩"),
        .ix  = TABLE_CHAR("╬"),
    },
    [TABLE_STYLE_BORDER_DOUBLE] = {
        .xhl = TABLE_CHAR("═"),
        .ihl = TABLE_CHAR("─"),
        .xvl = TABLE_CHAR("║"),
        .ivl = TABLE_CHAR("│"),
        .crt = TABLE_CHAR("╔"),
        .clt = TABLE_CHAR("╗"),
        .crb = TABLE_CHAR("╚"),
        .clb = TABLE_CHAR("╝"),
        .xtl = TABLE_CHAR("╟"),
        .xtr = TABLE_CHAR("╢"),
        .xtt = TABLE_CHAR("╤"),
        .xtb = TABLE_CHAR("╧"),
        .ix  = TABLE_CHAR("┼"),
    },
    [TABLE_STYLE_ROUND] = {
        .xhl = TABLE_CHAR("─"),
        .ihl = TABLE_CHAR("─"),
        .xvl = TABLE_CHAR("│"),
        .ivl = TABLE_CHAR("│"),
        .crt = TABLE_CHAR("╭"),
        .clt = TABLE_CHAR("╮"),
        .crb = TABLE_CHAR("╰"),
        .clb = TABLE_CHAR("╯"),
        .xtl = TABLE_CHAR("├"),
        .xtr = TABLE_CHAR("┤"),
        .xtt = TABLE_CHAR("┬"),
        .xtb = TABLE_CHAR("┴"),
        .ix  = TABLE_CHAR("┼"),
    },
    [TABLE_STYLE_ASCII] = {
        .xhl = TABLE_CHAR("-"),
        .ihl = TABLE_CHAR("-"),
        .xvl = TABLE_CHAR("|"),
        .ivl = TABLE_CHAR("|"),
        .crt = TABLE_CHAR("+"),
        .clt = TABLE_CHAR("+"),
        .crb = TABLE_CHAR("+"),
        .clb = TABLE_CHAR("+"),
        .xtl = TABLE_CHAR("+"),
        .xtr = TABLE_CHAR("+"),
        .xtt = TABLE_CHAR("+"),
        .xtb = TABLE_CHAR("+"),
        .ix  = TABLE_CHAR("+"),
    },
};

void table_init(table_t *tbl, strbuf_t *buf, table_style_type_t style, size_t *col_size, size_t ncols, size_t spc)
{
    tbl->buf = buf;
    tbl->style = style;
    tbl->col = 0;
    tbl->col_size = col_size;
    tbl->ncols = ncols;
    tbl->spc = spc;
}

int table_begin(table_t *tbl)
{
    table_style_t *style = &table_styles[tbl->style];

    int status = strbuf_putstrn(tbl->buf, style->crt.ptr, style->crt.len);

    for (size_t i = 0; i < tbl->ncols; i++) {
        if (i > 0)
            status |= strbuf_putstrn(tbl->buf, style->xtt.ptr, style->xtt.len);

        size_t spaces = tbl->spc + tbl->col_size[i] + tbl->spc;
        status |= strbuf_putxstrn(tbl->buf, style->xhl.ptr, style->xhl.len, spaces);
    }

    status |= strbuf_putstrn(tbl->buf, style->clt.ptr, style->clt.len);
    status |= strbuf_putchar(tbl->buf, '\n');
    return status;
}

int table_header_begin(table_t *tbl)
{
    table_style_t *style = &table_styles[tbl->style];
    tbl->col = 0;
    return strbuf_putstrn(tbl->buf, style->xvl.ptr, style->xvl.len);
}

int table_header_cell(table_t *tbl, const char *str)
{
    table_style_t *style = &table_styles[tbl->style];

    int status = 0;
    if (tbl->col > 0)
        status |= strbuf_putstrn(tbl->buf, style->ivl.ptr, style->ivl.len);
    if (tbl->spc > 0)
        status |= strbuf_putxchar(tbl->buf, ' ', tbl->spc);

    size_t str_len = 0;
    if (str != NULL) {
        str_len = strlen(str);
        status |= strbuf_putstrn(tbl->buf, str, str_len);
    }

    size_t spaces = tbl->col_size[tbl->col] - str_len + tbl->spc;
    if (spaces > 0)
        status |= strbuf_putxchar(tbl->buf, ' ', spaces);

    tbl->col++;

    return status;
}

int table_header_end(table_t *tbl)
{
    table_style_t *style = &table_styles[tbl->style];
    int status = strbuf_putstrn(tbl->buf, style->xvl.ptr, style->xvl.len);
    status |= strbuf_putchar(tbl->buf, '\n');

    status |= strbuf_putstrn(tbl->buf, style->xtl.ptr, style->xtl.len);

    for (size_t i = 0; i < tbl->ncols; i++) {
        if (i > 0)
            status |= strbuf_putstrn(tbl->buf, style->ix.ptr, style->ix.len);
        size_t spaces = tbl->spc + tbl->col_size[i] + tbl->spc;
        status |= strbuf_putxstrn(tbl->buf, style->ihl.ptr, style->ihl.len, spaces);
    }

    status |= strbuf_putstrn(tbl->buf, style->xtr.ptr, style->xtr.len);
    status |= strbuf_putchar(tbl->buf, '\n');

    return status;
}

int table_row_begin(table_t *tbl)
{
    table_style_t *style = &table_styles[tbl->style];
    tbl->col = 0;
    int status = 0;
#if 0
    status |= strbuf_putstrn(tbl->buf, style->xtl.ptr, style->xtl.len);

    for (size_t i = 0; i < tbl->ncols; i++) {
        if (i > 0)
            status |= strbuf_putstrn(tbl->buf, style->ix.ptr, style->ix.len);
        size_t spaces = tbl->spc + tbl->col_size[i] + tbl->spc;
        status |= strbuf_putxstrn(tbl->buf, style->ihl.ptr, style->ihl.len, spaces);
    }

    status |= strbuf_putstrn(tbl->buf, style->xtr.ptr, style->xtr.len);
    status |= strbuf_putchar(tbl->buf, '\n');
#endif
    status |= strbuf_putstrn(tbl->buf, style->xvl.ptr, style->xvl.len);
    return status;
}

int table_row_cell(table_t *tbl, const char *str)
{
    table_style_t *style = &table_styles[tbl->style];
    int status = 0;
    if (tbl->col > 0)
        status |= strbuf_putstrn(tbl->buf, style->ivl.ptr, style->ivl.len);
    if (tbl->spc > 0)
        status |= strbuf_putxchar(tbl->buf, ' ', tbl->spc);

    size_t str_len = 0;
    if (str != NULL) {
        str_len = strlen(str);
        status |= strbuf_putstrn(tbl->buf, str, str_len);
    }

    size_t spaces = tbl->col_size[tbl->col] - str_len + tbl->spc;
    if (spaces > 0)
        status |= strbuf_putxchar(tbl->buf, ' ', spaces);

    tbl->col++;

    return status;
}

int table_row_end(table_t *tbl)
{
    table_style_t *style = &table_styles[tbl->style];
    int status = strbuf_putstrn(tbl->buf, style->xvl.ptr, style->xvl.len);
    status |= strbuf_putchar(tbl->buf, '\n');
    return status;
}

int table_table_end(table_t *tbl)
{
    table_style_t *style = &table_styles[tbl->style];

    int status = strbuf_putstrn(tbl->buf, style->crb.ptr, style->crb.len);
    for (size_t i = 0; i < tbl->ncols; i++) {
        if (i > 0)
            status |= strbuf_putstrn(tbl->buf, style->xtb.ptr, style->xtb.len);
        size_t spaces = tbl->spc + tbl->col_size[i] + tbl->spc;
        status |= strbuf_putxstrn(tbl->buf, style->xhl.ptr, style->xhl.len, spaces);
    }

    status |= strbuf_putstrn(tbl->buf, style->clb.ptr, style->clb.len);
    status |= strbuf_putchar(tbl->buf, '\n');
    return status;
}
