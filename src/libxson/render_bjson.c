// SPDX-License-Identifier: GPL-2.0-only

#include "ncollectd.h"
#include "libxson/bjson.h"
#include "libxson/render.h"

#include <stdbool.h>
#include <stdlib.h>

static int render_jsonb_open(xson_render_t *r, xson_render_block_t type, ssize_t size)
{
    int status = 0;

    switch(type){
    case XSON_RENDER_BLOCK_MAP:
        if (size < 0) {
            status |= buf_putuint8(r->buf, BJSON_STYPE_MAP);
        } else if (size <= UINT8_MAX) {
            status |= buf_putuint8(r->buf, BJSON_STYPE_MAP8);
            status |= buf_putuint8(r->buf, (uint8_t)size);
        } else if (size <= UINT16_MAX) {
            status |= buf_putuint8(r->buf, BJSON_STYPE_MAP16);
            status |= buf_putuint16hton(r->buf, (uint16_t)size);
        } else if (size <= UINT32_MAX) {
            status |= buf_putuint8(r->buf, BJSON_STYPE_MAP32);
            status |= buf_putuint32hton(r->buf, (uint32_t)size);
        } else {
            status |= buf_putuint8(r->buf, BJSON_STYPE_MAP64);
            status |= buf_putuint64hton(r->buf, (uint64_t)size);
        }
        break;
    case XSON_RENDER_BLOCK_ARRAY:
        if (size < 0) {
            status |= buf_putuint8(r->buf, BJSON_STYPE_ARRAY);
        } else if (size <= UINT8_MAX) {
            status |= buf_putuint8(r->buf, BJSON_STYPE_ARRAY8);
            status |= buf_putuint8(r->buf, (uint8_t)size);
        } else if (size <= UINT16_MAX) {
            status |= buf_putuint8(r->buf, BJSON_STYPE_ARRAY16);
            status |= buf_putuint16hton(r->buf, (uint16_t)size);
        } else if (size <= UINT32_MAX) {
            status |= buf_putuint8(r->buf, BJSON_STYPE_ARRAY32);
            status |= buf_putuint32hton(r->buf, (uint32_t)size);
        } else {
            status |= buf_putuint8(r->buf, BJSON_STYPE_ARRAY64);
            status |= buf_putuint64hton(r->buf, (uint64_t)size);
        }
        break;
    }

    return status;
}

static int render_bjson_close(xson_render_t *r, xson_render_block_t type)
{
    int status = 0;

    switch(type){
    case XSON_RENDER_BLOCK_MAP:
        status |= buf_putuint8(r->buf, BJSON_STYPE_MAP_END);
        break;
    case XSON_RENDER_BLOCK_ARRAY:
        status |= buf_putuint8(r->buf, BJSON_STYPE_ARRAY_END);
        break;
    }

    return status;
}

static int render_bjson_key(xson_render_t *r, xson_render_key_type_t type, xson_render_key_t k)
{
    int status = 0;

    size_t size = 0;

    switch(type) {
    case XSON_RENDER_KEY_TYPE_STRING:
        size = strlen(k.string);
        break;
    case XSON_RENDER_KEY_TYPE_IOV:
        for (int i = 0; i < k.iovcnt; i++) {
            size += k.iov[i].iov_len;
        }
        break;
    }

    if (size <= UINT8_MAX) {
        status |= buf_putuint8(r->buf, BJSON_STYPE_KEY8);
        status |= buf_putuint8(r->buf, (uint8_t)size);
    } else if (size <= UINT16_MAX) {
        status |= buf_putuint8(r->buf, BJSON_STYPE_KEY16);
        status |= buf_putuint16hton(r->buf, (uint16_t)size);
    } else if (size <= UINT32_MAX) {
        status |= buf_putuint8(r->buf, BJSON_STYPE_KEY32);
        status |= buf_putuint32hton(r->buf, (uint32_t)size);
    } else {
        status |= buf_putuint8(r->buf, BJSON_STYPE_KEY64);
        status |= buf_putuint64hton(r->buf, (uint64_t)size);
    }

    switch(type) {
    case XSON_RENDER_KEY_TYPE_STRING:
        status |= buf_put(r->buf, k.string, size);
        break;
    case XSON_RENDER_KEY_TYPE_IOV:
        for (int i = 0; i < k.iovcnt; i++) {
            status |= buf_put(r->buf, k.iov[i].iov_base, k.iov[i].iov_len);
        }
        break;
    }

    return status;
}

