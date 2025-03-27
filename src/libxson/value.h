/* SPDX-License-Identifier: GPL-2.0-only OR MIT                     */
/* SPDX-FileCopyrightText: Copyright (c) 2010-2011  Florian Forster */
/* SPDX-FileContributor: Florian Forster  <ff at octo.it>           */
#pragma once

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

typedef enum {
    XSON_TYPE_NULL   = 0,
    XSON_TYPE_STRING = 1,
    XSON_TYPE_NUMBER = 2,
    XSON_TYPE_OBJECT = 3,
    XSON_TYPE_ARRAY  = 4,
    XSON_TYPE_TRUE   = 5,
    XSON_TYPE_FALSE  = 6,
} xson_type_t;

typedef struct xson_value_s xson_value_t;
typedef struct xson_keyval_s xson_keyval_t;

typedef struct {
    xson_keyval_t *keyval;
    size_t len;
} xson_object_t;

typedef struct {
    xson_value_t *values;
    size_t len;
} xson_array_t;

struct xson_value_s {
    xson_type_t type;
    union {
        char *string;
        double number;
        xson_object_t object;
        xson_array_t array;
    };
};

struct xson_keyval_s {
    char *key;
    xson_value_t value;
};

xson_value_t *xson_value_parse (const char *input, char *error_buffer, size_t error_buffer_size);

static inline xson_value_t *xson_value_alloc(xson_type_t type)
{
    xson_value_t *v = calloc(1, sizeof(*v));
    if (v == NULL)
        return NULL;
    v->type = type;
    return v;
}

void xson_value_free (xson_value_t *v);
void xson_value_clear (xson_value_t *v);

xson_value_t *xson_value_clone(xson_value_t *val);

static inline xson_type_t xson_value_type(xson_value_t *val)
{
    return val->type;
}

const char *xson_value_type_name(xson_value_t *val);

static inline int xson_value_set_string(xson_value_t *val, const char *string)
{
    val->type = XSON_TYPE_STRING;
    val->string = strdup(string);
    if (val->string == NULL)
        return -1;
    return 0;
}

static inline int xson_value_set_number(xson_value_t *val, double number)
{
    val->type = XSON_TYPE_NUMBER;
    val->number = number;
    return 0;
}

static inline int xson_value_set_object(xson_value_t *val)
{
    val->type = XSON_TYPE_OBJECT;
    val->object.len = 0;
    val->object.keyval = NULL;
    return 0;
}

static inline int xson_value_set_array(xson_value_t *val)
{
    val->type = XSON_TYPE_ARRAY;
    val->array.len = 0;
    val->array.values = NULL;
    return 0;
}

static inline int xson_value_set_true(xson_value_t *val)
{
    val->type = XSON_TYPE_TRUE;
    return 0;
}

static inline int xson_value_set_false(xson_value_t *val)
{
    val->type = XSON_TYPE_FALSE;
    return 0;
}

static inline int xson_value_set_null(xson_value_t *val)
{
    val->type = XSON_TYPE_NULL;
    return 0;
}

static inline xson_value_t *xson_value_alloc_string(const char *string)
{
    xson_value_t *val = xson_value_alloc(XSON_TYPE_STRING);
    if (val == NULL)
        return NULL;
    val->string = strdup(string);
    if (val->string == NULL) {
        xson_value_free(val);
        return NULL;
    }
    return val;
}

static inline xson_value_t *xson_value_alloc_number(double number)
{
    xson_value_t *val = xson_value_alloc(XSON_TYPE_NUMBER);
    if (val == NULL)
        return NULL;
    val->number = number;
    return val;
}

static inline xson_value_t *xson_value_alloc_object(void)
{
    return xson_value_alloc(XSON_TYPE_OBJECT);
}

static inline xson_value_t *xson_value_alloc_array(void)
{
    return xson_value_alloc(XSON_TYPE_ARRAY);
}

static inline xson_value_t *xson_value_alloc_true(void)
{
    return xson_value_alloc(XSON_TYPE_TRUE);
}

static inline xson_value_t *xson_value_alloc_false(void)
{
    return xson_value_alloc(XSON_TYPE_FALSE);
}

static inline xson_value_t *xson_value_alloc_null(void)
{
    return xson_value_alloc(XSON_TYPE_NULL);
}

static inline size_t xson_value_array_size(xson_value_t *val)
{
    return val->array.len;
}

static inline size_t xson_value_object_size(xson_value_t *val)
{
    return val->object.len;
}

xson_value_t *xson_value_array_append(xson_value_t *val);
xson_value_t *xson_value_object_append(xson_value_t *val, char *name);

static inline bool xson_value_is_null(xson_value_t *val)
{
    return val->type == XSON_TYPE_NULL;
}

static inline bool xson_value_is_string(xson_value_t *val)
{
    return val->type == XSON_TYPE_STRING;
}

static inline bool xson_value_is_number(xson_value_t *val)
{
    return val->type == XSON_TYPE_NUMBER;
}

static inline bool xson_value_is_bool(xson_value_t *val)
{
    return (val->type == XSON_TYPE_TRUE) || (val->type == XSON_TYPE_FALSE);
}

static inline bool xson_value_is_true(xson_value_t *val)
{
    return val->type == XSON_TYPE_TRUE;
}

static inline bool xson_value_is_false(xson_value_t *val)
{
    return val->type == XSON_TYPE_FALSE;
}

static inline bool xson_value_is_array(xson_value_t *val)
{
    return val->type == XSON_TYPE_ARRAY;
}

static inline bool xson_value_is_object(xson_value_t *val)
{
    return val->type == XSON_TYPE_OBJECT;
}

static inline bool xson_value_is_scalar(xson_value_t *val)
{
    return (val->type == XSON_TYPE_NULL  ) ||
           (val->type == XSON_TYPE_STRING) ||
           (val->type == XSON_TYPE_NUMBER) ||
           (val->type == XSON_TYPE_TRUE  ) ||
           (val->type == XSON_TYPE_FALSE );
}

static inline xson_value_t *xson_value_object_find(xson_value_t *val, char *key)
{
    if (val->type == XSON_TYPE_OBJECT) {
        for (size_t i = 0; i < val->object.len; i++) {
            xson_keyval_t *kv = &val->object.keyval[i];
            if (strcmp(key, kv->key) == 0)
                return &kv->value;
        }
    }

    return NULL;
}

static inline  xson_value_t *xson_value_array_at(xson_value_t *val, size_t n)
{
    if (val->type == XSON_TYPE_ARRAY) {
        if (n >= val->array.len)
            return NULL;
        return &val->array.values[n];
    }

    return NULL;
}

int xson_value_to_number(xson_value_t *val);

int xson_value_to_string(xson_value_t *val);

int xson_value_to_boolean(xson_value_t *val);

int xson_value_cmp(const xson_value_t *val1, const xson_value_t *val2);
