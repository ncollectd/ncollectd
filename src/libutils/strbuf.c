// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2017 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "libutils/strbuf.h"
#include "libutils/itoa.h"
#include "libutils/dtoa.h"

#include <stdarg.h>

static size_t strbuf_pagesize(void)
{
    static size_t cached_pagesize;

    if (!cached_pagesize) {
        long tmp = sysconf(_SC_PAGESIZE);
        if (tmp >= 1)
            cached_pagesize = (size_t)tmp;
        else
            cached_pagesize = 1024;
    }

    return cached_pagesize;
}

/* strbuf_resize resizes an dynamic buffer to ensure that "need" bytes can be
 * stored in it. When called with an empty buffer, i.e. buf->size == 0, it will
 * allocate just enough memory to store need+1 bytes. Subsequent calls will
 * only allocate memory when needed, doubling the allocated memory size each
 * time until the page size is reached, then allocating. */
int strbuf_resize(strbuf_t *buf, size_t need)
{
     if (buf->fixed)
         return ENOMEM;

    if (strbuf_avail(buf) > need)
        return 0;

    size_t new_size;
    if (buf->size == 0) {
        /* New buffers: try to use a reasonable default. */
        new_size = 512;
    } else if (buf->size < strbuf_pagesize()) {
        /* Small buffers: double the size. */
        new_size = 2 * buf->size;
    } else {
        /* Large buffers: allocate an additional page. */
        size_t pages = (buf->size + strbuf_pagesize() - 1) / strbuf_pagesize();
        new_size = (pages + 1) * strbuf_pagesize();
    }

    /* Check that the new size is large enough. If not, calculate the exact number
     * of bytes needed. */
    if (new_size < (buf->pos + need + 1))
        new_size = buf->pos + need + 1;

    char *new_ptr = realloc(buf->ptr, new_size);
    if (new_ptr == NULL)
        return ENOMEM;

    buf->ptr = new_ptr;
    buf->size = new_size;

    return 0;
}

void strbuf_reset2page(strbuf_t *buf)
{
    if (buf == NULL)
        return;

    if (buf->fixed)
        return;

    buf->pos = 0;
    if (buf->ptr != NULL)
        buf->ptr[buf->pos] = 0;

    /* Truncate the buffer to the page size. This is deemed a good compromise
     * between freeing memory (after a large buffer has been constructed) and
     * performance (avoid unnecessary allocations). */
    size_t new_size = strbuf_pagesize();
    if (buf->size > new_size) {
        char *new_ptr = realloc(buf->ptr, new_size);
        if (new_ptr != NULL) {
            buf->ptr = new_ptr;
            buf->size = new_size;
        }
    }
}

int strbuf_print(strbuf_t *buf, char const *s)
{
    if ((buf == NULL) || (s == NULL))
        return EINVAL;

    size_t len = strlen(s);
    if (unlikely(strbuf_avail(buf) < len)) {
        if (strbuf_resize(buf, len) != 0)
            return ENOMEM;
    }

    if (unlikely(buf->ptr == NULL))
        return ENOMEM;

    memcpy(buf->ptr + buf->pos, s, len);
    buf->pos += len;
    buf->ptr[buf->pos] = 0;

    return 0;
}

int strbuf_printf(strbuf_t *buf, char const *format, ...)
{
    va_list ap;

    va_start(ap, format);
    int status = vsnprintf(NULL, 0, format, ap);
    va_end(ap);
    if (status <= 0)
        return status;

    size_t s_len = (size_t)status;
    if (unlikely(strbuf_avail(buf) < s_len)) {
        if (strbuf_resize(buf, s_len) != 0)
            return ENOMEM;
    }

    if (unlikely(buf->ptr == NULL))
        return ENOMEM;

    size_t bytes = strbuf_avail(buf);
    if (bytes == 0)
        return ENOSPC;

    if (bytes > s_len)
        bytes = s_len;

    va_start(ap, format);
    (void)vsnprintf(buf->ptr + buf->pos, bytes + 1, format, ap);
    va_end(ap);

    buf->pos += bytes;
    buf->ptr[buf->pos] = 0;

    return 0;
}

#ifndef HAVE_STRNLEN
static size_t strnlen(const char *s, size_t maxlen)
{
    for (size_t i = 0; i < maxlen; i++) {
        if (s[i] == 0) {
            return i;
        }
    }
    return maxlen;
}
#endif

int strbuf_printn(strbuf_t *buf, char const *s, size_t n)
{
    if ((buf == NULL) || (s == NULL))
        return EINVAL;

    if (n == 0)
        return 0;

    if (unlikely(strbuf_avail(buf) < n)) {
        if (strbuf_resize(buf, n) != 0)
            return ENOMEM;
    }

    if (unlikely(buf->ptr == NULL))
        return ENOMEM;

    memcpy(buf->ptr + buf->pos, s, n);
    buf->pos += n;
    buf->ptr[buf->pos] = 0;
    return 0;
}