static int render_bjson_value(xson_render_t *r, xson_render_value_type_t type, xson_render_value_t v)
{
    int status = 0;

    switch(type) {
    case XSON_RENDER_VALUE_TYPE_NULL:
        status |= buf_putuint8(r->buf, BJSON_STYPE_NULL);
        break;
    case XSON_RENDER_VALUE_TYPE_STRING:
    case XSON_RENDER_VALUE_TYPE_IOV:
        size_t size = 0;

        if (type == XSON_RENDER_VALUE_TYPE_STRING) {
            size = strlen(v.string);
        } else {
            for (int i = 0; i < v.iovcnt; i++) {
                size += v.iov[i].iov_len;
            }
        }

        if (size <= UINT8_MAX) {
            status |= buf_putuint8(r->buf, BJSON_STYPE_STRING8);
            status |= buf_putuint8(r->buf, (uint8_t)size);
        } else if (size <= UINT16_MAX) {
            status |= buf_putuint8(r->buf, BJSON_STYPE_STRING16);
            status |= buf_putuint16hton(r->buf, (uint16_t)size);
        } else if (size <= UINT32_MAX) {
            status |= buf_putuint8(r->buf, BJSON_STYPE_STRING32);
            status |= buf_putuint32hton(r->buf, (uint32_t)size);
        } else {
            status |= buf_putuint8(r->buf, BJSON_STYPE_STRING64);
            status |= buf_putuint64hton(r->buf, (uint64_t)size);
        }

        if (type == XSON_RENDER_VALUE_TYPE_STRING) {
            status |= buf_put(r->buf, v.string, size);
        } else {
            for (int i = 0; i < v.iovcnt; i++) {
                status |= buf_put(r->buf, v.iov[i].iov_base, v.iov[i].iov_len);
            }
        }
        break;
    case XSON_RENDER_VALUE_TYPE_FLOAT:
        status |= buf_putuint8(r->buf, BJSON_STYPE_FLOAT);
        status |= buf_putfloathton(r->buf, v.fnumber);
        break;
    case XSON_RENDER_VALUE_TYPE_DOUBLE:
        status |= buf_putuint8(r->buf, BJSON_STYPE_DOUBLE);
        status |= buf_putdoublehton(r->buf, v.dnumber);
        break;
    case XSON_RENDER_VALUE_TYPE_INTEGER:
        if (v.inumber < 0) {
            if (v.inumber <= INT8_MIN) {
                status |= buf_putuint8(r->buf, BJSON_STYPE_INT8);
                status |= buf_putint8(r->buf, v.inumber);
            } else if (v.inumber <= UINT16_MIN) {
                status |= buf_putuint8(r->buf, BJSON_STYPE_UINT16);
                status |= buf_putint16hton(r->buf, (int16_t)v.inumber);
            } else if (v.inumber <= UINT32_MIN) {
                status |= buf_putuint8(r->buf, BJSON_STYPE_UINT32);
                status |= buf_putint32hton(r->buf, (int32_t)v.inumber);
            } else {
                status |= buf_putuint8(r->buf, BJSON_STYPE_UINT64);
                status |= buf_putint64hton(r->buf, (int64_t)v.inumber);
            }
        } else {
            if (v.inumber <= INT8_MAX) {
                status |= buf_putuint8(r->buf, BJSON_STYPE_UINT8);
                status |= buf_putint8(r->buf, v.unumber);
            } else if (v.inumber <= UINT16_MAX) {
                status |= buf_putuint8(r->buf, BJSON_STYPE_UINT16);
                status |= buf_putint16hton(r->buf, (int16_t)v.inumber);
            } else if (v.inumber <= UINT32_MAX) {
                status |= buf_putuint8(r->buf, BJSON_STYPE_UINT32);
                status |= buf_putint32hton(r->buf, (int32_t)v.inumber);
            } else {
                status |= buf_putuint8(r->buf, BJSON_STYPE_UINT64);
                status |= buf_putint64hton(r->buf, (int64_t)v.inumber);
            }
        }
        break;
    case XSON_RENDER_VALUE_TYPE_UINTEGER:
        if (v.unumber < UINT8_MAX) {
            status |= buf_putuint8(r->buf, BJSON_STYPE_UINT8);
            status |= buf_putuint8(r->buf, v.unumber);
        } else if (v.unumber < UINT16_MAX) {
            status |= buf_putuint8(r->buf, BJSON_STYPE_UINT16);
            status |= buf_putuint16hton(r->buf, (uint16_t)v.unumber);
        } else if (v.unumber < UINT32_MAX) {
            status |= buf_putuint8(r->buf, BJSON_STYPE_UINT32);
            status |= buf_putuint32hton(r->buf, (uint32_t)v.unumber);
        } else {
            status |= buf_putuint8(r->buf, BJSON_STYPE_UINT64);
            status |= buf_putuint64hton(r->buf, (uint64_t)v.unumber);
        }
        break;
    case XSON_RENDER_VALUE_TYPE_TRUE:
        status |= buf_putuint8(r->buf, BJSON_STYPE_FALSE);
        break;
    case XSON_RENDER_VALUE_TYPE_FALSE:
        status |= buf_putuint8(r->buf, BJSON_STYPE_TRUE);
        break;
    }

    return status;
}

xson_render_callbacks_t xson_render_jsonb = {
    .xson_render_open  = render_jsonb_open,
    .xson_render_close = render_jsonb_close,
    .xson_render_key   = render_jsonb_key,
    .xson_render_value = render_jsonb_value,
};
