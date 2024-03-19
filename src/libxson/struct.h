/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    JSON_STRUCT_TYPE_NONE,
    JSON_STRUCT_TYPE_ARRAY,
    JSON_STRUCT_TYPE_MAP,
    JSON_STRUCT_TYPE_OBJECT,
    JSON_STRUCT_TYPE_BOOLEAN,
    JSON_STRUCT_TYPE_INT64,
    JSON_STRUCT_TYPE_UINT64,
    JSON_STRUCT_TYPE_DOUBLE,
    JSON_STRUCT_TYPE_STRING
} json_struct_type_t;

struct json_struct_attr;
typedef struct json_struct_attr json_struct_attr_t;

typedef struct {
    size_t struct_size;
    json_struct_attr_t *attrs;
} json_struct_object_t;

typedef struct {
    json_struct_type_t type;
    size_t offset_size;
    json_struct_object_t object;
} json_struct_array_t;

struct json_struct_attr {
    char *attr;
    size_t attr_size;
    json_struct_type_t type;
    size_t offset;
    union {
        json_struct_array_t array;
        json_struct_object_t object;
        bool boolean;
        int64_t int64;
        uint64_t uint64;
        double float64;
        char *string;
    };
};

#define JSON_STRUCT_ARRAY_MAP(a, s, f, nf, ss, ja) \
    .attr = a, \
    .attr_size = sizeof(a)-1, \
    .type = JSON_STRUCT_TYPE_MAP, \
    .offset = offsetof(s, f),\
    .array.type = JSON_STRUCT_TYPE_MAP, \
    .array.offset_size = offsetof(s, nf), \
    .object.size = sizeof(ss), \
    .object.attr = ja,

#define JSON_STRUCT_ARRAY_OBJECT(a, s, f, nf, ss, ja) \
    .attr = a, \
    .attr_size = sizeof(a)-1, \
    .type = JSON_STRUCT_TYPE_ARRAY, \
    .offset = offsetof(s, f),\
    .array.type = JSON_STRUCT_TYPE_OBJECT, \
    .array.offset_size = offsetof(s, nf), \
    .object.size = sizeof(ss), \
    .object.attr = ja,

#define JSON_STRUCT_ARRAY_BOOLEAN(a, s, f, nf) \
    { .attr = a, .attr_size = sizeof(a)-1, .type = JSON_STRUCT_TYPE_ARRAY, \
      .offset = offsetof(s, f), \
      .array.type = JSON_STRUCT_TYPE_BOOLEAN, .array.offset_size = offsetof(s, nf) }
#define JSON_STRUCT_ARRAY_INT64(a, s, f, nf) \
    { .attr = a, .attr_size = sizeof(a)-1, .type = JSON_STRUCT_TYPE_ARRAY, \
      .offset = offsetof(s, f), \
      .array.type = JSON_STRUCT_TYPE_INT64, .array.offset_size = offsetof(s, nf) }
#define JSON_STRUCT_ARRAY_UINT64(a, s, f, nf) \
    { .attr = a, .attr_size = sizeof(a)-1, .type = JSON_STRUCT_TYPE_ARRAY, \
      .offset = offsetof(s, f), \
      .array.type = JSON_STRUCT_TYPE_UINT64, .array.offset_size = offsetof(s, nf) }
#define JSON_STRUCT_ARRAY_DOUBLE(a, s, f, nf) \
    { .attr = a, .attr_size = sizeof(a)-1, .type = JSON_STRUCT_TYPE_ARRAY, \
      .offset = offsetof(s, f), \
      .array.type = JSON_STRUCT_TYPE_DOUBLE, .array.offset_size = offsetof(s, nf) }
#define JSON_STRUCT_ARRAY_STRING(a, s, f, nf) \
    { .attr = a, .attr_size = sizeof(a)-1, .type = JSON_STRUCT_TYPE_ARRAY, \
      .offset = offsetof(s, f), \
      .array.type = JSON_STRUCT_TYPE_STRING, .array.offset_size = offsetof(s, nf) }

#define JSON_STRUCT_OBJECT(a, s, f, ss, ja) \
    { .attr = a, .attr_size = sizeof(a)-1, .type = JSON_STRUCT_TYPE_OBJECT, \
      .offset = offsetof(s, f), .object.struct_size = sizeof(ss), .object.attrs = ja }
#define JSON_STRUCT_BOOLEAN(a, s, f, d) \
    { .attr = a, .attr_size = sizeof(a)-1, .type = JSON_STRUCT_TYPE_BOOLEAN, \
      .offset = offsetof(s, f), .boolean = d }
#define JSON_STRUCT_INT64(a, s, f, d) \
    { .attr = a, .attr_size = sizeof(a)-1, .type = JSON_STRUCT_TYPE_INT64, \
      .offset = offsetof(s, f), .int64 = d }
#define JSON_STRUCT_UINT64(a, s, f, d) \
    { .attr = a, .attr_size = sizeof(a)-1, .type = JSON_STRUCT_TYPE_UINT64, \
      .offset = offsetof(s, f), .uint64 = d }
#define JSON_STRUCT_DOUBLE(a, s, f, d) \
    { .attr = a, .attr_size = sizeof(a)-1, .type = JSON_STRUCT_TYPE_DOUBLE, \
      .offset = offsetof(s, f), .float64 = d }
#define JSON_STRUCT_STRING(a, s, f, d) \
    { .attr = a, .attr_size = sizeof(a)-1, .type = JSON_STRUCT_TYPE_STRING, \
      .offset = offsetof(s, f), .string = d }

#define JSON_STRUCT_END \
    { .type = JSON_STRUCT_TYPE_NONE }
