/**
 * collectd - src/utils/strbuf/strbuf.h
 * Copyright (C) 2017       Florian octo Forster
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *   Florian octo Forster <octo at collectd.org>
 */

#ifndef UTILS_STRBUF_H
#define UTILS_STRBUF_H 1

#include <stdbool.h>

typedef struct {
  char *ptr;
  size_t pos;
  size_t size;
} strbuf_t;

/* STRBUF_CREATE allocates a new strbuf_t on the stack, which must be freed
 * using STRBUF_DESTROY before returning from the function. Failure to call
 * STRBUF_DESTROY will leak the memory allocated to (strbuf_t).ptr. */
#define STRBUF_CREATE  (strbuf_t) { .ptr = NULL }

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

static inline size_t strbuf_avail(strbuf_t const *buf)
{
  return buf->size ? buf->size - buf->pos - 1 : 0;
}

static inline int strbuf_putchar(strbuf_t *buf, int c)
{
  if (strbuf_avail(buf) < 1) {
    if (strbuf_resize(buf, 1) < 0)
      return ENOMEM;
  }

  buf->ptr[buf->pos++] = c;
  buf->ptr[buf->pos] = '\0';
  return 0;
}

static inline int strbuf_putstrn(strbuf_t *buf, char const *s, size_t len)
{
  if (strbuf_avail(buf) < len) {
    if (strbuf_resize(buf, len) < 0)
      return ENOMEM;
  }

  memcpy(buf->ptr + buf->pos, s, len);
  buf->pos += len;
  buf->ptr[buf->pos] = '\0';
  return 0;
}

static inline int strbuf_putstr(strbuf_t *buf, char const *s)
{
  return strbuf_putstrn(buf, s, strlen(s));
}

static inline int strbuf_putstrv(strbuf_t *buf, const struct iovec *iov, int iovcnt)
{
  size_t len = 0;
  for (int i=0; i < iovcnt; i++)
    len = iov[i].iov_len;

  if (strbuf_avail(buf) < len) {
    if (strbuf_resize(buf, len) < 0)
      return ENOMEM;
  }

  for (int i=0; i < iovcnt; i++) {
    memcpy(buf->ptr + buf->pos, (char *)iov[i].iov_base, iov[i].iov_len);
    buf->pos += iov[i].iov_len;
  }

  buf->ptr[buf->pos] = '\0';
  return 0;
}

int strbuf_putint(strbuf_t *buf, int64_t n);

int strbuf_putuint(strbuf_t *buf, uint64_t n);

static inline void strbuf_reset(strbuf_t *buf)
{
  if (buf == NULL)
    return;

  buf->pos = 0;
  if (buf->ptr != NULL)
    buf->ptr[buf->pos] = '\0';
}

void strbuf_reset2page(strbuf_t *buf);

static inline void strbuf_destroy(strbuf_t *buf)
{
  if (buf == NULL)
    return;

  if (buf->ptr != NULL) {
    free(buf->ptr);
    buf->ptr = NULL;
  }
}
#endif
