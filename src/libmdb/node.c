// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>

#include "libmdb/node.h"

char *mql_node_kind2str(mql_node_e kind)
{
    switch(kind) {
        case MQL_NODE_AGGREGATE:
            return "aggregate";
        case MQL_NODE_BINARY:
            return "binary";
        case MQL_NODE_UNARY:
            return "unary";
        case MQL_NODE_CALL:
            return "call";
        case MQL_NODE_SUBQUERY:
            return "subquery";
        case MQL_NODE_VECTOR:
            return "vector";
        case MQL_NODE_STRING:
            return "string";
        case MQL_NODE_NUMBER:
            return "number";
        case MQL_NODE_MATRIX:
            return "matrix";
    }
    return NULL;
}

char *mql_aggregate_op2str(mql_aggregate_op_e op)
{
    switch(op) {
        case MQL_AGGREGATE_OP_AVG:
            return "avg";
        case MQL_AGGREGATE_OP_BOTTOMK:
            return "bottomk";
        case MQL_AGGREGATE_OP_COUNT:
            return "count";
        case MQL_AGGREGATE_OP_COUNT_VALUES:
            return "values";
        case MQL_AGGREGATE_OP_GROUP:
            return "group";
        case MQL_AGGREGATE_OP_MAX:
            return "max";
        case MQL_AGGREGATE_OP_MIN:
            return "min";
        case MQL_AGGREGATE_OP_QUANTILE:
            return "quantile";
        case MQL_AGGREGATE_OP_STDDEV:
            return "stddev";
        case MQL_AGGREGATE_OP_STDVAR:
            return "stdvar";
        case MQL_AGGREGATE_OP_SUM:
            return "sum";
        case MQL_AGGREGATE_OP_TOPK:
            return "topk";
    }
    return NULL;
}

char *mql_aggregate_modifier2str(mql_aggregate_modifier_e modifier)
{
    switch (modifier) {
         case MQL_AGGREGATE_NONE:
             return NULL;
         case MQL_AGGREGATE_BY:
             return "by";
         case MQL_AGGREGATE_WITHOUT:
             return "without";
    }
    return NULL;
}

char *mql_binary_op2str(mql_binary_op_e op)
{
    switch(op) {
        case MQL_BINARY_OP_ADD:
            return "+";
        case MQL_BINARY_OP_DIV:
            return "/";
        case MQL_BINARY_OP_EQLC:
            return "==";
        case MQL_BINARY_OP_GTE:
            return ">=";
        case MQL_BINARY_OP_GTR:
            return ">";
        case MQL_BINARY_OP_AND:
            return "and";
        case MQL_BINARY_OP_OR:
            return "or";
        case MQL_BINARY_OP_LSS:
            return "<";
        case MQL_BINARY_OP_LTE:
            return "<=";
        case MQL_BINARY_OP_UNLESS:
            return "unless";
        case MQL_BINARY_OP_MOD:
            return "%";
        case MQL_BINARY_OP_MUL:
            return "*";
        case MQL_BINARY_OP_NEQ:
            return "!=";
        case MQL_BINARY_OP_POW:
            return "^";
        case MQL_BINARY_OP_SUB:
            return "-";
    }
    return NULL;
}

char *mql_unary_op2str(mql_unary_op_e op)
{
    switch(op) {
        case MQL_UNARY_OP_ADD:
            return "+";
        case MQL_UNARY_OP_SUB:
            return "-";
    }
    return NULL;
}

char *mql_match_op2str(metric_match_op_t op)
{
    switch(op) {
    case METRIC_MATCH_OP_NONE:
        break;
    case METRIC_MATCH_OP_EQL:
        return "=";
    case METRIC_MATCH_OP_NEQ:
        return "!=";
    case METRIC_MATCH_OP_EQL_REGEX:
        return "=~";
    case METRIC_MATCH_OP_NEQ_REGEX:
        return "!~";
    case METRIC_MATCH_OP_EXISTS:
        return "!=";
    case METRIC_MATCH_OP_NEXISTS:
        return "=";
    }
    return NULL;
}

