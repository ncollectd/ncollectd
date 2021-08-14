/**
 * collectd - src/common.h
 * Copyright (C) 2005-2014  Florian octo Forster
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Florian octo Forster <octo at collectd.org>
 *   Niki W. Waibel <niki.waibel@gmx.net>
 **/

#ifndef COMMON_H
#define COMMON_H

#include "config.h"
#include "collectd.h"
#include "plugin.h"

#include <string.h>

#if HAVE_PWD_H
#include <pwd.h>
#endif

#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#define sfree(ptr)                                                             \
  do {                                                                         \
    free(ptr);                                                                 \
    (ptr) = NULL;                                                              \
  } while (0)

#define STATIC_ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

#define IS_TRUE(s)                                                             \
  ((strcasecmp("true", (s)) == 0) || (strcasecmp("yes", (s)) == 0) ||          \
   (strcasecmp("on", (s)) == 0))
#define IS_FALSE(s)                                                            \
  ((strcasecmp("false", (s)) == 0) || (strcasecmp("no", (s)) == 0) ||          \
   (strcasecmp("off", (s)) == 0))

struct rate_to_counter_state_s {
  uint64_t last_value;
  cdtime_t last_time;
  double residual;
};
typedef struct rate_to_counter_state_s rate_to_counter_state_t;

struct counter_to_rate_state_s {
  uint64_t last_value;
  cdtime_t last_time;
};
typedef struct counter_to_rate_state_s counter_to_rate_state_t;

__attribute__((format(printf, 3, 4))) int ssnprintf(char *str, size_t size,
                                                    char const *format, ...);

__attribute__((format(printf, 1, 2))) char *ssnprintf_alloc(char const *format,
                                                            ...);

#if !HAVE_STRNLEN
static inline size_t strnlen(const char *s, size_t maxlen)
{
  for (size_t i = 0; i < maxlen; i++) {
    if (s[i] == 0) {
      return i;
    }
  }
  return maxlen;
}
#endif

static inline char *sstrncpy(char *dest, const char *src, size_t n)
{
  size_t len  = strnlen(src, n - 1);
  memcpy(dest, src, len);
  dest[len] = '\0';
/*
  strncpy(dest, src, n-1);
  dest[n - 1] = '\0';
*/
  return dest;
}

static inline char *sstrdup(const char *s)
{
  if (s == NULL)
    return NULL;

  /* Do not use `strdup' here, because it's not specified in POSIX. It's
   * ``only'' an XSI extension. */
  size_t sz = strlen(s) + 1;
  char *r = malloc(sz);
  if (r == NULL) {
    ERROR("sstrdup: Out of memory.");
    exit(3);
  }
  memcpy(r, s, sz);

  return r;
}

static inline char *sstrndup(const char *s, size_t n)
{
  if (s == NULL)
    return NULL;

  size_t sz = strnlen(s, n);
  char *r = malloc(sz + 1);
  if (r == NULL) {
    ERROR("sstrndup: Out of memory.");
    exit(3);
  }
  memcpy(r, s, sz);
  r[sz] = '\0';

  return r;
}

void *smalloc(size_t size);
char *sstrerror(int errnum, char *buf, size_t buflen);

#ifndef ERRBUF_SIZE
#define ERRBUF_SIZE 256
#endif

#define STRERROR(e) sstrerror((e), (char[ERRBUF_SIZE]){0}, ERRBUF_SIZE)
#define STRERRNO STRERROR(errno)

/*
 * NAME
 *   sread
 *
 * DESCRIPTION
 *   Reads exactly `n' bytes or fails. Syntax and other behavior is analogous
 *   to `read(2)'.
 *
 * PARAMETERS
 *   `fd'          File descriptor to write to.
 *   `buf'         Buffer that is to be written.
 *   `count'       Number of bytes in the buffer.
 *
 * RETURN VALUE
 *   Zero upon success or non-zero if an error occurred. `errno' is set in this
 *   case.
 */
static inline int sread(int fd, void *buf, size_t count)
{
  char *ptr = (char *)buf;
  size_t nleft = count;

  while (nleft > 0) {
    ssize_t status = read(fd, (void *)ptr, nleft);

    if ((status < 0) && ((errno == EAGAIN) || (errno == EINTR)))
      continue;

    if (status < 0)
      return status;

    if (status == 0) {
      DEBUG("Received EOF from fd %i. ", fd);
      return -1;
    }

    assert((0 > status) || (nleft >= (size_t)status));

    nleft = nleft - ((size_t)status);
    ptr = ptr + ((size_t)status);
  }

  return 0;
}

