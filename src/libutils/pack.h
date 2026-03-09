#pragma once

#include "libutils/buf.h"

static inline int pack_size(buf_t *buf, uint8_t id, size_t len)
{
    int status = 0;

    if (len < UINT8_MAX) {
        status |= buf_putuint8(buf, id | 0x01);
        status |= buf_putuint8(buf, (uint8_t)len);
    } else if (len < UINT16_MAX) {
        status |= buf_putuint8(buf, id | 0x02);
        status |= buf_putuint16(buf, (uint16_t)len);
    } else if (len < UINT32_MAX) {
        status |= buf_putuint8(buf, id | 0x04);
        status |= buf_putuint32(buf, (uint32_t)len);
    } else {
        status |= buf_putuint8(buf, id | 0x08);
        status |= buf_putuint64(buf, (uint64_t)len);
    }

    return status;
}

static inline int pack_id(buf_t *buf, uint8_t id)
{
    return buf_putuint8(buf, id);
}

static inline int pack_uint8(buf_t *buf, uint8_t id, uint8_t v)
{
    int status = buf_putuint8(buf, id);
    return status | buf_putuint8(buf, v);
}

static inline int pack_uint64(buf_t *buf, uint8_t id, uint64_t v)
{
    int status = buf_putuint8(buf, id);
    return status | buf_putuint64(buf, v);
}

static inline int pack_int64(buf_t *buf, uint8_t id, int64_t v)
{
    int status = buf_putuint8(buf, id);
    return status | buf_putint64(buf, v);
}

static inline int pack_double(buf_t *buf, uint8_t id, double d)
{
    int status = buf_putuint8(buf, id);
    return status | buf_putdouble(buf, d);
}

static inline int pack_string(buf_t *buf, uint8_t id, const char *s)
{
    if (s == NULL)
        return 0;

    size_t len = strlen(s);
    int status = pack_size(buf, id, len + 1);
    return status | buf_put(buf, s, len + 1);
}

static inline int pack_block_begin(buf_t *buf, size_t *pos)
{
    *pos = buf_len(buf);
    return buf_putuint32(buf, 0xff000000U);
}

static inline int pack_block_end(buf_t *buf, size_t pos)
{
    size_t current = buf_len(buf);
    if ((pos + sizeof(uint32_t)) > current)
        return EINVAL;

    size_t diff = current - (pos + sizeof(uint32_t));
    if (diff > 0x00ffffffU)
        return EINVAL;

    return buf_putuint32at(buf, pos, (uint32_t)0xff000000U | (uint32_t)diff);
}

static inline int unpack_size(rbuf_t *rbuf, uint8_t id, uint64_t *size)
{
    int status = 0;

    switch(id & 0x0f) {
    case 0x01: {
        uint8_t value = 0;
        status = rbuf_getuint8(rbuf, &value);
        *size = value;
    }   break;
    case 0x02: {
        uint16_t value = 0;
        status = rbuf_getuint16(rbuf, &value);
        *size = value;
    }   break;
    case 0x04: {
        uint32_t value = 0;
        status = rbuf_getuint32(rbuf, &value);
        *size = value;
    }   break;
    case 0x08: {
        uint64_t value = 0;
        status = rbuf_getuint64(rbuf, &value);
        *size = value;
    }   break;
    default:
        return -1;
    }

    return status;
}

static inline int unpack_id(rbuf_t *rbuf, uint8_t *value)
{
    return rbuf_getuint8(rbuf, value);
}

static inline int unpack_uint8(rbuf_t *rbuf, uint8_t *value)
{
    return rbuf_getuint8(rbuf, value);
}

static inline int unpack_id_uint8(rbuf_t *rbuf, uint8_t id, uint8_t *value)
{
    uint8_t rid = 0;
    int status = rbuf_getuint8(rbuf, &rid);
    if (rid != id)
        return EINVAL;

    return status | rbuf_getuint8(rbuf, value);
}

static inline int unpack_uint64(rbuf_t *rbuf, uint64_t *value)
{
    return rbuf_getuint64(rbuf, value);
}

static inline int unpack_id_uint64(rbuf_t *rbuf, uint8_t id, uint64_t *value)
{
    uint8_t rid = 0;
    int status = rbuf_getuint8(rbuf, &rid);
    if (rid != id)
        return EINVAL;

    return status | rbuf_getuint64(rbuf, value);
}

static inline int unpack_int64(rbuf_t *rbuf, int64_t *value)
{
    return rbuf_getint64(rbuf, value);
}

static inline int unpack_id_int64(rbuf_t *rbuf, uint8_t id, int64_t *value)
{
    uint8_t rid = 0;
    int status = rbuf_getuint8(rbuf, &rid);
    if (rid != id)
        return EINVAL;

    return status | rbuf_getint64(rbuf, value);
}

static inline int unpack_double(rbuf_t *rbuf, double *value)
{
    return rbuf_getdouble(rbuf, value);
}

static inline int unpack_id_double(rbuf_t *rbuf, uint8_t id, double *value)
{
    uint8_t rid = 0;
    int status = rbuf_getuint8(rbuf, &rid);
    if (rid != id)
        return EINVAL;

    return status | rbuf_getdouble(rbuf, value);
}

static inline int unpack_string(rbuf_t *rbuf, uint8_t id, char **str)
{
    if (*str != NULL)
        free(*str);

    size_t len = 0;
    int status = unpack_size(rbuf, id, &len);
    if (status != 0)
        return status;

    if (len == 0) {
        *str = NULL;
        return 0;
    }

    char *rstr = NULL;
    status = rbuf_refstring(rbuf, &rstr, len);
    if (status != 0)
        return status;

    *str = strdup(rstr);
    if (*str == NULL)
        return ENOMEM;

    return 0;
}

static inline int unpack_refstring(rbuf_t *rbuf, uint8_t id, char **str)
{
    size_t len = 0;
    int status = unpack_size(rbuf, id, &len);
    if (status != 0)
        return status;

    if (len == 0) {
        *str = NULL;
        return 0;
    }

    char *rstr = NULL;
    status = rbuf_refstring(rbuf, &rstr, len);
    if (status != 0)
        return status;

    *str = rstr;

    return 0;
}

static inline int unpack_id_refstring(rbuf_t *rbuf, uint8_t id, char **str)
{
    uint8_t rid = 0;
    int status = unpack_id(rbuf, &rid);
    if (status != 0)
        return 1;

    if ((rid & 0xf0) != id)
        return 1;

    size_t len = 0;
    status = unpack_size(rbuf, rid, &len);
    if (status != 0)
        return status;

    if (len == 0) {
        *str = NULL;
        return 0;
    }

    char *rstr = NULL;
    status = rbuf_refstring(rbuf, &rstr, len);
    if (status != 0)
        return status;

    *str = rstr;

    return 0;
}

static inline int unpack_block(rbuf_t *rbuf, rbuf_t *srbuf)
{
    uint32_t value = 0;

    int status = rbuf_getuint32(rbuf, &value);
    if (status != 0)
        return status;

    if ((value & 0xff000000U) != 0xff000000U)
        return EINVAL;

    size_t size = value & 0x00ffffffU;
    if (size == 0)
        return EINVAL;

    return rbuf_sub(rbuf, size, srbuf);
}

static inline void unpack_avance_block(rbuf_t *rbuf, rbuf_t *srbuf)
{
    rbuf->pos += srbuf->size;
}