mql_node_t *mql_node (mql_node_e kind)
{
    mql_node_t *node = calloc (1, sizeof(*node));
    if (node == NULL) {
        // FIXME
        return NULL;
    }

    node->kind = kind;

    return node;
}

static void mql_node_list_free(mql_node_list_t *args)
{
    if (args == NULL)
        return;

    mql_node_free(args->expr);

    if (args->next != NULL)
        mql_node_list_free(args->next);

    free(args);
}

static void mql_labels_free(mql_labels_t *labels)
{
    if (labels == NULL)
        return;

    if (labels->labels != NULL) {
        for (int i=0; i < labels->num ; i++) {
            if (labels->labels[i] != NULL)
                free(labels->labels[i]);
        }
        free(labels->labels);
    }

    free(labels);
}

void mql_node_free(mql_node_t *node)
{
    if (node == NULL)
        return;

    switch (node->kind) {
        case MQL_NODE_AGGREGATE:
            mql_node_list_free(node->aggregate.args);
            mql_labels_free(node->aggregate.labels);
            break;
        case MQL_NODE_BINARY:
            mql_node_free(node->binary.lexpr);
            mql_node_free(node->binary.rexpr);
            if (node->binary.mod != NULL) {
                if(node->binary.mod->inclexcl_labels != NULL)
                    mql_labels_free(node->binary.mod->inclexcl_labels);
                if(node->binary.mod->group_labels != NULL)
                    mql_labels_free(node->binary.mod->group_labels);
            }
            break;
        case MQL_NODE_UNARY:
            mql_node_free(node->unary.expr);
            break;
        case MQL_NODE_CALL:
            if (node->call.args != NULL)
                mql_node_list_free(node->call.args);
            break;
        case MQL_NODE_SUBQUERY:
            mql_node_free(node->subquery.expr);
            break;
        case MQL_NODE_VECTOR:
            metric_match_reset(&node->vector.match);
            break;
        case MQL_NODE_STRING:
            if (node->string != NULL)
                free(node->string);
            break;
        case MQL_NODE_NUMBER:
            break;
        case MQL_NODE_MATRIX:
            mql_node_free(node->matrix.expr);
            break;
    }
    free(node);
}

mql_node_t *mql_node_offset(mql_node_t *node, int64_t offset)
{
    if (node == NULL)
        return NULL;

    mql_node_vector_t *vector = NULL;
    if (node->kind == MQL_NODE_VECTOR) {
        vector = &node->vector;
        vector->offset = offset;
        return node;
    } else if (node->kind == MQL_NODE_MATRIX) {
        if (node->matrix.expr != NULL && node->matrix.expr->kind == MQL_NODE_VECTOR) {
            vector = &node->matrix.expr->vector;
            vector->offset = offset;
            return node;
        }
    } else if (node->kind == MQL_NODE_SUBQUERY) {
        node->subquery.offset = offset;
        return node;
    }

    return NULL;
}

mql_node_t *mql_node_at(mql_node_t *node, mql_at_e at, int64_t timestamp)
{
    if (node == NULL)
        return NULL;

    mql_node_vector_t *vector = NULL;
    if (node->kind == MQL_NODE_VECTOR) {
        vector = &node->vector;
    } else if (node->kind == MQL_NODE_MATRIX) {
        if (node->matrix.expr != NULL && node->matrix.expr->kind == MQL_NODE_VECTOR) {
            vector = &node->matrix.expr->vector;
        }
    }

    if (vector == NULL) {
        return NULL;
    }

    vector->at.at = at;
    vector->at.timestamp = timestamp;

    return node;
}