/*
 * NAME
 *   swrite
 *
 * DESCRIPTION
 *   Writes exactly `n' bytes or fails. Syntax and other behavior is analogous
 *   to `write(2)'.
 *
 * PARAMETERS
 *   `fd'          File descriptor to write to.
 *   `buf'         Buffer that is to be written.
 *   `count'       Number of bytes in the buffer.
 *
 * RETURN VALUE
 *   Zero upon success or non-zero if an error occurred. `errno' is set in this
 *   case.
 */
int swrite(int fd, const void *buf, size_t count);
/*
 * NAME
 *   strsplit
 *
 * DESCRIPTION
 *   Splits a string into parts and stores pointers to the parts in `fields'.
 *   The characters split at are: " ", "\t", "\r", and "\n".
 *
 * PARAMETERS
 *   `string'      String to split. This string will be modified. `fields' will
 *                 contain pointers to parts of this string, so free'ing it
 *                 will destroy `fields' as well.
 *   `fields'      Array of strings where pointers to the parts will be stored.
 *   `size'        Number of elements in the array. No more than `size'
 *                 pointers will be stored in `fields'.
 *
 * RETURN VALUE
 *    Returns the number of parts stored in `fields'.
 */
int strsplit(char *string, char **fields, size_t size);

/*
 * NAME
 *   strjoin
 *
 * DESCRIPTION
 *   Joins together several parts of a string using `sep' as a separator. This
 *   is equivalent to the Perl built-in `join'.
 *
 * PARAMETERS
 *   `dst'         Buffer where the result is stored. Can be NULL if you need to
 *                 determine the required buffer size only.
 *   `dst_len'     Length of the destination buffer. No more than this many
 *                 bytes will be written to the memory pointed to by `dst',
 *                 including the trailing null-byte. Must be zero if dst is
 *                 NULL.
 *   `fields'      Array of strings to be joined.
 *   `fields_num'  Number of elements in the `fields' array.
 *   `sep'         String to be inserted between any two elements of `fields'.
 *                 This string is neither prepended nor appended to the result.
 *                 Instead of passing "" (empty string) one can pass NULL.
 *
 * RETURN VALUE
 *   Returns the number of characters in the resulting string, excluding a
 *   tailing null byte. If this value is greater than or equal to "dst_len", the
 *   result in "dst" is truncated (but still null terminated). On error a
 *   negative value is returned.
 */
int strjoin(char *dst, size_t dst_len, char **fields, size_t fields_num,
            const char *sep);

/*
 * NAME
 *   escape_slashes
 *
 * DESCRIPTION
 *   Removes slashes ("/") from "buffer". If buffer contains a single slash,
 *   the result will be "root". Leading slashes are removed. All other slashes
 *   are replaced with underscores ("_").
 *   This function is used by plugin_dispatch_values() to escape all parts of
 *   the identifier.
 *
 * PARAMETERS
 *   `buffer'         String to be escaped.
 *   `buffer_size'    Size of the buffer. No more then this many bytes will be
 *                    written to `buffer', including the trailing null-byte.
 *
 * RETURN VALUE
 *   Returns zero upon success and a value smaller than zero upon failure.
 */
int escape_slashes(char *buffer, size_t buffer_size);

/**
 * NAME
 *   escape_string
 *
 * DESCRIPTION
 *   escape_string quotes and escapes a string to be usable with collectd's
 *   plain text protocol. "simple" strings are left as they are, for example if
 *   buffer is 'simple' before the call, it will remain 'simple'. However, if
 *   buffer contains 'more "complex"' before the call, the returned buffer will
 *   contain '"more \"complex\""'.
 *
 *   If the buffer is too small to contain the escaped string, the string will
 *   be truncated. However, leading and trailing double quotes, as well as an
 *   ending null byte are guaranteed.
 *
 * RETURN VALUE
 *   Returns zero on success, even if the string was truncated. Non-zero on
 *   failure.
 */
int escape_string(char *buffer, size_t buffer_size);

/*
 * NAME
 *   replace_special
 *
 * DESCRIPTION
 *   Replaces any special characters (anything that's not alpha-numeric or a
 *   dash) with an underscore.
 *
 *   E.g. "foo$bar&" would become "foo_bar_".
 *
 * PARAMETERS
 *   `buffer'      String to be handled.
 *   `buffer_size' Length of the string. The function returns after
 *                 encountering a null-byte or reading this many bytes.
 */
void replace_special(char *buffer, size_t buffer_size);

