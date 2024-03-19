// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include "libmdb/mql.h"
#include "libmdb/node.h"
#include "libmdb/value.h"

#if 0

int nsteps = (int)((ctx->end - ctx->start) / ctx->interval) +1

for (int ts = ctx->start; ts <= ctx->end; ts += ctx->interval) {

}

#endif


mql_value_t *mql_eval_binary_scalar_scalar (__attribute__((unused)) mql_eval_ctx_t *ctx,
                                              mql_node_t *node,
                                              mql_value_t *val1,
                                              mql_value_t *val2)
{
    double result = 0.0;

    switch (node->binary.op) {
        case MQL_BINARY_OP_ADD:
            result = val1->scalar.value + val2->scalar.value;
            break;
        case MQL_BINARY_OP_SUB:
            result = val1->scalar.value - val2->scalar.value;
            break;
        case MQL_BINARY_OP_MUL:
            result = val1->scalar.value * val2->scalar.value;
            break;
        case MQL_BINARY_OP_DIV:
            result = val1->scalar.value / val2->scalar.value;
            break;
        case MQL_BINARY_OP_MOD:
            result = fmod(val1->scalar.value, val2->scalar.value);
            break;
        case MQL_BINARY_OP_POW:
            result = pow(val1->scalar.value, val2->scalar.value);
            break;
        case MQL_BINARY_OP_EQLC:
            result = val1->scalar.value == val2->scalar.value;
            break;
        case MQL_BINARY_OP_NEQ:
            result = val1->scalar.value != val2->scalar.value;
            break;
        case MQL_BINARY_OP_GTR:
            result = val1->scalar.value > val2->scalar.value;
            break;
        case MQL_BINARY_OP_GTE:
            result = val1->scalar.value >= val2->scalar.value;
            break;
        case MQL_BINARY_OP_LSS:
            result = val1->scalar.value < val2->scalar.value;
            break;
        case MQL_BINARY_OP_LTE:
            result = val1->scalar.value <= val2->scalar.value;
            break;
        case MQL_BINARY_OP_AND:
            // FIXME error
            break;
        case MQL_BINARY_OP_OR:
            // FIXME error
            break;
        case MQL_BINARY_OP_UNLESS:
            // FIXME error
            break;
    }
    // FIXME free
    return mql_value_scalar(0, result);
}

mql_value_t *mql_eval_binary_vector_scalar (__attribute__((unused)) mql_eval_ctx_t *ctx,
                                              mql_node_t *node,
                                              mql_value_t *val1,
                                              mql_value_t *val2)
{
    double result = 0.0;

    switch (node->binary.op) {
        case MQL_BINARY_OP_ADD:
            result = val1->scalar.value + val2->scalar.value;
            break;
        case MQL_BINARY_OP_SUB:
            result = val1->scalar.value - val2->scalar.value;
            break;
        case MQL_BINARY_OP_MUL:
            result = val1->scalar.value * val2->scalar.value;
            break;
        case MQL_BINARY_OP_DIV:
            result = val1->scalar.value / val2->scalar.value;
            break;
        case MQL_BINARY_OP_MOD:
//          result = val1->scalar.value % val2->scalar.value;
            break;
        case MQL_BINARY_OP_POW:
            result = pow(val1->scalar.value, val2->scalar.value);
            break;
        case MQL_BINARY_OP_EQLC:
            result = val1->scalar.value == val2->scalar.value;
            break;
        case MQL_BINARY_OP_NEQ:
            result = val1->scalar.value != val2->scalar.value;
            break;
        case MQL_BINARY_OP_GTR:
            result = val1->scalar.value > val2->scalar.value;
            break;
        case MQL_BINARY_OP_GTE:
            result = val1->scalar.value >= val2->scalar.value;
            break;
        case MQL_BINARY_OP_LSS:
            result = val1->scalar.value < val2->scalar.value;
            break;
        case MQL_BINARY_OP_LTE:
            result = val1->scalar.value <= val2->scalar.value;
            break;
        case MQL_BINARY_OP_AND:
            // FIXME error
            break;
        case MQL_BINARY_OP_OR:
            // FIXME error
            break;
        case MQL_BINARY_OP_UNLESS:
            // FIXME error
            break;
    }
    // FIXME free
    return mql_value_scalar(0, result);
}