mql_node_t *mql_node_aggregate (mql_aggregate_op_e op, mql_aggregate_modifier_e mod, mql_labels_t *labels, mql_node_list_t *args)
{
    mql_node_t *node = mql_node(MQL_NODE_AGGREGATE);
    if (node == NULL)
        return NULL;

    node->aggregate.op  = op;
    node->aggregate.modifier = mod;
    node->aggregate.args = args;

    node->aggregate.labels = labels;

    return node;
}

mql_node_t *mql_node_binary (mql_node_t *lexpr, mql_binary_op_e op,  mql_node_group_mod_t *mod, mql_node_t *rexpr)
{
    mql_node_t *node = mql_node(MQL_NODE_BINARY);
    if (node == NULL)
        return NULL;

    node->binary.lexpr = lexpr;
    node->binary.op  = op;
    node->binary.mod = mod;
    node->binary.rexpr = rexpr;

    return node;
}

mql_node_t *mql_node_unary (mql_unary_op_e op, mql_node_t *expr)
{
    mql_node_t *node = mql_node(MQL_NODE_UNARY);
    if (node == NULL)
        return NULL;

    node->unary.op = op;
    node->unary.expr = expr;

    return node;
}

mql_node_t *mql_node_call (char *identifier, mql_node_list_t *args)
{
    mql_node_t *node = mql_node(MQL_NODE_CALL);
    if (node == NULL)
        return NULL;

    node->call.args = args;

    mql_function_t *func = mql_function_get (identifier);
    if (func == NULL) {
        mql_node_free(node);
        return NULL;
    }

    node->call.func = func;

    return node;
}

mql_node_t *mql_node_subquery (mql_node_t *expr, uint64_t range, uint64_t step)
{
    mql_node_t *node = mql_node(MQL_NODE_SUBQUERY);
    if (node == NULL)
        return NULL;

    node->subquery.expr = expr;
    node->subquery.range = range;
    node->subquery.step = step;
    return node;
}

mql_node_t *mql_node_vector (char *metric, metric_match_set_t *match_labels)
{
    mql_node_t *node = mql_node(MQL_NODE_VECTOR);
    if (node == NULL) {
        metric_match_set_free(match_labels);
        return NULL;
    }

    if (metric != NULL) {
        node->vector.match.name = metric_match_set_alloc();
        if (node->vector.match.name == NULL) {
            metric_match_set_free(match_labels);
            mql_node_free(node);
            return NULL;
        }

        metric_match_pair_t *pair = metric_match_pair_alloc("__name__", METRIC_MATCH_OP_EQL, metric);
        if (pair == NULL) {
            metric_match_set_free(match_labels);
            mql_node_free(node);
            return NULL;
        }

        metric_match_set_append(node->vector.match.name, pair);
    }

    if (match_labels != NULL) {
        for (size_t i = 0; i < match_labels->num; i++) {
            metric_match_pair_t *pair = match_labels->ptr[i];
            if (pair == NULL)
                continue;
            if (strcmp(pair->name, "__name__") == 0) {
                if (node->vector.match.name == NULL) {
                    node->vector.match.name = metric_match_set_alloc();
                    if (node->vector.match.name == NULL) {
                        metric_match_set_free(match_labels);
                        mql_node_free(node);
                        return NULL;
                    }
                }
                metric_match_set_append(node->vector.match.name, pair);
                match_labels->ptr[i] = NULL;
            }
        }

        node->vector.match.labels = match_labels;
    }

    return node;
}

mql_node_t *mql_node_matrix (mql_node_t *expr, uint64_t range)
{
    mql_node_t *node = mql_node(MQL_NODE_MATRIX);
    if (node == NULL)
        return NULL;

    node->matrix.expr = expr;
    node->matrix.range = range;

    return node;
}

mql_node_t *mql_node_string (char *string)
{
    mql_node_t *node = mql_node(MQL_NODE_STRING);
    if (node == NULL)
        return NULL;

    node->string = strdup(string);
    if (node->string == NULL) {
        mql_node_free(node);
        return NULL;
    }

    return node;
}