/*
 * NAME
 *   strunescape
 *
 * DESCRIPTION
 *   Replaces any escaped characters in a string with the appropriate special
 *   characters. The following escaped characters are recognized:
 *
 *     \t -> <tab>
 *     \n -> <newline>
 *     \r -> <carriage return>
 *
 *   For all other escacped characters only the backslash will be removed.
 *
 * PARAMETERS
 *   `buf'         String to be unescaped.
 *   `buf_len'     Length of the string, including the terminating null-byte.
 *
 * RETURN VALUE
 *   Returns zero upon success, a value less than zero else.
 */
int strunescape(char *buf, size_t buf_len);

/**
 * Removed trailing newline characters (CR and LF) from buffer, which must be
 * null terminated. Returns the length of the resulting string.
 */
__attribute__((nonnull(1))) size_t strstripnewline(char *buffer);

/*
 * NAME
 *   timeval_cmp
 *
 * DESCRIPTION
 *   Compare the two time values `tv0' and `tv1' and store the absolut value
 *   of the difference in the time value pointed to by `delta' if it does not
 *   equal NULL.
 *
 * RETURN VALUE
 *   Returns an integer less than, equal to, or greater than zero if `tv0' is
 *   less than, equal to, or greater than `tv1' respectively.
 */
int timeval_cmp(struct timeval tv0, struct timeval tv1, struct timeval *delta);

/* make sure tv_usec stores less than a second */
#define NORMALIZE_TIMEVAL(tv)                                                  \
  do {                                                                         \
    (tv).tv_sec += (tv).tv_usec / 1000000;                                     \
    (tv).tv_usec = (tv).tv_usec % 1000000;                                     \
  } while (0)

/* make sure tv_sec stores less than a second */
#define NORMALIZE_TIMESPEC(tv)                                                 \
  do {                                                                         \
    (tv).tv_sec += (tv).tv_nsec / 1000000000;                                  \
    (tv).tv_nsec = (tv).tv_nsec % 1000000000;                                  \
  } while (0)

int check_create_dir(const char *file_orig);

#ifdef HAVE_LIBKSTAT
#if HAVE_KSTAT_H
#include <kstat.h>
#endif
int get_kstat(kstat_t **ksp_ptr, char *module, int instance, char *name);
long long get_kstat_value(kstat_t *ksp, char *name);
#endif

#ifndef HAVE_HTONLL
static inline unsigned long long ntohll(unsigned long long n)
{
#if BYTE_ORDER == BIG_ENDIAN
  return n;
#else
  return (((unsigned long long)ntohl(n)) << 32) + ntohl(n >> 32);
#endif
}

static inline unsigned long long htonll(unsigned long long n)
{
#if BYTE_ORDER == BIG_ENDIAN
  return n;
#else
  return (((unsigned long long)htonl(n)) << 32) + htonl(n >> 32);
#endif
}
#endif /* HAVE_HTONLL */

#if FP_LAYOUT_NEED_NOTHING
/* Well, we need nothing.. */
/* #endif FP_LAYOUT_NEED_NOTHING */
#elif FP_LAYOUT_NEED_ENDIANFLIP || FP_LAYOUT_NEED_INTSWAP
#if FP_LAYOUT_NEED_ENDIANFLIP
#define FP_CONVERT(A)                                                          \
  ((((uint64_t)(A)&0xff00000000000000LL) >> 56) |                              \
   (((uint64_t)(A)&0x00ff000000000000LL) >> 40) |                              \
   (((uint64_t)(A)&0x0000ff0000000000LL) >> 24) |                              \
   (((uint64_t)(A)&0x000000ff00000000LL) >> 8) |                               \
   (((uint64_t)(A)&0x00000000ff000000LL) << 8) |                               \
   (((uint64_t)(A)&0x0000000000ff0000LL) << 24) |                              \
   (((uint64_t)(A)&0x000000000000ff00LL) << 40) |                              \
   (((uint64_t)(A)&0x00000000000000ffLL) << 56))
#else
#define FP_CONVERT(A)                                                          \
  ((((uint64_t)(A)&0xffffffff00000000LL) >> 32) |                              \
   (((uint64_t)(A)&0x00000000ffffffffLL) << 32))
#endif

static inline double ntohd(double d)
{
  union {
    uint8_t byte[8];
    uint64_t integer;
    double floating;
  } ret;

  ret.floating = d;

  /* NAN in x86 byte order */
  if ((ret.byte[0] == 0x00) && (ret.byte[1] == 0x00) && (ret.byte[2] == 0x00) &&
      (ret.byte[3] == 0x00) && (ret.byte[4] == 0x00) && (ret.byte[5] == 0x00) &&
      (ret.byte[6] == 0xf8) && (ret.byte[7] == 0x7f)) {
    return NAN;
  } else {
    uint64_t tmp;

    tmp = ret.integer;
    ret.integer = FP_CONVERT(tmp);
    return ret.floating;
  }
}

