/* SPDX-License-Identifier: GPL-2.0-only OR ISC         */
/* Copyright (c) 2007-2014, Lloyd Hilaiel <me@lloyd.io> */
/* From  http://github.com/lloyd/yajl                   */
#pragma once

#include "libxson/common.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define JSON_BUF_INIT_SIZE 2048

/**
 * json_buf is a buffer with exponential growth.  the buffer ensures that
 * you are always null padded.
 */

static inline void json_buf_ensure_available(json_buf_t *buf, size_t want)
{
    assert(buf != NULL);

    /* first call */
    if (buf->data == NULL) {
        buf->len = JSON_BUF_INIT_SIZE;
        buf->data = malloc(buf->len);
        buf->data[0] = 0;
    }

    size_t need = buf->len;

    while (want >= (need - buf->used)) need <<= 1;

    if (need != buf->len) {
        buf->data = realloc(buf->data, need);
        buf->len = need;
    }
}

static inline void json_buf_free(json_buf_t *buf)
{
    assert(buf != NULL);
    if (buf->data)
        free(buf->data);
}

static inline void json_buf_append(json_buf_t *buf, const void * data, size_t len)
{
    json_buf_ensure_available(buf, len);
    if (len > 0) {
        assert(data != NULL);
        memcpy(buf->data + buf->used, data, len);
        buf->used += len;
        buf->data[buf->used] = 0;
    }
}

static inline void json_buf_clear(json_buf_t *buf)
{
    buf->used = 0;
    if (buf->data)
        buf->data[buf->used] = 0;
}

static inline const unsigned char * json_buf_data(json_buf_t *buf)
{
    return buf->data;
}

static inline size_t json_buf_len(json_buf_t *buf)
{
    return buf->used;
}

static inline void json_buf_truncate(json_buf_t *buf, size_t len)
{
    assert(len <= buf->used);
    buf->used = len;
}
