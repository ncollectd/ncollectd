/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "libmdb/mql.h"
#include "libmdb/value.h"

//typedef mql_value_t (*mql_function_cb)(mql_eval_ctx_t *, size_t, mql_value_t *);
// typedef mql_value_t *(*mql_function_cb)(mql_eval_ctx_t *, size_t, mql_value_t *);

typedef struct {
    char *name;
    mql_value_t *(*callback)(void *, size_t, mql_value_t *);
    mql_value_e ret;
    int variadic;
    mql_value_e arg1;
    mql_value_e arg2;
    mql_value_e arg3;
    mql_value_e arg4;
    mql_value_e arg5;
} mql_function_t;

mql_function_t *mql_function_get (char *name);