static inline double htond(double d)
{
  union {
    uint8_t byte[8];
    uint64_t integer;
    double floating;
  } ret;

  if (isnan(d)) {
    ret.byte[0] = ret.byte[1] = ret.byte[2] = ret.byte[3] = 0x00;
    ret.byte[4] = ret.byte[5] = 0x00;
    ret.byte[6] = 0xf8;
    ret.byte[7] = 0x7f;
    return ret.floating;
  } else {
    uint64_t tmp;

    ret.floating = d;
    tmp = FP_CONVERT(ret.integer);
    ret.integer = tmp;
    return ret.floating;
  }
}
#else
#error "Don't know how to convert between host and network representation of doubles."
#endif /* FP_LAYOUT_NEED_ENDIANFLIP || FP_LAYOUT_NEED_INTSWAP */

int parse_uinteger(const char *value_orig, uint64_t *ret_value);
int parse_double (const char *value_orig, double *ret_value);

int parse_uinteger_file(char const *path, uint64_t *ret_value);
int parse_double_file(char const *path, double *ret_value);

#if !HAVE_GETPWNAM_R
struct passwd;
int getpwnam_r(const char *name, struct passwd *pwbuf, char *buf, size_t buflen,
               struct passwd **pwbufp);
#endif

typedef int (*dirwalk_callback_f)(const char *dirname, const char *filename,
                                  void *user_data);
int walk_directory(const char *dir, dirwalk_callback_f callback,
                   void *user_data, int hidden);
/* Returns the number of bytes read or negative on error. */
ssize_t read_file_contents(char const *filename, void *buf, size_t bufsize);
/* Writes the contents of the file into the buffer with a trailing NUL.
 * Returns the number of bytes written to the buffer or negative on error. */
ssize_t read_text_file_contents(char const *filename, char *buf,
                                size_t bufsize);

uint64_t counter_diff(uint64_t old_value, uint64_t new_value);

int rate_to_counter(uint64_t *ret_value, double rate, cdtime_t t,
                  rate_to_counter_state_t *state);

int counter_to_rate(double *ret_rate, uint64_t value, cdtime_t t,
                    counter_to_rate_state_t *state);
/* Converts a service name (a string) to a port number
 * (in the range [1-65535]). Returns less than zero on error. */
int service_name_to_port_number(const char *service_name);

/* Sets various, non-default, socket options */
void set_sock_opts(int sockfd);

/** Parse a string to a integer value. Returns zero on success or non-zero on
 * failure. If failure is returned, ret_value is not touched. */
static inline int strtouint(const char *string, uint64_t *ret_value)
{
  if ((string == NULL) || (ret_value == NULL))
    return EINVAL;

  errno = 0;
  char *endptr = NULL;
  uint64_t tmp = (uint64_t)strtoull(string, &endptr, /* base = */ 0);
  if ((endptr == string) || (errno != 0))
    return -1;

  *ret_value = tmp;
  return 0;
}

/** Parse a string to a double value. Returns zero on success or non-zero on
 * failure. If failure is returned, ret_value is not touched. */
static inline int strtodouble(const char *string, double *ret_value) 
{
  if ((string == NULL) || (ret_value == NULL))
    return EINVAL;

  errno = 0;
  char *endptr = NULL;
  double tmp = (double )strtod(string, &endptr);
  if (errno != 0)
    return errno;
  else if ((endptr == NULL) || (*endptr != 0))
    return EINVAL;

  *ret_value = tmp;
  return 0;
}

int filetodouble_at(int dirfd, char const *pathname, double *ret_value);

static inline int filetodouble(char const *pathname, double *ret_value)
{
  return filetodouble_at(AT_FDCWD, pathname, ret_value);
}

int filetouint_at(int dirfd, char const *pathname, uint64_t *ret_value);

static inline int filetouint(int dirfd, char const *pathname, uint64_t *ret_value)
{
  return filetouint_at(AT_FDCWD, pathname, ret_value);
}


int strarray_add(char ***ret_array, size_t *ret_array_len, char const *str);
void strarray_free(char **array, size_t array_len);

/** Check if the current process benefits from the capability passed in
 * argument. Returns zero if it does, less than zero if it doesn't or on error.
 * See capabilities(7) for the list of possible capabilities.
 * */
int check_capability(int arg);

#endif /* COMMON_H */
