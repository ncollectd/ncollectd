/* SPDX-License-Identifier: GPL-2.0-only OR PostgreSQL                                  */
/* SPDX-FileCopyrightText: Copyright (c) 1996-2023, PostgreSQL Global Development Group */
/* SPDX-FileCopyrightText: Copyright (c) 1994, Regents of the University of California  */

#pragma once

typedef struct {
    char *val;
    int len;
    int total;
} jsonpath_string_t;

#include "jsonpath.h"

extern int jsonpath_yyparse(void *scanner, jsonpath_parse_result_t *result);
