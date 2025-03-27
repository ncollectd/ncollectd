/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    XSON_STRUCT_TYPE_NONE,
    XSON_STRUCT_TYPE_ARRAY,
    XSON_STRUCT_TYPE_MAP,
    XSON_STRUCT_TYPE_OBJECT,
    XSON_STRUCT_TYPE_BOOLEAN,
    XSON_STRUCT_TYPE_INT64,
    XSON_STRUCT_TYPE_UINT64,
    XSON_STRUCT_TYPE_DOUBLE,
    XSON_STRUCT_TYPE_STRING
} xson_struct_type_t;

struct xson_struct_attr;
typedef struct xson_struct_attr xson_struct_attr_t;

typedef struct {
    size_t struct_size;
    xson_struct_attr_t *attrs;
} xson_struct_object_t;

typedef struct {
    xson_struct_type_t type;
    size_t offset_size;
    xson_struct_object_t object;
} xson_struct_array_t;

struct xson_struct_attr {
    char *attr;
    size_t attr_size;
    xson_struct_type_t type;
    size_t offset;
    union {
        xson_struct_array_t array;
        xson_struct_object_t object;
        bool boolean;
        int64_t int64;
        uint64_t uint64;
        double float64;
        char *string;
    };
};

#define XSON_STRUCT_ARRAY_MAP(a, s, f, nf, ss, ja) \
    .attr = a, \
    .attr_size = sizeof(a)-1, \
    .type = XSON_STRUCT_TYPE_MAP, \
    .offset = offsetof(s, f),\
    .array.type = XSON_STRUCT_TYPE_MAP, \
    .array.offset_size = offsetof(s, nf), \
    .object.size = sizeof(ss), \
    .object.attr = ja,

#define XSON_STRUCT_ARRAY_OBJECT(a, s, f, nf, ss, ja) \
    .attr = a, \
    .attr_size = sizeof(a)-1, \
    .type = XSON_STRUCT_TYPE_ARRAY, \
    .offset = offsetof(s, f),\
    .array.type = XSON_STRUCT_TYPE_OBJECT, \
    .array.offset_size = offsetof(s, nf), \
    .object.size = sizeof(ss), \
    .object.attr = ja,

#define XSON_STRUCT_ARRAY_BOOLEAN(a, s, f, nf) \
    { .attr = a, .attr_size = sizeof(a)-1, .type = XSON_STRUCT_TYPE_ARRAY, \
      .offset = offsetof(s, f), \
      .array.type = XSON_STRUCT_TYPE_BOOLEAN, .array.offset_size = offsetof(s, nf) }
#define XSON_STRUCT_ARRAY_INT64(a, s, f, nf) \
    { .attr = a, .attr_size = sizeof(a)-1, .type = XSON_STRUCT_TYPE_ARRAY, \
      .offset = offsetof(s, f), \
      .array.type = XSON_STRUCT_TYPE_INT64, .array.offset_size = offsetof(s, nf) }
#define XSON_STRUCT_ARRAY_UINT64(a, s, f, nf) \
    { .attr = a, .attr_size = sizeof(a)-1, .type = XSON_STRUCT_TYPE_ARRAY, \
      .offset = offsetof(s, f), \
      .array.type = XSON_STRUCT_TYPE_UINT64, .array.offset_size = offsetof(s, nf) }
#define XSON_STRUCT_ARRAY_DOUBLE(a, s, f, nf) \
    { .attr = a, .attr_size = sizeof(a)-1, .type = XSON_STRUCT_TYPE_ARRAY, \
      .offset = offsetof(s, f), \
      .array.type = XSON_STRUCT_TYPE_DOUBLE, .array.offset_size = offsetof(s, nf) }
#define XSON_STRUCT_ARRAY_STRING(a, s, f, nf) \
    { .attr = a, .attr_size = sizeof(a)-1, .type = XSON_STRUCT_TYPE_ARRAY, \
      .offset = offsetof(s, f), \
      .array.type = XSON_STRUCT_TYPE_STRING, .array.offset_size = offsetof(s, nf) }

#define XSON_STRUCT_OBJECT(a, s, f, ss, ja) \
    { .attr = a, .attr_size = sizeof(a)-1, .type = XSON_STRUCT_TYPE_OBJECT, \
      .offset = offsetof(s, f), .object.struct_size = sizeof(ss), .object.attrs = ja }
#define XSON_STRUCT_BOOLEAN(a, s, f, d) \
    { .attr = a, .attr_size = sizeof(a)-1, .type = XSON_STRUCT_TYPE_BOOLEAN, \
      .offset = offsetof(s, f), .boolean = d }
#define XSON_STRUCT_INT64(a, s, f, d) \
    { .attr = a, .attr_size = sizeof(a)-1, .type = XSON_STRUCT_TYPE_INT64, \
      .offset = offsetof(s, f), .int64 = d }
#define XSON_STRUCT_UINT64(a, s, f, d) \
    { .attr = a, .attr_size = sizeof(a)-1, .type = XSON_STRUCT_TYPE_UINT64, \
      .offset = offsetof(s, f), .uint64 = d }
#define XSON_STRUCT_DOUBLE(a, s, f, d) \
    { .attr = a, .attr_size = sizeof(a)-1, .type = XSON_STRUCT_TYPE_DOUBLE, \
      .offset = offsetof(s, f), .float64 = d }
#define XSON_STRUCT_STRING(a, s, f, d) \
    { .attr = a, .attr_size = sizeof(a)-1, .type = XSON_STRUCT_TYPE_STRING, \
      .offset = offsetof(s, f), .string = d }

#define XSON_STRUCT_END \
    { .type = XSON_STRUCT_TYPE_NONE }
