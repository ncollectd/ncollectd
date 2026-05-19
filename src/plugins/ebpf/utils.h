/* SPDX-License-Identifier: GPL-2.0-only                              */
/* SPDX-FileCopyrightText: Copyright (C) Tobias Waldekranz            */
/* SPDX-FileContributor: Tobias Waldekranz <tobias at waldekranz.com> */

/* From https://github.com/wkz/ply */

#pragma once

#include "plugin.h"

#include <assert.h>

int strtonum(const char *_str, int64_t *s64, uint64_t *u64);
int isstring(const char *data, size_t len);

char *strorder(int n, char *buf, size_t buflen);
#define ORDERBUF_SIZE 16
#define STRORDER(n) strorder((n), (char[ORDERBUF_SIZE]){0}, ORDERBUF_SIZE)

FILE *fopenf(const char *mode, const char *fmt, ...)
    __attribute__ ((format (printf, 2, 3)));

void ast_fprint(FILE *fp, struct node *root);

/* This variable controls debug output for non-DEBUG build. */
extern int ply_debug;

#include "printxf.h"

#define container_of(ptr, type, member) __extension__ ({ \
    typeof( ((type *)0)->member ) *__mptr = (ptr); \
    (type *)( (char *)__mptr - offsetof(type,member) );})

#define max(a, b) __extension__ \
    ({                    \
        __typeof__ (a) _a = (a);    \
        __typeof__ (b) _b = (b);    \
        _a > _b ? _a : _b;        \
    })

#define min(a, b) __extension__ \
    ({                    \
        __typeof__ (a) _a = (a);    \
        __typeof__ (b) _b = (b);    \
        _a < _b ? _a : _b;        \
    })

static inline void *xcalloc(size_t nmemb, size_t size)
{
    void *mem = calloc(nmemb, size);

    assert(mem);
    return mem;
}

