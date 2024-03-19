// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "libmetric/state_set.h"

static const char valid_label_chars[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static inline bool state_check_name (const char *str)
{
    unsigned const char *s = (unsigned const char *)str;

    if (valid_label_chars[s[0]] != 1)
        return false;

    for (unsigned const char *c = s+1; *c != '\0' ; c++) {
        if (valid_label_chars[*c] == 0)
            return false;
    }

    return true;
}

static int state_compare(void const *a, void const *b)
{
    return strcmp(((state_t const *)a)->name, ((state_t const *)b)->name);
}

state_t *state_set_read(state_set_t set, const char *name)
{
    if (name == NULL) {
        errno = EINVAL;
        return NULL;
    }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
    state_t state = (state_t) {.name = name };
#pragma GCC diagnostic pop

    state_t *ret = bsearch(&state, set.ptr, set.num, sizeof(*set.ptr), state_compare);
    if (ret == NULL) {
        errno = ENOENT;
        return NULL;
    }

    return ret;
}

int state_set_create(state_set_t *set, char const *name, bool enabled)
{
    if ((set == NULL) || (name == NULL))
        return EINVAL;

    if (!state_check_name(name))
        return EINVAL;

    if (state_set_read(*set, name) != NULL)
        return EEXIST;

    errno = 0;

    state_t *tmp = realloc(set->ptr, sizeof(*set->ptr) * (set->num + 1));
    if (tmp == NULL)
        return errno;

    set->ptr = tmp;

    char *alloc_name = strdup(name);
    if (alloc_name == NULL)
        return ENOMEM;

    set->ptr[set->num].name = alloc_name;
    set->ptr[set->num].enabled = enabled;
    set->num++;

    qsort(set->ptr, set->num, sizeof(*set->ptr), state_compare);
    return 0;
}

int state_set_add(state_set_t *set, char const *name, bool enabled)
{
    if ((set == NULL) || (name == NULL))
        return EINVAL;

    state_t *state = state_set_read(*set, name);
    if ((state == NULL) && (errno != ENOENT))
        return errno;
    errno = 0;

    if (state == NULL)
        return state_set_create(set, name, enabled);

    state->enabled = enabled;
    return 0;
}

void state_set_reset(state_set_t *set)
{
    if (set == NULL)
        return;

    for (size_t i = 0; i < set->num; i++) {
        free(set->ptr[i].name);
    }
    free(set->ptr);

    set->ptr = NULL;
    set->num = 0;
}

int state_set_clone(state_set_t *dest, state_set_t src)
{
    if (src.num == 0)
        return 0;

    state_set_t ret = {
        .ptr = calloc(src.num, sizeof(*ret.ptr)),
        .num = src.num,
    };

    if (ret.ptr == NULL)
        return ENOMEM;

    for (size_t i = 0; i < src.num; i++) {
        ret.ptr[i].name = strdup(src.ptr[i].name);
        ret.ptr[i].enabled = src.ptr[i].enabled;
        if (ret.ptr[i].name == NULL) {
            state_set_reset(&ret);
            return ENOMEM;
        }
    }

    *dest = ret;
    return 0;
}
