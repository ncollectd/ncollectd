/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "libutils/common.h"
#include "libutils/buf.h"

enum {
    PB_WIRE_TYPE_VARINT   = 0, // Varint int32, int64, uint32, uint64, sint32, sint64, bool, enum
    PB_WIRE_TYPE_FIXED64  = 1, // 64-bit fixed64, sfixed64, double
    PB_WIRE_TYPE_LENDELIM = 2, // Length-delimited string, bytes, embedded messages, packed repeated fields
    PB_WIRE_TYPE_FIXED32  = 5, // 32-bit fixed32, sfixed32, float
};

static inline size_t buf_pb_size_varint(uint64_t value)
{
    size_t len = 1;

    while (value > 127) {
        value >>= 7;
        len++;
    }

    return len;
}

static int buf_pb_enc_varint(buf_t *buf, uint64_t value)
{
    if (buf_avail(buf) < 10) {
        if (buf_resize(buf, 10) != 0)
            return ENOMEM;
    }

    while (value > 127) {
        buf->ptr[buf->pos++] =  ((uint8_t)(value & 127)) | 128;
        value >>= 7;
    }

    buf->ptr[buf->pos++] = ((uint8_t)value) & 127;
    return 0;
}

static inline size_t buf_pb_size_type(int field, int type)
{
    return buf_pb_size_varint((field << 3) | type);
}

static inline int buf_pb_enc_type(buf_t *buf, int field, int type)
{
    return buf_pb_enc_varint(buf, (field << 3) | type);
}

static inline size_t buf_pb_size_str(int field, char *str)
{
    size_t len = buf_pb_size_type(field, PB_WIRE_TYPE_LENDELIM);
    size_t str_len = strlen(str);
    len += buf_pb_size_varint(str_len);
    return str_len + len;
}

static inline int buf_pb_enc_str(buf_t *buf, int field, char *str)
{
    int status = buf_pb_enc_type(buf, field, PB_WIRE_TYPE_LENDELIM);

    size_t len = strlen(str);
    status = status | buf_pb_enc_varint(buf, len);
    if (status !=0)
        return status;

    if (buf_avail(buf) < len) {
        if (buf_resize(buf, len) != 0)
            return ENOMEM;
    }

    memcpy(buf->ptr + buf->pos, str, len);
    buf->pos += len;
    return 0;
}

static inline size_t buf_pb_size_str_str(int field, char *str1, char *str2)
{
    if (str1 == NULL)
        return 0;
    size_t len = buf_pb_size_type(field, PB_WIRE_TYPE_LENDELIM);
    size_t str_len = strlen(str1) + (str2 == NULL ? 0 : strlen(str2));
    len += buf_pb_size_varint(str_len);
    return str_len + len;
}

static inline int buf_pb_enc_str_str(buf_t *buf, int field, char *str1, char *str2)
{
    if (str1 == NULL)
        return 0;

    int status = buf_pb_enc_type(buf, field, PB_WIRE_TYPE_LENDELIM);

    size_t len1 = strlen(str1);
    size_t len2 = str2 == NULL ? 0 : strlen(str2);
    status = status | buf_pb_enc_varint(buf, len1 + len2);
    if (status !=0)
        return status;

    if (buf_avail(buf) < len1 + len2) {
        if (buf_resize(buf, len1 + len2) != 0)
            return ENOMEM;
    }

    memcpy(buf->ptr + buf->pos, str1, len1);
    buf->pos += len1;
    if (str2 != NULL) {
        memcpy(buf->ptr + buf->pos, str2, len2);
        buf->pos += len2;
    }
    return 0;
}

static inline size_t buf_pb_size_int64(int field, int64_t value)
{
    return buf_pb_size_type(field, PB_WIRE_TYPE_VARINT) +
           buf_pb_size_varint(value);
//         buf_pb_size_varint((value << 1) ^ (value >> 63)); //FIXME
}

static inline int buf_pb_enc_int64(buf_t *buf, int field, int64_t value)
{
    int status = buf_pb_enc_type(buf, field, PB_WIRE_TYPE_VARINT);
//    (value << 1) ^ (value >> 63)
//    return status | buf_pb_enc_varint(buf, (value << 1) ^ (value >> 63)); // FIXME
    return status | buf_pb_enc_varint(buf, value);
}

static inline size_t buf_pb_size_uint64(int field, uint64_t value)
{
    return buf_pb_size_type(field, PB_WIRE_TYPE_VARINT) + buf_pb_size_varint(value);
}

static inline int buf_pb_enc_uint64(buf_t *buf, int field, uint64_t value)
{
    int status = buf_pb_enc_type(buf, field, PB_WIRE_TYPE_VARINT);
    return status | buf_pb_enc_varint(buf, value);
}

static inline size_t buf_pb_size_int32(int field, int32_t value)
{
    return buf_pb_size_type(field, PB_WIRE_TYPE_VARINT) +
           buf_pb_size_varint((value << 1) ^ (value >> 31));
}

static inline int buf_pb_enc_int32(buf_t *buf, int field, int32_t value)
{
    int status = buf_pb_enc_type(buf, field, PB_WIRE_TYPE_VARINT);
    return status | buf_pb_enc_varint(buf, (value << 1) ^ (value >> 31));
}

static inline size_t buf_pb_size_uint32(int field, uint32_t value)
{
    return buf_pb_size_type(field, PB_WIRE_TYPE_VARINT) + buf_pb_size_varint(value);
}

static inline int buf_pb_enc_uint32(buf_t *buf, int field, uint32_t value)
{
    int status = buf_pb_enc_type(buf, field, PB_WIRE_TYPE_VARINT);
    return status | buf_pb_enc_varint(buf, value);
}

static inline size_t buf_pb_size_double(int field, __attribute__((unused)) double value)
{
    return buf_pb_size_type(field, PB_WIRE_TYPE_FIXED64)+8;
}

static inline int buf_pb_enc_double(buf_t *buf, int field, __attribute__((unused)) double value)
{
    int status = buf_pb_enc_type(buf, field, PB_WIRE_TYPE_FIXED64);
    if (status != 0)
        return status;

    if (buf_avail(buf) < 8) {
        if (buf_resize(buf, 8) != 0)
            return ENOMEM;
    }

    double hvalue = htond(value);
    memcpy(buf->ptr + buf->pos, &hvalue, 8); // FIXME sizeof(double) == 8
    buf->pos += 8;
    return 0;
}

static inline size_t buf_pb_size_bool(int field, bool value)
{
    return buf_pb_size_type(field, PB_WIRE_TYPE_VARINT) +
           buf_pb_size_varint(value ? 1 : 0);
}

static inline int buf_pb_enc_bool(buf_t *buf, int field, bool value)
{
    int status = buf_pb_enc_type(buf, field, PB_WIRE_TYPE_VARINT);
    return status | buf_pb_enc_varint(buf, value ? 1 : 0); // FIXME
}

static inline int buf_pb_enc_message(buf_t *buf, __attribute__((unused)) int field, buf_t *msg)
{
    if (buf_avail(buf) < (msg->pos + 10)) {
        if (buf_resize(buf, (msg->pos + 10)) != 0)
            return ENOMEM;
    }

    int status = buf_pb_enc_varint(buf, msg->pos);
    if (status != 0)
        return status;

    memcpy(buf->ptr + buf->pos, msg->ptr, msg->pos);
    buf->pos += buf_len(msg);
    return 0;
}