mql_node_t *mql_node_number (double number)
{
    mql_node_t *node = mql_node(MQL_NODE_NUMBER);
    if (node == NULL)
        return NULL;

    node->number = number;

    return node;
}


mql_labels_t *mql_labels_append(mql_labels_t *labels, char *label)
{
    if (label == NULL) {
        // FIXME
        return NULL;
    }

    if (labels == NULL) {
        labels = calloc (1, sizeof(*labels));
        if (labels == NULL) {
            // FIXME
            return NULL;
        }
    }

    char **tmp = realloc(labels->labels, sizeof(char *)*(labels->num+1));
    if (tmp == NULL) {
        free(labels);
        return NULL;
    }
    labels->labels = tmp;

    labels->labels[labels->num] = strdup(label);
    // FIXME
    labels->num++;

    return labels;
}
#if 0
metric_match_pair_t *mql_label_match(char *label, metric_match_op_t op, char *value)
{
    if ((value == NULL) || (strlen(value) == 0)) {
        if (op == METRIC_MATCH_OP_EQL) {
            op = METRIC_MATCH_OP_NEXISTS;
        } else if (op == METRIC_MATCH_OP_NEQ) {
            op = METRIC_MATCH_OP_EXISTS;
        } else {
            // FIXME
            return NULL;
        }
    }

    metric_match_pair_t *pair = calloc (1, sizeof(*pair));
    if (pair == NULL) {
        // FIXME
        return NULL;
    }


    label_match->label = strdup(label);
    if (label_match->label == NULL) {
        // FIXME
    }

    if ((value == NULL) || (strlen(value) == 0)) {

    label_match->value = strdup(value);
    if (label_match->value == NULL) {
        // FIXME
    }
    label_match->op = op;

    return label_match;
}

mql_labels_match_t *mql_labels_match_append(mql_labels_match_t *labels_match, mql_label_match_t *label_match)
{
    if (label_match == NULL)
        return NULL;

    if (labels_match == NULL) {
        labels_match = calloc (1, sizeof(*labels_match));
        if (labels_match == NULL) {
            // FIXME
            return NULL;
        }
    }

    mql_label_match_t **tmp = realloc(labels_match->labels, sizeof(mql_label_match_t *)*(labels_match->num+1));
    if (tmp == NULL) {
        return NULL;
    }
    labels_match->labels = tmp;

    labels_match->labels[labels_match->num] = label_match;

    labels_match->num++;

    return labels_match;
}
#endif

mql_node_group_mod_t *mql_node_group_mod (bool bool_mod)
{
    mql_node_group_mod_t *mod = calloc (1, sizeof(mql_node_group_mod_t));
    if (mod == NULL)
        return NULL;

    mod->bool_mod = bool_mod;
    return mod;
}

mql_node_group_mod_t *mql_node_group_mod_inclexcl(mql_node_group_mod_t *mod, mql_inclexcl_e op, mql_labels_t *labels)
{
    if (mod == NULL)
        return NULL;

    mod->inclexcl_op = op;
    mod->inclexcl_labels = labels;
    return mod;
}

mql_node_group_mod_t *mql_node_group_mod_group(mql_node_group_mod_t *mod, mql_group_e op, mql_labels_t *labels)
{
    if (mod == NULL)
        return NULL;

    mod->group_op = op;
    mod->group_labels = labels;
    return mod;
}

mql_node_list_t *mql_node_list_append(mql_node_list_t *list, mql_node_t *expr)
{
    mql_node_list_t *node_list = calloc (1, sizeof(*node_list));
    if (node_list == NULL) {
        // FIXME
        return NULL;
    }
    node_list->expr = expr;

    if (list != NULL) {
        mql_node_list_t *end = list;
        while (end != NULL) {
            if (end->next == NULL)
                break;
            end = end->next;
        }
        end->next = node_list;
        return list;
    }

    return node_list;
}

static int mql_node_labels_cmp(mql_labels_t *labels1, mql_labels_t *labels2)
{
    if ((labels1 == NULL) && (labels2 == NULL))
        return 0;
    if ((labels1 != NULL) && (labels2 == NULL))
        return -1;
    if ((labels1 == NULL) && (labels2 != NULL))
        return -1;

    if (labels1->num != labels2->num)
        return -1;

    for (int i = 0; i <  labels1->num ; i++) {
        if (strcmp(labels1->labels[i], labels2->labels[i]) != 0)
            return -1;
    }
    return 0;
}

static int mql_node_list_cmp(mql_node_list_t *args1, mql_node_list_t *args2)
{
    if ((args1 == NULL) && (args2 == NULL))
        return 0;
    if ((args1 != NULL) && (args2 == NULL))
        return -1;
    if ((args1 == NULL) && (args2 != NULL))
        return -1;

    while ((args1 != NULL) && (args2 != NULL)) {
        if (mql_node_cmp(args1->expr, args2->expr) < 0)
            return -1;
        args1 = args1->next;
        args2 = args2->next;
        if ((args1 != NULL) && (args2 == NULL))
            return -1;
        if ((args1 == NULL) && (args2 != NULL))
            return -1;
    }
    return 0;
}

static int mql_node_metric_match_set_cmp(metric_match_set_t *set1, metric_match_set_t *set2)
{
    if ((set1 == NULL) && (set2 == NULL))
        return 0;
    if ((set1 != NULL) && (set2 == NULL))
        return -1;
    if ((set1 == NULL) && (set2 != NULL))
        return -1;

    if (set1->num != set2->num)
        return -1;

    for (size_t i = 0; i < set1->num; i++) {
        metric_match_pair_t *pair1 = set1->ptr[i];
        metric_match_pair_t *pair2 = set2->ptr[i];

        if ((pair1 == NULL) || (pair2 == NULL))
            return -1; // FIXME

        if (pair1->op != pair2->op)
            return -1;
        if (strcmp(pair1->name, pair2->name) != 0)
            return -1;

        if ((pair1->value.string != NULL) && (pair2->value.string != NULL)) {
            if (strcmp(pair1->value.string, pair2->value.string) != 0)
                return -1;
        } else if ((pair1->value.string == NULL) || (pair2->value.string == NULL)) {
            // FIXME
        }
    }

    return 0;
}

static int mql_node_metric_match_cmp(metric_match_t *match1, metric_match_t *match2)
{
    if ((match1 == NULL) && (match2 == NULL))
        return 0;
    if ((match1 != NULL) && (match2 == NULL))
        return -1;
    if ((match1 == NULL) && (match2 != NULL))
        return -1;

    int status = mql_node_metric_match_set_cmp(match1->name, match2->name);
    if (status != 0)
        return status;

    return mql_node_metric_match_set_cmp(match1->labels, match2->labels);
}

int mql_node_cmp(mql_node_t *node1, mql_node_t *node2)
{
    if ((node1 == NULL) && (node2 == NULL))
        return 0;
    if ((node1 != NULL) && (node2 == NULL))
        return -1;
    if ((node1 == NULL) && (node2 != NULL))
        return -1;

    if (node1->kind != node2->kind)
        return -1;

    switch (node1->kind) {
        case MQL_NODE_AGGREGATE: {
            if (node1->aggregate.op != node2->aggregate.op)
                return -1;
            if (node1->aggregate.modifier != node2->aggregate.modifier)
                return -1;

            if (mql_node_labels_cmp(node1->aggregate.labels, node2->aggregate.labels) < 0)
                return -1;
            if (mql_node_list_cmp(node1->aggregate.args, node2->aggregate.args) < 0)
                return -1;
        }
            break;
        case MQL_NODE_BINARY:
            if (node1->binary.op != node2->binary.op)
                return -1;
            if ((node1->binary.mod != NULL) && (node2->binary.mod == NULL))
                return -1;
            if ((node1->binary.mod == NULL) && (node2->binary.mod != NULL))
                return -1;
            if (node1->binary.mod != NULL) {
                if (node1->binary.mod->bool_mod != node2->binary.mod->bool_mod)
                    return -1;
                if (node1->binary.mod->inclexcl_op != node2->binary.mod->inclexcl_op)
                    return -1;
                if (node1->binary.mod->group_op != node2->binary.mod->group_op)
                    return -1;
                if (mql_node_labels_cmp(node1->binary.mod->inclexcl_labels, node2->binary.mod->inclexcl_labels) < 0)
                    return -1;
                if (mql_node_labels_cmp(node1->binary.mod->group_labels, node2->binary.mod->group_labels) < 0)
                    return -1;
            }
            if (mql_node_cmp(node1->binary.lexpr, node2->binary.lexpr) < 0)
                return -1;
            if (mql_node_cmp(node1->binary.rexpr, node2->binary.rexpr) < 0)
                return -1;
            break;
        case MQL_NODE_UNARY:
            if (node1->unary.op != node2->unary.op)
                return -1;
            if (mql_node_cmp(node1->unary.expr, node2->unary.expr) < 0)
                return -1;
            break;
        case MQL_NODE_CALL:
            if (strcmp(node1->call.func->name, node2->call.func->name) != 0)
                return -1;
            if (mql_node_list_cmp(node1->call.args, node2->call.args) < 0)
                return -1;
            break;
        case MQL_NODE_SUBQUERY:
            if (node1->subquery.offset != node2->subquery.offset)
                return -1;
            if (node1->subquery.range != node2->subquery.range)
                return -1;
            if (node1->subquery.step != node2->subquery.step )
                return -1;
            if (mql_node_cmp(node1->subquery.expr, node2->subquery.expr) < 0)
                return -1;
            break;
        case MQL_NODE_VECTOR:
            if (node1->vector.offset != node2->vector.offset)
                return -1;
            if (node1->vector.at.at != node2->vector.at.at)
                return -1;
            if (node1->vector.at.timestamp != node2->vector.at.timestamp)
                return -1;
            if (mql_node_metric_match_cmp(&node1->vector.match, &node2->vector.match) < 0)
                return -1;
            break;
        case MQL_NODE_STRING:
            if (strcmp(node1->string, node2->string) != 0)
                return -1;
            break;
        case MQL_NODE_NUMBER:
            if (node1->number != node2->number)
                return -1;
            break;
        case MQL_NODE_MATRIX:
            if (node1->matrix.range != node2->matrix.range)
                return -1;
            if (mql_node_cmp(node1->matrix.expr, node2->matrix.expr) < 0)
                return -1;
            break;
    }
    return 0;
}

static int mql_dump_labels(mql_labels_t *labels, FILE *stream)
{
    if (labels == NULL) {
        return 0;
    }

    if (labels->num != 0) {
        fputs("(",stream);
        for (int n=0; n < labels->num; n++) {
            if (n!=0) fputs(",", stream);
            fputs(labels->labels[n], stream);
        }
        fputs(")",stream);
    }
    return 0;
}

static int mql_node_dump_node (int depth, int max_depth, int *parent, int connect, mql_node_t *node, FILE *stream)
{
    if (node == NULL) {
        return 0;
    }
    if (depth >= (max_depth -1)) {
        return 0;
    }

    int children = 0;
    switch (node->kind) {
        case MQL_NODE_AGGREGATE:
            children = node->aggregate.args != NULL ? 1 : 0;
            break;
        case MQL_NODE_BINARY:
            children = (node->binary.lexpr != NULL || node->binary.rexpr != NULL) ? 1 : 0;
            break;
        case MQL_NODE_UNARY:
            children = node->unary.expr != NULL ? 1 : 0;
            break;
        case MQL_NODE_CALL:
            children = node->call.args != NULL ? 1 : 0;
            break;
        case MQL_NODE_SUBQUERY:
            children = node->subquery.expr != NULL ? 1 : 0;
            break;
        case MQL_NODE_VECTOR:
            children = 0;
            break;
        case MQL_NODE_STRING:
            children = 0;
            break;
        case MQL_NODE_NUMBER:
            children = 0;
            break;
        case MQL_NODE_MATRIX:
            children = node->matrix.expr != NULL ? 1 : 0;
            break;
    }

    if (connect == 0)
        parent[depth] = 0;

    if (depth == 0) {
         if (children) {
             fputs("•─┬─", stream);
         } else {
             fputs("•───", stream);
         }
    } else {
        for (int n=0; n < depth; n++) {
            if (parent[n]) {
                fputs("│ ", stream);
            } else {
                fputs("  ", stream);
            }
        }
        if (children) {
            if (connect) {
                fputs("├─┬─", stream);
            } else {
                fputs("└─┬─", stream);
            }
        } else {
            if (connect) {
                fputs("├───", stream);
            } else {
                fputs("└───", stream);
            }
        }
    }

    switch (node->kind) {
        case MQL_NODE_AGGREGATE: {
            char *op = mql_aggregate_op2str(node->aggregate.op);
            fprintf(stream, "aggregate(%s", op);
            char *modif = mql_aggregate_modifier2str(node->aggregate.modifier);
            if (modif != NULL) {
                fprintf(stream, " %s", modif);
            }
            mql_dump_labels(node->aggregate.labels, stream);
            fputs(")\n", stream);

            parent[depth+1] = 1;
            mql_node_list_t *args = node->aggregate.args;
            while (args != NULL) {
                connect = args->next == NULL ? 0 : 1;
                mql_node_dump_node(depth+1, max_depth, parent, connect, args->expr, stream);
                args = args->next;
            }
        }
            break;
        case MQL_NODE_BINARY:
            fprintf(stream, "binary(%s", mql_binary_op2str(node->binary.op));
            if (node->binary.mod != NULL) {
                if (node->binary.mod->bool_mod) fputs(" bool", stream);

                switch(node->binary.mod->inclexcl_op) {
                    case MQL_INCLEXCL_NONE:
                        break;
                    case MQL_INCLEXCL_IGNORING:
                        fputs(" ignoring",stream);
                        mql_dump_labels(node->binary.mod->inclexcl_labels, stream);
                        break;
                    case MQL_INCLEXCL_ON:
                        fputs(" on",stream);
                        mql_dump_labels(node->binary.mod->inclexcl_labels, stream);
                        break;
                }
                switch(node->binary.mod->group_op) {
                    case MQL_GROUP_NONE:
                        break;
                    case MQL_GROUP_LEFT:
                        fputs(" group_left",stream);
                        mql_dump_labels(node->binary.mod->group_labels, stream);
                        break;
                    case MQL_GROUP_RIGHT:
                        fputs(" group_right",stream);
                        mql_dump_labels(node->binary.mod->group_labels, stream);
                        break;
                }
            }
            fputs(")\n", stream);

            parent[depth+1] = 1;
            mql_node_dump_node(depth+1, max_depth, parent, 1, node->binary.lexpr, stream);
            mql_node_dump_node(depth+1, max_depth, parent, 0, node->binary.rexpr, stream);
            break;
        case MQL_NODE_UNARY:
            fprintf(stream, "unary(%s)\n", mql_unary_op2str(node->unary.op));
            mql_node_dump_node(depth+1, max_depth, parent, 0, node->unary.expr, stream);
            break;
        case MQL_NODE_CALL: {
            fprintf(stream, "call(%s)\n", node->call.func->name);
            mql_node_list_t *args = node->call.args;
            parent[depth+1] = 1;
            while (args != NULL) {
                connect = args->next == NULL ? 0 : 1;
                mql_node_dump_node(depth+1, max_depth, parent, connect, args->expr, stream);
                args = args->next;
            }
        }
            break;
        case MQL_NODE_SUBQUERY:
            if (node->subquery.step == 0) {
                fprintf (stream, "subquery(%"PRIu64":)\n", node->subquery.range);
            } else {
                fprintf (stream, "subquery(%"PRIu64":%"PRIu64")\n", node->subquery.range, node->subquery.step);
            }
            mql_node_dump_node(depth+1, max_depth, parent, 0, node->subquery.expr, stream);
            break;
        case MQL_NODE_VECTOR:
            fprintf (stream, "vector(");
            if (node->vector.match.name != NULL) {
                metric_match_set_t *match_set = node->vector.match.name;
                if ((match_set->num == 1) && (match_set->ptr[0] != NULL) &&
                    (match_set->ptr[0]->op == METRIC_MATCH_OP_EQL)) {
                    fprintf (stream, "%s", match_set->ptr[0]->value.string);
                    if (node->vector.match.labels != NULL) {
                        fputs ("{", stream);
                        match_set = node->vector.match.labels;
                        for (size_t i=0; i < match_set->num; i++) {
                            metric_match_pair_t *pair = match_set->ptr[i];
                            if (pair == NULL)
                                continue;
                            fprintf(stream, "%s%s%s", pair->name, mql_match_op2str(pair->op),
                                                      pair->value.string);
                        }
                        fputs ("}", stream);
                    }
                } else {
                    fputs ("{", stream);
                    for (size_t i=0; i < match_set->num; i++) {
                        metric_match_pair_t *pair = match_set->ptr[i];
                        if (pair == NULL)
                            continue;
                        fprintf(stream, "%s%s%s", pair->name, mql_match_op2str(pair->op),
                                                  pair->value.string);
                    }
                    if (node->vector.match.labels != NULL) {
                        match_set = node->vector.match.labels;
                        for (size_t i=0; i < match_set->num; i++) {
                            metric_match_pair_t *pair = match_set->ptr[i];
                            if (pair == NULL)
                                continue;
                            fprintf(stream, "%s%s%s", pair->name, mql_match_op2str(pair->op),
                                                      pair->value.string);
                        }
                    }
                    fputs ("}", stream);
                }
            } else if (node->vector.match.labels != NULL) {
                fputs ("{", stream);
                metric_match_set_t *match_set = node->vector.match.labels;
                for (size_t i=0; i < match_set->num; i++) {
                    metric_match_pair_t *pair = match_set->ptr[i];
                    if (pair == NULL)
                        continue;
                    fprintf(stream, "%s%s%s", pair->name, mql_match_op2str(pair->op),
                                              pair->value.string);
                }
                fputs ("}", stream);
            }
            if (node->vector.offset != 0) {
                fprintf(stream, " offset %"PRId64"ms", node->vector.offset);
            }
            switch(node->vector.at.at) {
                case MQL_AT_NONE:
                    break;
                case MQL_AT_TIMESTAMP:
                    fprintf(stream, " @ %"PRId64, node->vector.at.timestamp);
                    break;
                case MQL_AT_START:
                    fputs(" @ start()", stream);
                    break;
                case MQL_AT_END:
                    fputs(" @ end()", stream);
                    break;
            }
            fputs (")\n", stream);
            break;
        case MQL_NODE_STRING:
            fprintf (stream, "string(%s)\n", node->string);
            break;
        case MQL_NODE_NUMBER:
            fprintf (stream, "number(%f)\n", node->number);
            break;
        case MQL_NODE_MATRIX:
            fprintf (stream, "matrix(%"PRIu64")\n", node->matrix.range);
            mql_node_dump_node(depth+1, max_depth, parent, 0, node->matrix.expr, stream);
            break;
    }

    return 0;
}

int mql_node_dump(mql_node_t *node, int max_depth, FILE *stream)
{
    int parent[max_depth+1];
    memset(parent, 0, (max_depth+1)*sizeof(int));
    return mql_node_dump_node(0, max_depth, parent, 0, node, stream);
}

