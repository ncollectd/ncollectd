// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include <stdlib.h>
#include "libutils/strlist.h"

int strlist_resize(strlist_t *sl, size_t need)
{
    if (strlist_avail(sl) > need)
        return 0;

    size_t new_alloc = sl->alloc == 0 ? 16 : 2 * sl->alloc;
    if (new_alloc < (sl->size + need))
        new_alloc = sl->size + need;

    char **new_ptr = realloc(sl->ptr, new_alloc * sizeof(char *));
    if (new_ptr == NULL)
        return ENOMEM;

    sl->ptr = new_ptr;
    sl->alloc = new_alloc;
    return 0;
}

strlist_t *strlist_alloc(size_t size)
{
    strlist_t *sl = calloc(1, sizeof(*sl));
    if (sl == NULL) {
        return NULL;
    }

    if (size > 0)
        strlist_resize(sl, size);

    return sl;
}

int strlist_nappend(strlist_t *sl, const char *str, size_t size)
{
    if (unlikely(strlist_avail(sl) < 1)) {
        if (strlist_resize(sl, 1) != 0)
            return ENOMEM;
    }

    char *new_str = strndup(str, size);
    if (unlikely(new_str == NULL))
        return ENOMEM;

    sl->ptr[sl->size] = new_str;
    sl->size++;
    return 0;
}

void strlist_reset(strlist_t *sl)
{
    if (sl == NULL)
        return;

    if (sl->ptr != NULL) {
        for (size_t i = 0; i < sl->size ; i++) {
            free(sl->ptr[i]);
        }
        sl->size = 0;
    }
}

void strlist_destroy(strlist_t *sl)
{
    if (sl == NULL)
        return;

    if (sl->ptr != NULL) {
        for (size_t i = 0; i < sl->size ; i++) {
            free(sl->ptr[i]);
        }
        free (sl->ptr);

        sl->ptr = NULL;
        sl->alloc = 0;
        sl->size = 0;
    }
}

void strlist_free(strlist_t *sl)
{
    if (sl == NULL)
        return;

    strlist_destroy(sl);

    free(sl);
}