int strbuf_print_escaped(strbuf_t *buf, char const *s, char const *need_escape, char escape_char)
{
    if ((buf == NULL) || (s == NULL) || (need_escape == NULL) || (escape_char == 0)) {
        return EINVAL;
    }

    size_t s_len = strlen(s);
    while (s_len > 0) {
        size_t valid_len = strcspn(s, need_escape);
        if (valid_len == s_len) {
            return strbuf_print(buf, s);
        }
        if (valid_len != 0) {
            int status = strbuf_printn(buf, s, valid_len);
            if (status != 0) {
                return status;
            }

            s += valid_len;
            s_len -= valid_len;
            continue;
        }

        char c = s[0];
        if (escape_char == '\\') {
            if (c == '\n') {
                c = 'n';
            } else if (c == '\r') {
                c = 'r';
            } else if (c == '\t') {
                c = 't';
            }
        }

        char tmp[3] = {escape_char, c, 0};
        int status = strbuf_print(buf, tmp);
        if (status != 0) {
            return status;
        }

        s++;
        s_len--;
    }

    return 0;
}

int strbuf_putint(strbuf_t *buf, int64_t value)
{
    if (strbuf_avail(buf) < ITOA_MAX+1) {
        if (strbuf_resize(buf, ITOA_MAX+1) != 0)
            return ENOMEM;
    }

    if (unlikely(buf->ptr == NULL))
        return ENOMEM;

    uint64_t uvalue = value;
    if (value < 0) {
        uvalue = -value;
        buf->ptr[buf->pos++] = '-';
    }

    size_t len = itoa(uvalue, (char *)(buf->ptr + buf->pos));
    buf->pos += len;
    buf->ptr[buf->pos] = '\0';
    return 0;
}

int strbuf_putuint(strbuf_t *buf, uint64_t value)
{
    if (strbuf_avail(buf) < ITOA_MAX) {
        if (strbuf_resize(buf, ITOA_MAX) != 0)
            return ENOMEM;
    }

    if (unlikely(buf->ptr == NULL))
        return ENOMEM;

    size_t len = uitoa(value, (char *)(buf->ptr + buf->pos));
    buf->pos += len;
    buf->ptr[buf->pos] = '\0';
    return 0;
}

int strbuf_putdouble(strbuf_t *buf, double value)
{
    if (strbuf_avail(buf) < DTOA_MAX) {
        if (strbuf_resize(buf, DTOA_MAX) != 0)
            return ENOMEM;
    }

    if (unlikely(buf->ptr == NULL))
        return ENOMEM;

    size_t len = dtoa(value, (char *)(buf->ptr + buf->pos), strbuf_avail(buf));
    buf->pos += len;
    buf->ptr[buf->pos] = '\0';
    return 0;
}

int strbuf_putstrntoupper(strbuf_t *buf, char const *s, size_t len)
{
    if (unlikely(strbuf_avail(buf) < len)) {
        if (strbuf_resize(buf, len) != 0)
            return ENOMEM;
    }

    if (unlikely(buf->ptr == NULL))
        return ENOMEM;

    for (size_t i = 0; i < len; i++) {
        buf->ptr[buf->pos+i] = toupper((int)s[i]);
    }

    buf->pos += len;
    buf->ptr[buf->pos] = '\0';
    return 0;
}

int strbuf_putnescape_json(strbuf_t *buf, char const *str, size_t len)
{
    static const char *hex = "0123456789abcdef";

    if (unlikely(strbuf_avail(buf) < len)) {
        if (strbuf_resize(buf, len) != 0)
            return ENOMEM;
    }

    if (unlikely(buf->ptr == NULL))
        return ENOMEM;

    size_t n = 0;
    while (n < len) {
        if (unlikely(strbuf_avail(buf) < 6)) {
            if (strbuf_resize(buf, 6) != 0)
                return ENOMEM;
        }

        unsigned char c = (unsigned char)str[n];
        switch(c) {
        case '"':
            buf->ptr[buf->pos++] = '\\';
            buf->ptr[buf->pos++] = '"';
            break;
        case '\\':
            buf->ptr[buf->pos++] = '\\';
            buf->ptr[buf->pos++] = '\\';
            break;
        case '\b':
            buf->ptr[buf->pos++] = '\\';
            buf->ptr[buf->pos++] = 'b';
            break;
        case '\f':
            buf->ptr[buf->pos++] = '\\';
            buf->ptr[buf->pos++] = 'f';
            break;
        case '\n':
            buf->ptr[buf->pos++] = '\\';
            buf->ptr[buf->pos++] = 'n';
            break;
        case '\r':
            buf->ptr[buf->pos++] = '\\';
            buf->ptr[buf->pos++] = 'r';
            break;
        case '\t':
            buf->ptr[buf->pos++] = '\\';
            buf->ptr[buf->pos++] = 't';
            break;
        default:
            if (c < 32) {
                buf->ptr[buf->pos++] = '\\';
                buf->ptr[buf->pos++] = 'u';
                buf->ptr[buf->pos++] = '0';
                buf->ptr[buf->pos++] = '0';
                buf->ptr[buf->pos++] = hex[c >> 4];
                buf->ptr[buf->pos++] = hex[c & 0xf];
            } else {
                buf->ptr[buf->pos++] = (char)c;
            }
            break;
        }
        n++;
    }

    buf->ptr[buf->pos] = '\0';
    return 0;
}

