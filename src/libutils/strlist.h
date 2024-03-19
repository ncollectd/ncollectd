/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "ncollectd.h"
#include <string.h>

typedef struct {
    size_t alloc;
    size_t size;
    char **ptr;
} strlist_t;

static inline size_t strlist_size(strlist_t const *sl)
{
    return sl->size;
}

static inline size_t strlist_avail(strlist_t const *sl)
{
    return sl->alloc ? sl->alloc - sl->size : 0;
}

strlist_t *strlist_alloc(size_t size);

int strlist_resize(strlist_t *sl, size_t need);

int strlist_nappend(strlist_t *sl, const char *str, size_t size);

static inline int strlist_append(strlist_t *sl, const char *str)
{
    return strlist_nappend(sl, str, strlen(str));
}

void strlist_reset(strlist_t *sl);

void strlist_destroy(strlist_t *sl);

void strlist_free(strlist_t *sl);