mql_value_t *mql_eval_binary_vector_vector (__attribute__((unused)) mql_eval_ctx_t *ctx,
                                              mql_node_t *node,
                                              mql_value_t *val1,
                                              mql_value_t *val2)
{
    double result = 0.0;

    switch (node->binary.op) {
        case MQL_BINARY_OP_ADD:
            result = val1->scalar.value + val2->scalar.value;
            break;
        case MQL_BINARY_OP_SUB:
            result = val1->scalar.value - val2->scalar.value;
            break;
        case MQL_BINARY_OP_MUL:
            result = val1->scalar.value * val2->scalar.value;
            break;
        case MQL_BINARY_OP_DIV:
            result = val1->scalar.value / val2->scalar.value;
            break;
        case MQL_BINARY_OP_MOD:
//          result = val1->scalar.value % val2->scalar.value;
            break;
        case MQL_BINARY_OP_POW:
            result = pow(val1->scalar.value, val2->scalar.value);
            break;
        case MQL_BINARY_OP_EQLC:
            result = val1->scalar.value == val2->scalar.value;
            break;
        case MQL_BINARY_OP_NEQ:
            result = val1->scalar.value != val2->scalar.value;
            break;
        case MQL_BINARY_OP_GTR:
            result = val1->scalar.value > val2->scalar.value;
            break;
        case MQL_BINARY_OP_GTE:
            result = val1->scalar.value >= val2->scalar.value;
            break;
        case MQL_BINARY_OP_LSS:
            result = val1->scalar.value < val2->scalar.value;
            break;
        case MQL_BINARY_OP_LTE:
            result = val1->scalar.value <= val2->scalar.value;
            break;
        case MQL_BINARY_OP_AND:
            // FIXME error
            break;
        case MQL_BINARY_OP_OR:
            // FIXME error
            break;
        case MQL_BINARY_OP_UNLESS:
            // FIXME error
            break;
    }
    // FIXME free
    return mql_value_scalar(0, result);
}

mql_value_t *mql_eval_unary(__attribute__((unused)) mql_eval_ctx_t *ctx, mql_node_t *node)
{
    switch (node->unary.op) {
        case MQL_UNARY_OP_ADD:
            break;
        case MQL_UNARY_OP_SUB:
            break;
    }

    return NULL;
}

#if 0
mql_value_t *mql_eval_vector(__attribute__((unused)) mql_eval_ctx_t *ctx, mql_node_t *node)
{


    return NULL;
}

mql_value_t *mql_eval_matrix(__attribute__((unused)) mql_eval_ctx_t *ctx, mql_node_t *node)
{

    return NULL;
}
#endif

mql_value_t *mql_eval(mql_eval_ctx_t *ctx, mql_node_t *node)
{
    mql_value_t *value = NULL;


    switch (node->kind) {
        case MQL_NODE_AGGREGATE:
//          value = mql_eval_aggregate(node);
            break;
        case MQL_NODE_BINARY: {
            mql_value_t *lvalue = mql_eval(ctx, node->binary.lexpr);
            if (lvalue == NULL) {
                return NULL;
            }
            mql_value_t *rvalue = mql_eval(ctx, node->binary.rexpr);
            if (rvalue == NULL) {
                return NULL;
            }
            if ((lvalue->kind == MQL_VALUE_SCALAR) && (rvalue->kind == MQL_VALUE_SCALAR)) {
                return mql_eval_binary_scalar_scalar(ctx, node, lvalue, rvalue);
            } else if ((lvalue->kind == MQL_VALUE_SAMPLES) && (rvalue->kind == MQL_VALUE_SAMPLES)) {
                return mql_eval_binary_vector_vector(ctx, node, lvalue, rvalue);
            } else if ((lvalue->kind == MQL_VALUE_SCALAR) && (rvalue->kind == MQL_VALUE_SAMPLES)) {
                return mql_eval_binary_vector_scalar(ctx, node, lvalue, rvalue);
            } else if ((lvalue->kind == MQL_VALUE_SAMPLES) && (rvalue->kind == MQL_VALUE_SCALAR)) {
                return mql_eval_binary_vector_scalar(ctx, node, lvalue, rvalue);
            } else {
                return NULL;
            }
            break;
        }
        case MQL_NODE_UNARY:
            value = mql_eval_unary(ctx, node);
            break;
        case MQL_NODE_CALL:
            break;
        case MQL_NODE_SUBQUERY:

            break;
        case MQL_NODE_VECTOR:
//          return mql_value_vector(node);
            break;
        case MQL_NODE_STRING:
#if 0
            return mql_value_string(0, node->string);
#endif
            break;
        case MQL_NODE_NUMBER:
            return mql_value_scalar(0, node->number);
            break;
        case MQL_NODE_MATRIX:
            break;
    }

    return value;
}
