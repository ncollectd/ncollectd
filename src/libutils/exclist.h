/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

struct exclist_entry_s;
typedef struct exclist_entry_s exclist_entry_t;

typedef struct {
    exclist_entry_t **ptr;
    size_t num;
} exclist_list_t;

typedef struct {
    exclist_list_t incl;
    exclist_list_t excl;
} exclist_t;

int cf_util_exclist(const config_item_t *ci, exclist_t *excl);

int exclist_add_incl_string(exclist_t *excl, const char *value);

int exclist_remove_incl_string(exclist_t *excl, const char *value);

int exclist_add_excl_string(exclist_t *excl, const char *value);

int exclist_remove_excl_string(exclist_t *excl, const char *value);

bool exclist_match(exclist_t *excl, const char *value);

static inline size_t exclist_size(exclist_t *excl)
{
    return excl->incl.num + excl->excl.num;
}

void exclist_reset(exclist_t *excl);
