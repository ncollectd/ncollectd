/* SPDX-License-Identifier: GPL-2.0-only or PostgreSQL                                  */
/* SPDX-FileCopyrightText: Copyright (c) 2019-2023, PostgreSQL Global Development Group */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <sys/uio.h>
#include <regex.h>

#include "libutils/strbuf.h"
#include "libxson/value.h"
#include "jsonpath_list.h"

typedef struct {
    int32_t   vl_len_;     /* varlena header (do not touch directly!) */
    uint32_t  header;      /* version and flags (see below) */
    char data[];
} jsonpath_t;

#define JSONPATH_VERSION  (0x01)
#define JSONPATH_HDRSZ    (offsetof(jsonpath_t, data))

#define jspIsScalar(type) ((type) >= JSONPATH_NULL && (type) <= JSONPATH_BOOL)


typedef enum {
    JSONPATH_NULL,                   /* NULL literal */
    JSONPATH_STRING,                 /* string literal */
    JSONPATH_NUMERIC,                /* numeric literal */
    JSONPATH_BOOL,                   /* boolean literal: TRUE or FALSE */
    JSONPATH_AND,                    /* predicate && predicate */
    JSONPATH_OR,                     /* predicate || predicate */
    JSONPATH_NOT,                    /* ! predicate */
    JSONPATH_EQUAL,                  /* expr == expr */
    JSONPATH_NOTEQUAL,               /* expr != expr */
    JSONPATH_LESS,                   /* expr < expr */
    JSONPATH_GREATER,                /* expr > expr */
    JSONPATH_LESSOREQUAL,            /* expr <= expr */
    JSONPATH_GREATEROREQUAL,         /* expr >= expr */
    JSONPATH_REGEX,                  /* expr ~= regex */
    JSONPATH_ADD,                    /* expr + expr */
    JSONPATH_SUB,                    /* expr - expr */
    JSONPATH_MUL,                    /* expr * expr */
    JSONPATH_DIV,                    /* expr / expr */
    JSONPATH_MOD,                    /* expr % expr */
    JSONPATH_PLUS,                   /* + expr */
    JSONPATH_MINUS,                  /* - expr */
    JSONPATH_ANYARRAY,               /* [*] */
    JSONPATH_ANYKEY,                 /* .* */
    JSONPATH_INDEXARRAY,             /* [subscript, ...] */
    JSONPATH_SLICE,                  /* [ start : end : step ] */
    JSONPATH_KEY,                    /* .key */
    JSONPATH_UNION,
    JSONPATH_DSC_UNION,
    JSONPATH_CURRENT,                /* @ */
    JSONPATH_ROOT,                   /* $ */
    JSONPATH_FILTER,                 /* ? (predicate) */
    JSONPATH_LENGTH,                 /* length() item method */
    JSONPATH_COUNT,                  /* size() item method */
    JSONPATH_AVG,                    /* avg() item method */
    JSONPATH_MAX,                    /* max() item method */
    JSONPATH_MIN,                    /* min() item method */
    JSONPATH_ABS,                    /* .abs() item method */
    JSONPATH_FLOOR,                  /* .floor() item method */
    JSONPATH_CEILING,                /* .ceiling() item method */
    JSONPATH_DOUBLE,                 /* .double() item method */
    JSONPATH_SUBSCRIPT,              /* array subscript: 'expr' or 'expr TO expr' */
} jsonpath_item_type_t;

typedef struct jsonpath_item jsonpath_item_t;

struct jsonpath_item {
    jsonpath_item_type_t type;
    jsonpath_item_t *next;    /* next in path */
    jsonpath_item_t *shadow;  // FIXME
    union {
        /* classic operator with two operands: and, or etc */
        struct {
            jsonpath_item_t *left;
            jsonpath_item_t *right;
        } args;
        /* any unary operation */
        jsonpath_item_t *arg;
        struct {
            int32_t len;
            jsonpath_item_t **items;
        } iunion;
        /* storage for JSONPATH_INDEXARRAY: indexes of array */
        struct {
            int32_t idx;
        } array;
        /* JSONPATH_ANY: levels */
        struct {
            uint32_t first;
            uint32_t last;
        } anybounds;
        struct {
            int32_t start;
            int32_t end;
            int32_t step;
        } slice;
        struct {
            jsonpath_item_t *expr;
            regex_t regex;
            int flags;
            char *pattern;
        } regex;
        /* scalars */
        double numeric;
        bool boolean;
        struct {
            uint32_t len;
            char *val;    /* could not be not null-terminated */
        } string;
    } value;
};

#define JSONPATH_ERROR_MSG_SIZE 246

typedef struct {
    jsonpath_item_t *expr;
    bool error;
    char error_msg[JSONPATH_ERROR_MSG_SIZE];
    bool lax;
} jsonpath_parse_result_t;

typedef enum {
    JSONPATH_EXEC_RESULT_OK        = 0,
    JSONPATH_EXEC_RESULT_NOT_FOUND = 1,
    JSONPATH_EXEC_RESULT_ERROR     = 2
} jsonpath_exec_result_t;

typedef struct {
    json_value_t *singleton;
    jsonpath_list_t *list;
} json_value_list_t;

typedef struct {
    json_value_t *value;
    jsonpath_list_t *list;
    jsonpath_list_cell_t *next;
} json_value_list_iterator_t;

jsonpath_item_t *jsonpath_parser(const char *str);

void jsonpath_item_free(jsonpath_item_t *item);

void jsonpath_print_item(strbuf_t *buf, jsonpath_item_t *v, bool inKey, bool printBracketes);

jsonpath_exec_result_t jsonpath_exec(jsonpath_item_t *jsp,
                                     json_value_t *json, bool throwErrors,
                                     json_value_list_t *result);

void json_value_list_destroy(json_value_list_t *jvl);

void json_value_list_init_iterator(const json_value_list_t *jvl, json_value_list_iterator_t *it);
json_value_t *json_value_list_next(const json_value_list_t *jvl, json_value_list_iterator_t *it);


#if 0
extern bool jspConvertRegexFlags(uint32_t xflags, int *result);
#endif

static inline bool jsonpath_has_next(jsonpath_item_t *v)
{
    return v->next != NULL;
}

static inline jsonpath_item_t *jsonpath_get_next(jsonpath_item_t *v)
{
    return v->next;
}

static inline jsonpath_item_t *jsonpath_get_arg(jsonpath_item_t *v)
{
    return v->value.arg;
}

static inline jsonpath_item_t *jsonpath_get_left_arg(jsonpath_item_t *v)
{
    return v->value.args.left;
}

static inline jsonpath_item_t *jsonpath_get_right_arg(jsonpath_item_t *v)
{
    return v->value.args.right;
}

static inline double jsonpath_get_numeric(jsonpath_item_t *v)
{
    return v->value.numeric;
}

static inline bool jsonpath_get_bool(jsonpath_item_t *v)
{
    return v->value.boolean;
}

static inline char *jsonpath_get_string(jsonpath_item_t *v, int32_t *len)
{
    if (len != NULL)
        *len = v->value.string.len;
    return v->value.string.val;
}

const char *jsonpath_operation_name(jsonpath_item_type_t type);
