/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include <errno.h>

typedef struct {
    char *name;
    bool enabled;
} state_t;

typedef struct {
    size_t num;
    state_t *ptr;
} state_set_t;

state_t *state_set_read(state_set_t set, const char *name);

int state_set_create(state_set_t *set, char const *name, bool enabled);

int state_set_add(state_set_t *set, char const *name, bool enabled);

static inline int state_set_enable(state_set_t set, char const *name)
{
    if (name == NULL)
        return EINVAL;

    for (size_t i = 0; i < set.num; i++) {
        if (strcmp(set.ptr[i].name, name) == 0) {
            set.ptr[i].enabled = true;
            return 0;
        }
    }

    return 0;
}

void state_set_reset(state_set_t *set);

int state_set_clone(state_set_t *dest, state_set_t src);
