// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) Tobias Waldekranz
// SPDX-FileContributor: Tobias Waldekranz <tobias at waldekranz.com>

// From https://github.com/wkz/ply

#pragma GCC diagnostic ignored "-Wunused-result"

#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ply.h"
#include "internal.h"

int ply_debug;

static void strkill(char *str, char kill)
{
    char *r, *w;

    for (r = w = str; *r; r++) {
        if (*r == kill)
            continue;

        *w++ = *r;
    }

    *w = '\0';
}

int strtonum(const char *_str, int64_t *s64, uint64_t *u64)
{
    char *str = strdup(_str);

    strkill(str, '_');

    errno = 0;
    if (*str == '-') {
        *s64 = strtoll(str, NULL, 0);
        if (!errno) {
            free(str);
            return -1;  
        }
    } else if (strstr(str, "0b") == str) {
        *u64 = strtoull(&str[2], NULL, 2);
        if (!errno) {
            free(str);
            return 1;
        }
    } else {
        *u64 = strtoull(str, NULL, 0);
        if (!errno) {
            free(str);
            return 1;
        }
    }

    free(str);

    return 0;
}

int isstring(const char *data, size_t len)
{
    size_t i;

    /* All characters up to a '\0' must be printable. */
    for (i = 0; (i < (len - 1)) && data[i]; i++)
        if (!isprint(data[i]))
            return 0;

    /* There must be at least one '\0', after which only more
     * '\0's may follow. */
    for (; i < len; i++)
        if (data[i])
            return 0;

    return 1;
}

FILE *fopenf(const char *mode, const char *fmt, ...)
{
    va_list ap;
    FILE *fp;
    char *path;

    va_start(ap, fmt);
    vasprintf(&path, fmt, ap);
    va_end(ap);

    fp = fopen(path, mode);
    free(path);
    return fp;
}

struct ast_fprint_info {
    FILE *fp;
    int indent;
};

static int __ast_fprint_pre(struct node *n, void *_info)
{
    struct ast_fprint_info *info = _info;

    fprintxf(NULL, info->fp, "%*s%N", info->indent, "", n);

    if (n->sym && n->sym->type)
        fprintxf(NULL, info->fp, "%T", n->sym->type);

    fputc('\n', info->fp);

    if (n->ntype == N_EXPR)
        info->indent += 4;

    return 0;
}

static int __ast_fprint_post(struct node *n, void *_info)
{
    struct ast_fprint_info *info = _info;

    if (n->ntype == N_EXPR)
        info->indent -= 4;

    return 0;
}

void ast_fprint(FILE *fp, struct node *root)
{
    struct ast_fprint_info info = {
        .fp = fp,
    };

    node_walk(root, __ast_fprint_pre, __ast_fprint_post, &info);
    fputc('\n', fp);        
}

char *strorder(int n, char *buf, size_t buflen)
{
    switch (n) {
    case 1:
        snprintf(buf, buflen, "1st");
        break;
    case 2:
        snprintf(buf, buflen, "2nd");
        break;
    case 3:
        snprintf(buf, buflen, "3rd");
        break;
    default:
        snprintf(buf, buflen, "%dth", n);
        break;
    }

    return buf;
}
