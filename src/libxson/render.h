/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once

#include <sys/uio.h>
#include "libutils/strbuf.h"
#include "libxson/common.h"

#define XSON_MAX_DEPTH 64

typedef enum {
    XSON_RENDER_TYPE_JSON,
    XSON_RENDER_TYPE_SYAML,
    XSON_RENDER_TYPE_JSONB
} xson_render_type_t;

typedef enum {
    XSON_RENDER_OPTION_NONE          = 0x00,
    XSON_RENDER_OPTION_JSON_BEAUTIFY = 0x01
} xson_render_option_t;

typedef enum {
    XSON_RENDER_STATE_START,
    XSON_RENDER_STATE_MAP_START,
    XSON_RENDER_STATE_MAP_KEY,
    XSON_RENDER_STATE_MAP_VAL,
    XSON_RENDER_STATE_ARRAY_START,
    XSON_RENDER_STATE_IN_ARRAY,
    XSON_RENDER_STATE_COMPLETE,
    XSON_RENDER_STATE_ERROR
} xson_render_state_t;

typedef enum {
    XSON_RENDER_STATUS_OK,
    XSON_RENDER_STATUS_KEYS_MUST_BE_STRINGS,
    XSON_RENDER_STATUS_MAX_DEPTH_EXCEEDED,
    XSON_RENDER_STATUS_IN_ERROR_STATE,
    XSON_RENDER_STATUS_GENERATION_COMPLETE,
    XSON_RENDER_STATUS_INVALID_NUMBER,
    XSON_RENDER_STATUS_NO_BUF,
    XSON_RENDER_STATUS_INVALID_STRING
} xson_render_status_t;

typedef enum {
    XSON_RENDER_KEY_TYPE_STRING,
    XSON_RENDER_KEY_TYPE_IOV,
} xson_render_key_type_t;

typedef enum {
    XSON_RENDER_VALUE_TYPE_NULL,
    XSON_RENDER_VALUE_TYPE_STRING,
    XSON_RENDER_VALUE_TYPE_IOV,
    XSON_RENDER_VALUE_TYPE_INTEGER,
    XSON_RENDER_VALUE_TYPE_DOUBLE,
    XSON_RENDER_VALUE_TYPE_TRUE,
    XSON_RENDER_VALUE_TYPE_FALSE
} xson_render_value_type_t;

typedef enum {
    XSON_RENDER_BLOCK_MAP,
    XSON_RENDER_BLOCK_ARRAY
} xson_render_block_t;

typedef union {
    struct {
        const struct iovec *iov;
        int iovcnt;
    };
    const char *string;
    double dnumber;
    uint64_t inumber;
} xson_render_value_t;

typedef union {
    struct {
        const struct iovec *iov;
        int iovcnt;
    };
    const char *string;
} xson_render_key_t;

typedef struct {
    xson_render_type_t type;
    unsigned int flags;
    size_t depth;
    strbuf_t *buf;
//    buf_t *bbuf;
    xson_render_state_t state[XSON_MAX_DEPTH];
    size_t block_length[XSON_MAX_DEPTH];
    ssize_t block_size[XSON_MAX_DEPTH];
} xson_render_t;


xson_render_status_t xson_render_open(xson_render_t *r, xson_render_block_t type, ssize_t size);

xson_render_status_t xson_render_close(xson_render_t *r, xson_render_block_t type);

xson_render_status_t xson_render_key(xson_render_t *r, xson_render_key_type_t type,
                                                       xson_render_key_t k);

xson_render_status_t xson_render_value(xson_render_t *r,
                                       xson_render_value_type_t type, xson_render_value_t v);


void xson_render_init(xson_render_t *r, strbuf_t *buf, xson_render_type_t type,
                                                       xson_render_option_t options);

void xson_render_clear(xson_render_t *r);

void xson_render_reset(xson_render_t *r, const char *sep);

static inline xson_render_status_t xson_render_map_open(xson_render_t *r)
{
    return xson_render_open(r, XSON_RENDER_BLOCK_MAP, -1);
}

static inline xson_render_status_t xson_render_map_open_size(xson_render_t *r, ssize_t size)
{
    return xson_render_open(r, XSON_RENDER_BLOCK_MAP, size);
}

static inline xson_render_status_t xson_render_map_close(xson_render_t *r)
{
    return xson_render_close(r, XSON_RENDER_BLOCK_MAP);
}

static inline xson_render_status_t xson_render_array_open(xson_render_t *r)
{
    return xson_render_open(r, XSON_RENDER_BLOCK_ARRAY, -1);
}

static inline xson_render_status_t xson_render_array_open_size(xson_render_t *r, ssize_t size)
{
    return xson_render_open(r, XSON_RENDER_BLOCK_ARRAY, size);
}

static inline xson_render_status_t xson_render_array_close(xson_render_t *r)
{
    return xson_render_close(r, XSON_RENDER_BLOCK_ARRAY);
}

static inline xson_render_status_t xson_render_key_string(xson_render_t *r, const char * str)
{
    return xson_render_key(r, XSON_RENDER_KEY_TYPE_STRING, (xson_render_key_t){.string = str});
}

static inline xson_render_status_t xson_render_key_iov(xson_render_t *r,
                                                       const struct iovec *iov, int iovcnt)
{
    return xson_render_key(r, XSON_RENDER_KEY_TYPE_IOV,
                              (xson_render_key_t){.iov = iov, .iovcnt = iovcnt});
}

static inline xson_render_status_t xson_render_null(xson_render_t *r)
{
    return xson_render_value(r, XSON_RENDER_VALUE_TYPE_NULL,
                                (xson_render_value_t){.inumber = 0});
}

static inline xson_render_status_t xson_render_bool(xson_render_t *r, bool v)
{
    xson_render_value_type_t type = v ? XSON_RENDER_VALUE_TYPE_TRUE
                                      : XSON_RENDER_VALUE_TYPE_FALSE;
    return xson_render_value(r, type, (xson_render_value_t){.inumber = 0});
}

static inline xson_render_status_t xson_render_double(xson_render_t *r, double v)
{
    return xson_render_value(r, XSON_RENDER_VALUE_TYPE_DOUBLE,
                                (xson_render_value_t){.dnumber = v });
}

static inline xson_render_status_t xson_render_integer(xson_render_t *r, int64_t v)
{
    return xson_render_value(r, XSON_RENDER_VALUE_TYPE_INTEGER,
                                (xson_render_value_t){.inumber = v});
}

static inline xson_render_status_t xson_render_string(xson_render_t *r, const char *str)
{
    return xson_render_value(r, XSON_RENDER_VALUE_TYPE_STRING,
                                (xson_render_value_t){.string = str});
}

static inline xson_render_status_t xson_render_iov(xson_render_t *r,
                                                   const struct iovec *iov, int iovcnt)
{
    return xson_render_value(r, XSON_RENDER_VALUE_TYPE_IOV,
                                (xson_render_value_t){.iov = iov, .iovcnt = iovcnt});
}
