/* SPDX-License-Identifier: GPL-2.0-only OR MIT                      */
/* SPDX-FileCopyrightText: Copyright (C) 2017 Florian octo Forster   */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Florian octo Forster <octo at collectd.org> */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "ncollectd.h"

#include <stdbool.h>

typedef struct {
    char *ptr;
    size_t pos;
    size_t size;
    bool fixed;
} strbuf_t;

/* STRBUF_CREATE allocates a new strbuf_t on the stack, which must be freed
 * using STRBUF_DESTROY before returning from the function. Failure to call
 * STRBUF_DESTROY will leak the memory allocated to (strbuf_t).ptr. */
#define STRBUF_CREATE  (strbuf_t) { .ptr = NULL }

/* STRBUF_CREATE_FIXED allocates a new strbuf_t on the stack, using the buffer
 * "b" of fixed size "sz". The buffer is freed automatically when it goes out
 * of scope. */
#define STRBUF_CREATE_FIXED(b, sz)  (strbuf_t) { .ptr = b, .size = sz, .fixed = 1 }

/* STRBUF_CREATE_STATIC allocates a new strbuf_t on the stack, using the static
 * buffer "b". This macro assumes that is can use `sizeof(b)` to determine the
 * size of "b". If that is not the case, use STRBUF_CREATE_FIXED instead. */
#define STRBUF_CREATE_STATIC(b)  (strbuf_t) { .ptr = b, .size = sizeof(b), .fixed = 1 }

/* strbuf_print adds "s" to the buffer. If the size of the buffer is static and
 * there is no space available in the buffer, ENOSPC is returned. */
int strbuf_print(strbuf_t *buf, char const *s);

/* strbuf_printf adds a string to the buffer using formatting. If the size of
 * the buffer is static and there is no space available in the buffer, ENOSPC
 * is returned. */
int strbuf_printf(strbuf_t *buf, char const *format, ...);

/* strbuf_printn adds at most n bytes from "s" to the buffer.  If the size of
 * the buffer is static and there is no space available in the buffer, ENOSPC
 * is returned. */
int strbuf_printn(strbuf_t *buf, char const *s, size_t n);

/* strbuf_print_escaped adds an escaped copy of "s" to the buffer. Each
 * character in "need_escape" is prefixed by "escape_char". If "escape_char" is
 * '\' (backslash), newline (\n), cartridge return (\r) and tab (\t) are
 * handled correctly. */
int strbuf_print_escaped(strbuf_t *buf, char const *s, char const *need_escape, char escape_char);

int strbuf_resize(strbuf_t *buf, size_t need);

static inline size_t strbuf_len(strbuf_t const *buf)
{
    return buf->pos;
}

static inline size_t strbuf_offset(strbuf_t const *buf)
{
    return buf->pos ? buf->pos - 1 : 0;
}

static inline size_t strbuf_avail(strbuf_t const *buf)
{
    return buf->size ? buf->size - buf->pos - 1 : 0;
}

static inline int strbuf_putchar(strbuf_t *buf, char c)
{
    if (unlikely(strbuf_avail(buf) < 1)) {
        if (strbuf_resize(buf, 1) != 0)
            return ENOMEM;
    }

    if (unlikely(buf->ptr == NULL))
        return ENOMEM;

    buf->ptr[buf->pos++] = c;
    buf->ptr[buf->pos] = '\0';
    return 0;
}

static inline int strbuf_putxchar(strbuf_t *buf, char c, size_t n)
{
    if (unlikely(strbuf_avail(buf) < n)) {
        if (strbuf_resize(buf, n) != 0)
            return ENOMEM;
    }

    if (unlikely(buf->ptr == NULL))
        return ENOMEM;

    memset(buf->ptr + buf->pos, c, n);
    buf->pos += n;
    buf->ptr[buf->pos] = '\0';
    return 0;
}