int strbuf_putnurlencode(strbuf_t *buf, char const *str, size_t len)
{
    static const char *hex = "0123456789abcdef";
    static const char esc[256] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
    };

    if (unlikely(strbuf_avail(buf) < len)) {
        if (strbuf_resize(buf, len) != 0)
            return ENOMEM;
    }

    if (unlikely(buf->ptr == NULL))
        return ENOMEM;

    size_t n = 0;
    while (n < len) {
        if (unlikely(strbuf_avail(buf) < 3)) {
            if (strbuf_resize(buf, 3) != 0)
                return ENOMEM;
        }

        unsigned char c = (unsigned char)str[n];

        if (esc[c]) {
            buf->ptr[buf->pos++] = '%';
            buf->ptr[buf->pos++] = hex[c >> 4];
            buf->ptr[buf->pos++] = hex[c & 0xf];
        } else {
            buf->ptr[buf->pos++] = c;
        }
        n++;
    }

    buf->ptr[buf->pos] = '\0';
    return 0;
}

int strbuf_putnescape_label(strbuf_t *buf, char const *str, size_t len)
{
    if (unlikely(strbuf_avail(buf) < len)) {
        if (strbuf_resize(buf, len) != 0)
            return ENOMEM;
    }

    if (unlikely(buf->ptr == NULL))
        return ENOMEM;

    size_t n = 0;
    while (n < len) {
        if (unlikely(strbuf_avail(buf) < 2)) {
            if (strbuf_resize(buf, 2) != 0)
                return ENOMEM;
        }

        unsigned char c = (unsigned char)str[n];
        switch(c) {
        case '"':
            buf->ptr[buf->pos++] = '\\';
            buf->ptr[buf->pos++] = '"';
            break;
        case '\\':
            buf->ptr[buf->pos++] = '\\';
            buf->ptr[buf->pos++] = '\\';
            break;
        case '\n':
            buf->ptr[buf->pos++] = '\\';
            buf->ptr[buf->pos++] = 'n';
            break;
        case '\r':
            buf->ptr[buf->pos++] = '\\';
            buf->ptr[buf->pos++] = 'r';
            break;
        case '\t':
            buf->ptr[buf->pos++] = '\\';
            buf->ptr[buf->pos++] = 't';
            break;
        default:
            buf->ptr[buf->pos++] = (char)c;
            break;
        }
        n++;
    }

    buf->ptr[buf->pos] = '\0';
    return 0;
}

int strbuf_putnescape_squote(strbuf_t *buf, char const *str, size_t len)
{
    if (unlikely(strbuf_avail(buf) < len)) {
        if (strbuf_resize(buf, len) != 0)
            return ENOMEM;
    }

    if (unlikely(buf->ptr == NULL))
        return ENOMEM;

    size_t n = 0;
    while (n < len) {
        if (unlikely(strbuf_avail(buf) < 2)) {
            if (strbuf_resize(buf, 2) != 0)
                return ENOMEM;
        }

        unsigned char c = (unsigned char)str[n];
        switch(c) {
        case '\'':
            buf->ptr[buf->pos++] = '\\';
            buf->ptr[buf->pos++] = '\'';
            break;
        case '\\':
            buf->ptr[buf->pos++] = '\\';
            buf->ptr[buf->pos++] = '\\';
            break;
        case '\n':
            buf->ptr[buf->pos++] = '\\';
            buf->ptr[buf->pos++] = 'n';
            break;
        case '\r':
            buf->ptr[buf->pos++] = '\\';
            buf->ptr[buf->pos++] = 'r';
            break;
        case '\t':
            buf->ptr[buf->pos++] = '\\';
            buf->ptr[buf->pos++] = 't';
            break;
        default:
            buf->ptr[buf->pos++] = (char)c;
            break;
        }

        n++;
    }

    buf->ptr[buf->pos] = '\0';
    return 0;
}

int strbuf_putnreplace_set(strbuf_t *buf, char const *str, size_t len, char rset[256], char rchar)
{
    if (unlikely(strbuf_avail(buf) < len)) {
        if (strbuf_resize(buf, len) != 0)
            return ENOMEM;
    }

    if (unlikely(buf->ptr == NULL))
        return ENOMEM;

    size_t n = 0;
    while (n < len) {
        if (unlikely(strbuf_avail(buf) < 2)) {
            if (strbuf_resize(buf, 2) != 0)
                return ENOMEM;
        }

        unsigned char c = (unsigned char)str[n];

        if (rset[c]) {
            buf->ptr[buf->pos++] = rchar;
        } else {
            buf->ptr[buf->pos++] = c;
        }
        n++;
    }

    buf->ptr[buf->pos] = '\0';
    return 0;
}
