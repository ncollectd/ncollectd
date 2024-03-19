/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include <stdio.h>
#include "libmdb/node.h"
#include "libmdb/value.h"

struct mql_node;
typedef struct mql_node mql_node_t;

typedef struct {
    int first_line;
    int first_column;
    int last_line;
    int last_column;
    const char *errmsg;
    mql_node_t *root;
} mql_status_t;

typedef struct {
    uint64_t start;
    uint64_t end;
    uint64_t step;
} mql_eval_ctx_t;

int mql_parser(char *query, mql_status_t *status);

int mql_node_dump(mql_node_t *root, int max_depth, FILE *stream);

mql_value_t *mql_eval(mql_eval_ctx_t *ctx, mql_node_t *node);

void mql_node_free(mql_node_t *node);
