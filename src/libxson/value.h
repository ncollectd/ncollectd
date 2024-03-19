/* SPDX-License-Identifier: GPL-2.0-only OR MIT                     */
/* SPDX-FileCopyrightText: Copyright (c) 2010-2011  Florian Forster */
/* SPDX-FileContributor: Florian Forster  <ff at octo.it>           */
#pragma once

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

typedef enum {
    JSON_TYPE_NULL   = 0,
    JSON_TYPE_STRING = 1,
    JSON_TYPE_NUMBER = 2,
    JSON_TYPE_OBJECT = 3,
    JSON_TYPE_ARRAY  = 4,
    JSON_TYPE_TRUE   = 5,
    JSON_TYPE_FALSE  = 6,
} json_type_t;

typedef struct json_value_s json_value_t;
typedef struct json_keyval_s json_keyval_t;

typedef struct {
    json_keyval_t *keyval;
    size_t len;
} json_object_t;

typedef struct {
    json_value_t *values;
    size_t len;
} json_array_t;

struct json_value_s {
    json_type_t type;
    union {
        char *string;
        double number;
        json_object_t object;
        json_array_t array;
    };
};

struct json_keyval_s {
    char *key;
    json_value_t value;
};

json_value_t *json_value_parse (const char *input, char *error_buffer, size_t error_buffer_size);

static inline json_value_t *json_value_alloc(json_type_t type)
{
    json_value_t *v = calloc(1, sizeof(*v));
    if (v == NULL)
        return NULL;
    v->type = type;
    return v;
}

void json_value_free (json_value_t *v);
void json_value_clear (json_value_t *v);

json_value_t *json_value_clone(json_value_t *val);

static inline json_type_t json_value_type(json_value_t *val)
{
    return val->type;
}

const char *json_value_type_name(json_value_t *val);

static inline int json_value_set_string(json_value_t *val, const char *string)
{
    val->type = JSON_TYPE_STRING;
    val->string = strdup(string);
    if (val->string == NULL)
        return -1;
    return 0;
}

static inline int json_value_set_number(json_value_t *val, double number)
{
    val->type = JSON_TYPE_NUMBER;
    val->number = number;
    return 0;
}

static inline int json_value_set_object(json_value_t *val)
{
    val->type = JSON_TYPE_OBJECT;
    val->object.len = 0;
    val->object.keyval = NULL;
    return 0;
}

static inline int json_value_set_array(json_value_t *val)
{
    val->type = JSON_TYPE_ARRAY;
    val->array.len = 0;
    val->array.values = NULL;
    return 0;
}

static inline int json_value_set_true(json_value_t *val)
{
    val->type = JSON_TYPE_TRUE;
    return 0;
}

static inline int json_value_set_false(json_value_t *val)
{
    val->type = JSON_TYPE_FALSE;
    return 0;
}

static inline int json_value_set_null(json_value_t *val)
{
    val->type = JSON_TYPE_NULL;
    return 0;
}

static inline json_value_t *json_value_alloc_string(const char *string)
{
    json_value_t *val = json_value_alloc(JSON_TYPE_STRING);
    if (val == NULL)
        return NULL;
    val->string = strdup(string);
    if (val->string == NULL) {
        json_value_free(val);
        return NULL;
    }
    return val;
}

static inline json_value_t *json_value_alloc_number(double number)
{
    json_value_t *val = json_value_alloc(JSON_TYPE_NUMBER);
    if (val == NULL)
        return NULL;
    val->number = number;
    return val;
}

static inline json_value_t *json_value_alloc_object(void)
{
    return json_value_alloc(JSON_TYPE_OBJECT);
}

static inline json_value_t *json_value_alloc_array(void)
{
    return json_value_alloc(JSON_TYPE_ARRAY);
}

static inline json_value_t *json_value_alloc_true(void)
{
    return json_value_alloc(JSON_TYPE_TRUE);
}

static inline json_value_t *json_value_alloc_false(void)
{
    return json_value_alloc(JSON_TYPE_FALSE);
}

static inline json_value_t *json_value_alloc_null(void)
{
    return json_value_alloc(JSON_TYPE_NULL);
}

static inline size_t json_value_array_size(json_value_t *val)
{
    return val->array.len;
}

static inline size_t json_value_object_size(json_value_t *val)
{
    return val->object.len;
}

json_value_t *json_value_array_append(json_value_t *val);
json_value_t *json_value_object_append(json_value_t *val, char *name);

static inline bool json_value_is_null(json_value_t *val)
{
    return val->type == JSON_TYPE_NULL;
}

static inline bool json_value_is_string(json_value_t *val)
{
    return val->type == JSON_TYPE_STRING;
}

static inline bool json_value_is_number(json_value_t *val)
{
    return val->type == JSON_TYPE_NUMBER;
}

static inline bool json_value_is_bool(json_value_t *val)
{
    return (val->type == JSON_TYPE_TRUE) || (val->type == JSON_TYPE_FALSE);
}

static inline bool json_value_is_true(json_value_t *val)
{
    return val->type == JSON_TYPE_TRUE;
}

static inline bool json_value_is_false(json_value_t *val)
{
    return val->type == JSON_TYPE_FALSE;
}

static inline bool json_value_is_array(json_value_t *val)
{
    return val->type == JSON_TYPE_ARRAY;
}

static inline bool json_value_is_object(json_value_t *val)
{
    return val->type == JSON_TYPE_OBJECT;
}

static inline bool json_value_is_scalar(json_value_t *val)
{
    return (val->type == JSON_TYPE_NULL  ) ||
           (val->type == JSON_TYPE_STRING) ||
           (val->type == JSON_TYPE_NUMBER) ||
           (val->type == JSON_TYPE_TRUE  ) ||
           (val->type == JSON_TYPE_FALSE );
}

static inline json_value_t *json_value_object_find(json_value_t *val, char *key)
{
    if (val->type == JSON_TYPE_OBJECT) {
        for (size_t i = 0; i < val->object.len; i++) {
            json_keyval_t *kv = &val->object.keyval[i];
            if (strcmp(key, kv->key) == 0)
                return &kv->value;
        }
    }

    return NULL;
}

static inline  json_value_t *json_value_array_at(json_value_t *val, size_t n)
{
    if (val->type == JSON_TYPE_ARRAY) {
        if (n >= val->array.len)
            return NULL;
        return &val->array.values[n];
    }

    return NULL;
}

int json_value_to_number(json_value_t *val);

int json_value_to_string(json_value_t *val);

int json_value_to_boolean(json_value_t *val);