static inline int strbuf_putstrn(strbuf_t *buf, char const *s, size_t len)
{
    if (unlikely(strbuf_avail(buf) < len)) {
        if (strbuf_resize(buf, len) != 0)
            return ENOMEM;
    }

    if (unlikely(buf->ptr == NULL))
        return ENOMEM;

    memcpy(buf->ptr + buf->pos, s, len);
    buf->pos += len;
    buf->ptr[buf->pos] = '\0';
    return 0;
}

static inline int strbuf_putxstrn(strbuf_t *buf, char const *s, size_t len, size_t n)
{
    size_t total = len * n;
    if (unlikely(strbuf_avail(buf) < total)) {
        if (strbuf_resize(buf, total) != 0)
            return ENOMEM;
    }

    if (unlikely(buf->ptr == NULL))
        return ENOMEM;

    for (size_t i=0; i < n; i++) {
        memcpy(buf->ptr + buf->pos, s, len);
        buf->pos += len;
    }
    buf->ptr[buf->pos] = '\0';
    return 0;
}

static inline int strbuf_putstr(strbuf_t *buf, char const *s)
{
    return strbuf_putstrn(buf, s, strlen(s));
}

int strbuf_putstrntoupper(strbuf_t *buf, char const *s, size_t len);

static inline int strbuf_putstrtoupper(strbuf_t *buf, char const *s)
{
    return strbuf_putstrntoupper(buf, s, strlen(s));
}

static inline int strbuf_putstrv(strbuf_t *buf, const struct iovec *iov, int iovcnt)
{
    size_t len = 0;
    for (int i=0; i < iovcnt; i++)
        len += iov[i].iov_len;

    if (unlikely(strbuf_avail(buf) < len)) {
        if (strbuf_resize(buf, len) != 0)
            return ENOMEM;
    }

    if (unlikely(buf->ptr == NULL))
        return ENOMEM;

    for (int i=0; i < iovcnt; i++) {
        memcpy(buf->ptr + buf->pos, (char *)iov[i].iov_base, iov[i].iov_len);
        buf->pos += iov[i].iov_len;
    }

    buf->ptr[buf->pos] = '\0';
    return 0;
}

int strbuf_putint(strbuf_t *buf, int64_t n);

int strbuf_putuint(strbuf_t *buf, uint64_t n);

int strbuf_putdouble(strbuf_t *buf, double n);

static inline void strbuf_reset(strbuf_t *buf)
{
    if (buf == NULL)
        return;

    buf->pos = 0;
    if (buf->ptr != NULL)
        buf->ptr[0] = '\0';
}

static inline void strbuf_resetto(strbuf_t *buf, size_t pos)
{
    if (buf == NULL)
        return;

    if (pos > buf->pos)
        return;

    if (buf->ptr != NULL) {
        buf->pos = pos;
        buf->pos++;
        buf->ptr[buf->pos] = '\0';
    }
}

void strbuf_reset2page(strbuf_t *buf);

static inline void strbuf_destroy(strbuf_t *buf)
{
    if (buf == NULL)
        return;

    if (buf->fixed)
        return;

    if (buf->ptr != NULL) {
        free(buf->ptr);
        buf->ptr = NULL;
        buf->pos = 0;
        buf->size = 0;
    }
}

int strbuf_putnurlencode(strbuf_t *buf, char const *str, size_t len);

static inline int strbuf_puturlencode(strbuf_t *buf, char const *str)
{
    return strbuf_putnurlencode(buf, str, strlen(str));
}

int strbuf_putnescape_json(strbuf_t *buf, char const *str, size_t len);

static inline int strbuf_putescape_json(strbuf_t *buf, char const *str)
{
    return strbuf_putnescape_json(buf, str, strlen(str));
}

int strbuf_putnescape_label(strbuf_t *buf, char const *str, size_t len);

static inline int strbuf_putescape_label(strbuf_t *buf, char const *str)
{
    return strbuf_putnescape_label(buf, str, strlen(str));
}
