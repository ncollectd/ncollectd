/* SPDX-License-Identifier: GPL-2.0-only OR MIT                      */
/* SPDX-FileCopyrightText: Copyright (C) 2017 Florian octo Forster   */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Florian octo Forster <octo at collectd.org> */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include <stdbool.h>
#include <arpa/inet.h>

#include <libutils/common.h>
#include <libutils/strbuf.h>

typedef struct {
    uint8_t *ptr;
    size_t pos;
    size_t size;
    bool fixed;
} buf_t;

/* BUF_CREATE allocates a new buf_t on the stack, which must be freed
 * using BUF_DESTROY before returning from the function. Failure to call
 * BUF_DESTROY will leak the memory allocated to (buf_t).ptr. */
#define BUF_CREATE  (buf_t) { .ptr = NULL }

/* BUF_CREATE_FIXED allocates a new buf_t on the stack, using the buffer
 * "b" of fixed size "sz". The buffer is freed automatically when it goes out
 * of scope. */
#define BUF_CREATE_FIXED(b, sz)  (buf_t) { .ptr = b, .size = sz, .fixed = 1 }

/* STRBUF_CREATE_STATIC allocates a new buf_t on the stack, using the static
 * buffer "b". This macro assumes that is can use `sizeof(b)` to determine the
 * size of "b". If that is not the case, use STRBUF_CREATE_FIXED instead. */
#define BUF_CREATE_STATIC(b)  (buf_t) { .ptr = b, .size = sizeof(b), .fixed = 1 }

static inline void strbuf2buf (buf_t *dst, strbuf_t *src)
{
    dst->ptr = (uint8_t *)src->ptr;
    dst->pos = src->pos;
    dst->size = src->size;
    dst->fixed = src->fixed;
}

static inline void buf2strbuf (strbuf_t *dst, buf_t *src)
{
    dst->ptr = (char *)src->ptr;
    dst->pos = src->pos;
    dst->size = src->size;
    dst->fixed = src->fixed;
}

int buf_resize(buf_t *buf, size_t need);

static inline size_t buf_len(buf_t const *buf)
{
    return buf->pos;
}

static inline size_t buf_avail(buf_t const *buf)
{
    return buf->size ? buf->size - buf->pos : 0;
}

static inline int buf_append(buf_t *buf, buf_t *src)
{
    if (buf_avail(buf) < src->pos) {
        if (buf_resize(buf, src->pos) != 0)
            return ENOMEM;
    }

    memcpy(buf->ptr + buf->pos, src->ptr, src->pos);
    buf->pos += src->pos;
    return 0;
}

static inline int buf_put(buf_t *buf, const void *s, size_t len)
{
    if (buf_avail(buf) < len) {
        if (buf_resize(buf, len) != 0)
            return ENOMEM;
    }

    memcpy(buf->ptr + buf->pos, s, len);
    buf->pos += len;
    return 0;
}

static inline int buf_putstr(buf_t *buf, char *s)
{
    return buf_put(buf, s, strlen(s));
}

static inline int buf_putchar(buf_t *buf, char c)
{
    if (unlikely(buf_avail(buf) < 1)) {
        if (buf_resize(buf, 1) != 0)
            return ENOMEM;
    }

    buf->ptr[buf->pos++] = (uint8_t)c;
    return 0;
}

static inline int buf_putuint8(buf_t *buf, uint8_t c)
{
    if (unlikely(buf_avail(buf) < 1)) {
        if (buf_resize(buf, 1) != 0)
            return ENOMEM;
    }

    buf->ptr[buf->pos++] = c;
    return 0;
}

static inline int buf_putuint16(buf_t *buf, uint16_t n)
{
    if (unlikely(buf_avail(buf) < sizeof(n))) {
        if (buf_resize(buf, sizeof(n)) != 0)
            return ENOMEM;
    }

    memcpy(buf->ptr + buf->pos, &n, sizeof(n));
    buf->pos += sizeof(n);
    return 0;
}

