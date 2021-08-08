/**
 * collectd - src/utils_strbuf.h
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

#include "collectd.h"

#include "utils/strbuf/strbuf.h"

#include <stdarg.h>

static size_t strbuf_pagesize()
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

  size_t s_len = strlen(s);
  int status = strbuf_resize(buf, s_len);
  if (status != 0)
    return status;

  size_t bytes = strbuf_avail(buf);
  if (bytes == 0)
    return ENOSPC;

  if (bytes > s_len)
    bytes = s_len;

  memmove(buf->ptr + buf->pos, s, bytes);
  buf->pos += bytes;
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

  status = strbuf_resize(buf, s_len);
  if (status != 0)
    return status;

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

#if !HAVE_STRNLEN
static size_t strnlen(const char *s, size_t maxlen) {
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
  if (n == 0) {
    return 0;
  }

  size_t s_len = strnlen(s, n);
  int status = strbuf_resize(buf, s_len);
  if (status != 0)
    return status;

  size_t bytes = strbuf_avail(buf);
  if (bytes == 0)
    return ENOSPC;

  if (bytes > s_len)
    bytes = s_len;

  memmove(buf->ptr + buf->pos, s, bytes);
  buf->pos += bytes;
  buf->ptr[buf->pos] = 0;

  return 0;
}

int strbuf_print_escaped(strbuf_t *buf, char const *s, char const *need_escape,
                         char escape_char)
{
  if ((buf == NULL) || (s == NULL) || (need_escape == NULL) ||
      (escape_char == 0)) {
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

/* 
 * https://github.com/ulfjack/ryu/pull/75
 * https://github.com/jorgbrown
 */

static inline uint32_t digits10(const uint64_t val)
{
  static uint64_t table[20] = {
    0ull,
    9ull,
    99ull,
    999ull,
    9999ull,
    99999ull,
    999999ull,
    9999999ull,
    99999999ull,
    999999999ull,
    9999999999ull,
    99999999999ull,
    999999999999ull,
    9999999999999ull,
    99999999999999ull,
    999999999999999ull,
    9999999999999999ull,
    99999999999999999ull,
    999999999999999999ull,
    9999999999999999999ull};
  static unsigned char digits2N[64] = {
    1,  1,  1,  1,  2,  2,  2,  3,
    3,  3,  4,  4,  4,  4,  5,  5,
    5,  6,  6,  6,  7,  7,  7,  7,
    8,  8,  8,  9,  9,  9,  10, 10,
    10, 10, 11, 11, 11, 12, 12, 12,
    13, 13, 13, 13, 14, 14, 14, 15,
    15, 15, 16, 16, 16, 16, 17, 17,
    17, 18, 18, 18, 19, 19, 19, 19};
  uint32_t guess;

  if (val == 0) return 1;
  guess = digits2N [63 ^ __builtin_clzll(val)];
  return  guess + (val > table[guess] ? 1 : 0);
}


static inline uint32_t itoa(uint64_t value, char* dst)
{
  static const char digits[201] =
      "0001020304050607080910111213141516171819"
      "2021222324252627282930313233343536373839"
      "4041424344454647484950515253545556575859"
      "6061626364656667686970717273747576777879"
      "8081828384858687888990919293949596979899";
  uint32_t const length = digits10(value);
  uint32_t next = length - 1;

  while (value >= 100) {
      int const i = (value % 100) * 2;
      value /= 100;
      dst[next] = digits[i + 1];
      dst[next - 1] = digits[i];
      next -= 2;
  }

  /* Handle last 1-2 digits */
  if (value < 10) {
      dst[next] = '0' + (uint32_t) value;
  } else {
      int i = (uint32_t) value * 2;
      dst[next] = digits[i + 1];
      dst[next - 1] = digits[i];
  }

  return length;
}

int strbuf_putint(strbuf_t *buf, int64_t value)
{
  if (strbuf_avail(buf) < 21) {
    if (strbuf_resize(buf, 21) < 0)
      return ENOMEM;
  }

  if (value < 0) {
    buf->ptr[buf->pos++] = '-';
  }

  size_t len = itoa((uint64_t)value, (char *)(buf->ptr + buf->pos));
  buf->pos += len;
  buf->ptr[buf->pos] = '\0';
  return 0;
}

int strbuf_putuint(strbuf_t *buf, uint64_t value)
{
  if (strbuf_avail(buf) < 21) {
    if (strbuf_resize(buf, 21) < 0)
      return ENOMEM;
  }

  size_t len = itoa(value, (char *)(buf->ptr + buf->pos));
  buf->pos += len;
  buf->ptr[buf->pos] = '\0';
  return 0;
}
