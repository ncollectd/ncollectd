/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include <stdbool.h>

#include "libmetric/metric_match.h"
#include "libmdb/functions.h"

struct mql_node;
typedef struct mql_node mql_node_t;

typedef struct mql_labels {
    char **labels;
    int num;
} mql_labels_t;

typedef struct mql_node_list {
    mql_node_t *expr;
    struct mql_node_list *next;
} mql_node_list_t;

typedef enum {
    MQL_AGGREGATE_OP_AVG,
    MQL_AGGREGATE_OP_BOTTOMK,
    MQL_AGGREGATE_OP_COUNT,
    MQL_AGGREGATE_OP_COUNT_VALUES,
    MQL_AGGREGATE_OP_GROUP,
    MQL_AGGREGATE_OP_MAX,
    MQL_AGGREGATE_OP_MIN,
    MQL_AGGREGATE_OP_QUANTILE,
    MQL_AGGREGATE_OP_STDDEV,
    MQL_AGGREGATE_OP_STDVAR,
    MQL_AGGREGATE_OP_SUM,
    MQL_AGGREGATE_OP_TOPK
} mql_aggregate_op_e;

typedef enum {
    MQL_AGGREGATE_NONE,
    MQL_AGGREGATE_BY,
    MQL_AGGREGATE_WITHOUT
} mql_aggregate_modifier_e;

typedef struct  {
    mql_aggregate_op_e op;
    mql_node_list_t *args;
    mql_aggregate_modifier_e modifier;
    mql_labels_t *labels;
} mql_node_aggregate_t;

typedef enum {
    MQL_BINARY_OP_ADD,
    MQL_BINARY_OP_DIV,
    MQL_BINARY_OP_EQLC,
    MQL_BINARY_OP_GTE,
    MQL_BINARY_OP_GTR,
    MQL_BINARY_OP_AND,
    MQL_BINARY_OP_OR,
    MQL_BINARY_OP_LSS,
    MQL_BINARY_OP_LTE,
    MQL_BINARY_OP_UNLESS,
    MQL_BINARY_OP_MOD,
    MQL_BINARY_OP_MUL,
    MQL_BINARY_OP_NEQ,
    MQL_BINARY_OP_POW,
    MQL_BINARY_OP_SUB
} mql_binary_op_e;

typedef enum {
    MQL_INCLEXCL_NONE,
    MQL_INCLEXCL_IGNORING,
    MQL_INCLEXCL_ON
} mql_inclexcl_e;

typedef enum {
    MQL_GROUP_NONE,
    MQL_GROUP_LEFT,
    MQL_GROUP_RIGHT
} mql_group_e;

typedef struct mql_node_group_mod {
    bool bool_mod;
    mql_inclexcl_e inclexcl_op;
    mql_labels_t *inclexcl_labels;
    mql_group_e group_op;
    mql_labels_t *group_labels;
} mql_node_group_mod_t;

typedef struct mql_node_binary {
    mql_binary_op_e op;
    mql_node_t *lexpr;
    mql_node_t *rexpr;
    mql_node_group_mod_t *mod;
} mql_node_binary_t;

typedef enum {
    MQL_UNARY_OP_ADD,
    MQL_UNARY_OP_SUB
} mql_unary_op_e;

typedef struct mql_node_unary {
    mql_unary_op_e op;
    mql_node_t *expr;
} mql_node_unary_t;

typedef struct mql_node_call {
    mql_function_t *func;
    mql_node_list_t *args;
} mql_node_call_t;

typedef struct mql_node_subquery {
    mql_node_t *expr;
    int64_t offset;
    uint64_t range;
    uint64_t step; // FIXME
} mql_node_subquery_t;

typedef enum {
    MQL_AT_NONE,
    MQL_AT_TIMESTAMP,
    MQL_AT_START,
    MQL_AT_END,
} mql_at_e;

typedef struct mql_node_vector {
    metric_match_t match;
    struct {
        mql_at_e at;
        int64_t timestamp;
    } at;
    int64_t offset;
} mql_node_vector_t;

typedef struct mql_node_matrix {
    mql_node_t *expr;
    uint64_t range;
} mql_node_matrix_t;

typedef enum {
    MQL_NODE_AGGREGATE,
    MQL_NODE_BINARY,
    MQL_NODE_UNARY,
    MQL_NODE_CALL,
    MQL_NODE_SUBQUERY,
    MQL_NODE_VECTOR,
    MQL_NODE_STRING,
    MQL_NODE_NUMBER,
    MQL_NODE_MATRIX,
} mql_node_e;

typedef struct mql_node {
    mql_node_e kind;
    union {
        mql_node_aggregate_t aggregate;
        mql_node_binary_t binary;
        mql_node_unary_t unary;
        mql_node_call_t call;
        mql_node_subquery_t subquery;
        mql_node_vector_t vector;
        mql_node_matrix_t matrix;
        char *string;
        double number;
    };
} mql_node_t;



mql_labels_t *mql_labels_append(mql_labels_t *labels, char *label);
#if 0
mql_label_match_t *mql_label_match(char *label, metric_match_op_t op, char *value);
mql_labels_match_t *mql_labels_match(void);
mql_labels_match_t *mql_labels_match_append(mql_labels_match_t *labels_match, mql_label_match_t *label_match);
#endif

mql_node_group_mod_t *mql_node_group_mod (bool bool_mod);

mql_node_group_mod_t *mql_node_group_mod_inclexcl(mql_node_group_mod_t *mod, mql_inclexcl_e op, mql_labels_t *labels);

mql_node_group_mod_t *mql_node_group_mod_group(mql_node_group_mod_t *mod, mql_group_e op, mql_labels_t *labels);

mql_node_t *mql_node_offset(mql_node_t *node, int64_t offset);

mql_node_t *mql_node_at(mql_node_t *node, mql_at_e at, int64_t timestamp);

mql_node_t *mql_node_aggregate (mql_aggregate_op_e op, mql_aggregate_modifier_e mod, mql_labels_t *labels, mql_node_list_t *args);

mql_node_t *mql_node_binary (mql_node_t *lexpr, mql_binary_op_e op, mql_node_group_mod_t *mod, mql_node_t *rexpr);

mql_node_t *mql_node_unary (mql_unary_op_e op, mql_node_t *expr);

mql_node_t *mql_node_vector (char *metric, metric_match_set_t *labels_match);

mql_node_t *mql_node_subquery (mql_node_t *expr, uint64_t range, uint64_t step);

mql_node_t *mql_node_matrix (mql_node_t *expr, uint64_t range);

mql_node_t *mql_node_call (char *idenfier, mql_node_list_t *args);

mql_node_t *mql_node_string (char *string);

mql_node_t *mql_node_number (double number);

mql_node_list_t *mql_node_list_append(mql_node_list_t *list, mql_node_t *expr);

int mql_node_cmp(mql_node_t *node1, mql_node_t *node2);

void mql_node_free(mql_node_t *node);