static inline int buf_putuint16hton(buf_t *buf, uint16_t n)
{
    return buf_putuint16(buf, htons(n));
}

static inline int buf_putint16(buf_t *buf, int16_t n)
{
    if (unlikely(buf_avail(buf) < sizeof(n))) {
        if (buf_resize(buf, sizeof(n)) != 0)
            return ENOMEM;
    }

    memcpy(buf->ptr + buf->pos, &n, sizeof(n));
    buf->pos += sizeof(n);
    return 0;
}

static inline int buf_putint16hton(buf_t *buf, int16_t n)
{
    return buf_putuint16(buf, htons((uint16_t)n));
}

static inline int buf_putuint32(buf_t *buf, uint32_t n)
{
    if (unlikely(buf_avail(buf) < sizeof(n))) {
        if (buf_resize(buf, sizeof(n)) != 0)
            return ENOMEM;
    }

    memcpy(buf->ptr + buf->pos, &n, sizeof(n));
    buf->pos += sizeof(n);
    return 0;
}

static inline int buf_putuint32hton(buf_t *buf, uint32_t n)
{
    return buf_putuint32(buf, htonl(n));
}

static inline int buf_putint32(buf_t *buf, int32_t n)
{
    if (unlikely(buf_avail(buf) < sizeof(n))) {
        if (buf_resize(buf, sizeof(n)) != 0)
            return ENOMEM;
    }

    memcpy(buf->ptr + buf->pos, &n, sizeof(n));
    buf->pos += sizeof(n);
    return 0;
}

static inline int buf_putint32hton(buf_t *buf, int32_t n)
{
    return buf_putuint32(buf, htonl((uint32_t)n));
}

static inline int buf_putuint64(buf_t *buf, uint64_t n)
{
    if (unlikely(buf_avail(buf) < sizeof(n))) {
        if (buf_resize(buf, sizeof(n)) != 0)
            return ENOMEM;
    }

    memcpy(buf->ptr + buf->pos, &n, sizeof(n));
    buf->pos += sizeof(n);
    return 0;
}

static inline int buf_putuint64hton(buf_t *buf, uint64_t n)
{
    return buf_putuint64(buf, htonll(n));
}

static inline int buf_putint64(buf_t *buf, int64_t n)
{
    if (unlikely(buf_avail(buf) < sizeof(n))) {
        if (buf_resize(buf, sizeof(n)) != 0)
            return ENOMEM;
    }

    memcpy(buf->ptr + buf->pos, &n, sizeof(n));
    buf->pos += sizeof(n);
    return 0;
}

static inline int buf_putint64hton(buf_t *buf, int64_t n)
{
    return buf_putuint64(buf, htonll((uint64_t)n));
}

static inline int buf_putdouble(buf_t *buf, double n)
{
    if (unlikely(buf_avail(buf) < sizeof(n))) {
        if (buf_resize(buf, sizeof(n)) != 0)
            return ENOMEM;
    }

    memcpy(buf->ptr + buf->pos, &n, sizeof(n));
    buf->pos += sizeof(n);
    return 0;
}

static inline int buf_putdoublehton(buf_t *buf, double n)
{
    return buf_putdouble(buf, htond(n));
}

int buf_putitoa(buf_t *buf, int64_t n);
int buf_putdtoa(buf_t *buf, double n);

static inline void buf_reset(buf_t *buf)
{
    buf->pos = 0;
}

static inline void buf_resetto(buf_t *buf, size_t pos)
{
    if (pos > buf->pos)
        return;

    if (buf->ptr != NULL)
        buf->pos = pos;
}

void buf_reset2page(buf_t *buf);

static inline void buf_destroy(buf_t *buf)
{
    if (buf->fixed)
        return;

    if (buf->ptr != NULL) {
        free(buf->ptr);
        buf->ptr = NULL;
    }
}
