/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <regex.h>

typedef enum {
    EXPR_VALUE_NUMBER,
    EXPR_VALUE_STRING,
    EXPR_VALUE_BOOLEAN
} expr_value_type_t;

struct expr_value;
typedef struct expr_value expr_value_t;

struct expr_value {
    expr_value_type_t type;
    union {
        double number;
        bool boolean;
        char *string;
    };
};

typedef enum {
    EXPR_ID_NAME,
    EXPR_ID_IDX
} expr_id_type_t;

typedef struct {
    expr_id_type_t type;
    union {
        char *name;
        int idx;
    };
} expr_id_item_t;

typedef struct {
    size_t num;
    expr_id_item_t *ptr;
} expr_id_t;

struct expr_symtab;
typedef struct expr_symtab expr_symtab_t;

typedef enum {
    EXPR_SYMTAB_VALUE,
    EXPR_SYMTAB_VALUE_REF,
    EXPR_SYMTAB_CALLBACK,
} expr_symtab_entry_type_t;

struct expr_symtab_entry;
typedef struct  expr_symtab_entry expr_symtab_entry_t;

typedef expr_value_t *(*expr_symtab_cb_t)(expr_id_t *id, void *data);

struct expr_symtab_entry {
    expr_symtab_entry_type_t type;
    expr_id_t *id;
    union {
        expr_value_t value;
        expr_value_t *value_ref;
        struct {
            void *data;
            expr_symtab_cb_t cb;
        };
    };
};

typedef enum {
    EXPR_STRING,
    EXPR_NUMBER,
    EXPR_BOOL,
    EXPR_IDENTIFIER,

    EXPR_AND,
    EXPR_OR,
    EXPR_NOT,
    EXPR_EQL,
    EXPR_NQL,
    EXPR_LT,
    EXPR_GT,
    EXPR_LTE,
    EXPR_GTE,
    EXPR_MATCH,
    EXPR_NMATCH,

    EXPR_ADD,
    EXPR_SUB,
    EXPR_MUL,
    EXPR_DIV,
    EXPR_MOD,
    EXPR_MINUS,

    EXPR_BIT_AND,
    EXPR_BIT_OR,
    EXPR_BIT_XOR,
    EXPR_BIT_LSHIFT,
    EXPR_BIT_RSHIFT,
    EXPR_BIT_NOT,

    EXPR_IF,

    EXPR_FUNC_RANDOM,
    EXPR_FUNC_TIME,
    EXPR_FUNC_EXP,
    EXPR_FUNC_EXPM1,
    EXPR_FUNC_LOG,
    EXPR_FUNC_LOG2,
    EXPR_FUNC_LOG10,
    EXPR_FUNC_LOG1P,
    EXPR_FUNC_SQRT,
    EXPR_FUNC_CBRT,
    EXPR_FUNC_SIN,
    EXPR_FUNC_COS,
    EXPR_FUNC_TAN,
    EXPR_FUNC_ASIN,
    EXPR_FUNC_ACOS,
    EXPR_FUNC_ATAN,
    EXPR_FUNC_COSH,
    EXPR_FUNC_SINH,
    EXPR_FUNC_TANH,
    EXPR_FUNC_CTANH,
    EXPR_FUNC_ACOSH,
    EXPR_FUNC_ASINH,
    EXPR_FUNC_ATANH,
    EXPR_FUNC_ABS,
    EXPR_FUNC_CEIL,
    EXPR_FUNC_FLOOR,
    EXPR_FUNC_ROUND,
    EXPR_FUNC_TRUNC,
    EXPR_FUNC_ISNAN,
    EXPR_FUNC_ISINF,
    EXPR_FUNC_ISNORMAL,
    EXPR_FUNC_POW,
    EXPR_FUNC_HYPOT,
    EXPR_FUNC_ATAN2,
    EXPR_FUNC_MAX,
    EXPR_FUNC_MIN,

    EXPR_AGG_SUM,
    EXPR_AGG_AVG,
    EXPR_AGG_ALL,
    EXPR_AGG_ANY,
} expr_node_type_t;

struct expr_node;
typedef struct expr_node expr_node_t;

struct expr_node {
    expr_node_type_t type;
    union {
        struct {
            expr_node_t *left;
            expr_node_t *right;
        };
        struct {
            expr_node_t *expr;
            expr_node_t *expr_then;
            expr_node_t *expr_else;
        };
        struct {
            expr_node_t *arg0;
            expr_node_t *arg1;
        };
        struct {
            char *pattern;
            expr_node_t *regex_expr;
            regex_t regex;
        };
        struct {
            expr_id_t id;
            expr_symtab_entry_t *entry;
        };
        struct {
            expr_node_t *loop_id;
            expr_node_t *loop_start;
            expr_node_t *loop_end;
            expr_node_t *loop_step;
            expr_node_t *loop_expr;
        };
        expr_node_t *arg;
        char *string;
        double number;
        bool boolean;
    };
};

#define EXPR_PARSE_RESULT_ERROR_MSG_SIZE 245

typedef struct {
    expr_symtab_t *symtab;
    expr_node_t *root;
    bool error;
    char error_msg[EXPR_PARSE_RESULT_ERROR_MSG_SIZE];
} expr_parse_result_t;

expr_value_t *expr_value_alloc_bool(bool boolean);
expr_value_t *expr_value_alloc_number(double number);
expr_value_t *expr_value_alloc_string(char *str);
bool expr_value_to_bool(expr_value_t *value);

expr_symtab_t *expr_symtab_alloc(void);
void expr_symtab_free(expr_symtab_t *symtab);
void expr_symtab_entry_free(expr_symtab_entry_t *entry);
int expr_symtab_append_number(expr_symtab_t *symtab, expr_id_t *id, double value);
int expr_symtab_append_boolean(expr_symtab_t *symtab, expr_id_t *id, bool boolean);
int expr_symtab_append_string(expr_symtab_t *symtab, expr_id_t *id, char *str);
int expr_symtab_append_value(expr_symtab_t *symtab, expr_id_t *id, expr_value_t *value);
int expr_symtab_append_name_value(expr_symtab_t *symtab, char *name,  expr_value_t *value);
int expr_symtab_append_callback(expr_symtab_t *symtab, expr_id_t *id, expr_symtab_cb_t cb, void *data);
int expr_symtab_default(expr_symtab_t *symtab, expr_symtab_cb_t cb, void *data);
expr_symtab_entry_t *expr_symtab_lookup(expr_symtab_t *symtab, expr_id_t *id);

void expr_node_free(expr_node_t *node);
void expr_value_free(expr_value_t *value);

expr_node_t *expr_parse(char *str, expr_symtab_t *symtab);
expr_value_t *expr_eval(expr_node_t *node);
