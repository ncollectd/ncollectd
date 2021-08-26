/**
 * collectd - src/common.c
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
 *   Sebastian Harl <sh at tokkee.org>
 *   Michał Mirosław <mirq-linux at rere.qmqm.pl>
 *   Manoj Srivastava <srivasta at google.com>
 **/

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"
#include "utils/strbuf/strbuf.h"
#include "utils_cache.h"

/* for getaddrinfo */
#include <netdb.h>
#include <sys/types.h>

#include <poll.h>

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#if HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

/* for ntohl and htonl */
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#if HAVE_CAPABILITY
#include <sys/capability.h>
#endif

#if HAVE_KSTAT_H
#include <kstat.h>
#endif

#ifdef HAVE_LIBKSTAT
extern kstat_ctl_t *kc;
#endif

#if !defined(MSG_DONTWAIT)
#if defined(MSG_NONBLOCK)
/* AIX doesn't have MSG_DONTWAIT */
#define MSG_DONTWAIT MSG_NONBLOCK
#else
/* Windows doesn't have MSG_DONTWAIT or MSG_NONBLOCK */
#define MSG_DONTWAIT 0
#endif /* defined(MSG_NONBLOCK) */
#endif /* !defined(MSG_DONTWAIT) */

#if !HAVE_GETPWNAM_R && defined(HAVE_GETPWNAM)
static pthread_mutex_t getpwnam_r_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

#if !HAVE_STRERROR_R
static pthread_mutex_t strerror_r_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

/* ssnprintf returns result from vsnprintf conistent with snprintf */
int ssnprintf(char *str, size_t sz, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);

  int ret = vsnprintf(str, sz, format, ap);

  va_end(ap);

  return ret;
} /* int ssnprintf */

char *ssnprintf_alloc(char const *format, ...)
{
  char static_buffer[1024] = "";
  char *alloc_buffer;
  size_t alloc_buffer_size;
  int status;
  va_list ap;

  /* Try printing into the static buffer. In many cases it will be
   * sufficiently large and we can simply return a strdup() of this
   * buffer. */
  va_start(ap, format);
  status = vsnprintf(static_buffer, sizeof(static_buffer), format, ap);
  va_end(ap);
  if (status < 0)
    return NULL;

  /* "status" does not include the null byte. */
  alloc_buffer_size = (size_t)(status + 1);
  if (alloc_buffer_size <= sizeof(static_buffer))
    return strdup(static_buffer);

  /* Allocate a buffer large enough to hold the string. */
  alloc_buffer = calloc(1, alloc_buffer_size);
  if (alloc_buffer == NULL)
    return NULL;

  /* Print again into this new buffer. */
  va_start(ap, format);
  status = vsnprintf(alloc_buffer, alloc_buffer_size, format, ap);
  va_end(ap);
  if (status < 0) {
    sfree(alloc_buffer);
    return NULL;
  }

  return alloc_buffer;
}

/* Even though Posix requires "strerror_r" to return an "int",
 * some systems (e.g. the GNU libc) return a "char *" _and_
 * ignore the second argument ... -tokkee */
char *sstrerror(int errnum, char *buf, size_t buflen)
{
  buf[0] = '\0';

#if !HAVE_STRERROR_R
  {
    char *temp;

    pthread_mutex_lock(&strerror_r_lock);

    temp = strerror(errnum);
    sstrncpy(buf, temp, buflen);

    pthread_mutex_unlock(&strerror_r_lock);
  }
  /* #endif !HAVE_STRERROR_R */

#elif STRERROR_R_CHAR_P
  {
    char *temp;
    temp = strerror_r(errnum, buf, buflen);
    if (buf[0] == '\0') {
      if ((temp != NULL) && (temp != buf) && (temp[0] != '\0'))
        sstrncpy(buf, temp, buflen);
      else
        sstrncpy(buf,
                 "strerror_r did not return "
                 "an error message",
                 buflen);
    }
  }
  /* #endif STRERROR_R_CHAR_P */

#else
  if (strerror_r(errnum, buf, buflen) != 0) {
    snprintf(buf, buflen,
             "Error #%i; "
             "Additionally, strerror_r failed.",
             errnum);
  }
#endif /* STRERROR_R_CHAR_P */

  return buf;
}

void *smalloc(size_t size)
{
  void *r;

  if ((r = malloc(size)) == NULL) {
    ERROR("Not enough memory.");
    exit(3);
  }

  return r;
}

int strsplit(char *string, char **fields, size_t size)
{
  size_t i = 0;
  char *ptr = string;
  char *saveptr = NULL;
  while ((fields[i] = strtok_r(ptr, " \t\r\n", &saveptr)) != NULL) {
    ptr = NULL;
    i++;

    if (i >= size)
      break;
  }

  return (int)i;
}

int strjoin(char *buffer, size_t buffer_size, char **fields, size_t fields_num,
            const char *sep)
{
  size_t avail = 0;
  char *ptr = buffer;
  size_t sep_len = 0;

  size_t buffer_req = 0;

  if (((fields_num != 0) && (fields == NULL)) ||
      ((buffer_size != 0) && (buffer == NULL)))
    return -EINVAL;

  if (buffer != NULL)
    buffer[0] = 0;

  if (buffer_size != 0)
    avail = buffer_size - 1;

  if (sep != NULL)
    sep_len = strlen(sep);

  for (size_t i = 0; i < fields_num; i++) {
    size_t field_len = strlen(fields[i]);

    if (i != 0)
      buffer_req += sep_len;
    buffer_req += field_len;

    if (buffer_size == 0)
      continue;

    if ((i != 0) && (sep_len > 0)) {
      if (sep_len >= avail) {
        /* prevent subsequent iterations from writing to the
         * buffer. */
        avail = 0;
        continue;
      }

      memcpy(ptr, sep, sep_len);

      ptr += sep_len;
      avail -= sep_len;
    }

    if (field_len > avail)
      field_len = avail;

    memcpy(ptr, fields[i], field_len);
    ptr += field_len;

    avail -= field_len;
    if (ptr != NULL)
      *ptr = 0;
  }

  return (int)buffer_req;
}

int escape_string(char *buffer, size_t buffer_size)
{
  char *temp;
  size_t j;

  /* Check if we need to escape at all first */
  temp = strpbrk(buffer, " \t\"\\");
  if (temp == NULL)
    return 0;

  if (buffer_size < 3)
    return EINVAL;

  temp = calloc(1, buffer_size);
  if (temp == NULL)
    return ENOMEM;

  temp[0] = '"';
  j = 1;

  for (size_t i = 0; i < buffer_size; i++) {
    if (buffer[i] == 0) {
      break;
    } else if ((buffer[i] == '"') || (buffer[i] == '\\')) {
      if (j > (buffer_size - 4))
        break;
      temp[j] = '\\';
      temp[j + 1] = buffer[i];
      j += 2;
    } else {
      if (j > (buffer_size - 3))
        break;
      temp[j] = buffer[i];
      j++;
    }
  }

  assert((j + 1) < buffer_size);
  temp[j] = '"';
  temp[j + 1] = 0;

  sstrncpy(buffer, temp, buffer_size);
  sfree(temp);
  return 0;
} /* int escape_string */

int strunescape(char *buf, size_t buf_len)
{
  for (size_t i = 0; (i < buf_len) && (buf[i] != '\0'); ++i) {
    if (buf[i] != '\\')
      continue;

    if (((i + 1) >= buf_len) || (buf[i + 1] == 0)) {
      P_ERROR("string unescape: backslash found at end of string.");
      /* Ensure null-byte at the end of the buffer. */
      buf[i] = 0;
      return -1;
    }

    switch (buf[i + 1]) {
    case 't':
      buf[i] = '\t';
      break;
    case 'n':
      buf[i] = '\n';
      break;
    case 'r':
      buf[i] = '\r';
      break;
    default:
      buf[i] = buf[i + 1];
      break;
    }

    /* Move everything after the position one position to the left.
     * Add a null-byte as last character in the buffer. */
    memmove(buf + i + 1, buf + i + 2, buf_len - i - 2);
    buf[buf_len - 1] = '\0';
  }
  return 0;
}

size_t strstripnewline(char *buffer)
{
  size_t buffer_len = strlen(buffer);

  while (buffer_len > 0) {
    if ((buffer[buffer_len - 1] != '\n') && (buffer[buffer_len - 1] != '\r'))
      break;
    buffer_len--;
    buffer[buffer_len] = 0;
  }

  return buffer_len;
}

int escape_slashes(char *buffer, size_t buffer_size)
{
  size_t buffer_len;

  buffer_len = strlen(buffer);

  if (buffer_len <= 1) {
    if (strcmp("/", buffer) == 0) {
      if (buffer_size < 5)
        return -1;
      sstrncpy(buffer, "root", buffer_size);
    }
    return 0;
  }

  /* Move one to the left */
  if (buffer[0] == '/') {
    memmove(buffer, buffer + 1, buffer_len);
    buffer_len--;
  }

  for (size_t i = 0; i < buffer_len; i++) {
    if (buffer[i] == '/')
      buffer[i] = '_';
  }

  return 0;
}

void replace_special(char *buffer, size_t buffer_size)
{
  for (size_t i = 0; i < buffer_size; i++) {
    if (buffer[i] == 0)
      return;
    if ((!isalnum((int)buffer[i])) && (buffer[i] != '-'))
      buffer[i] = '_';
  }
}

int timeval_cmp(struct timeval tv0, struct timeval tv1, struct timeval *delta)
{
  struct timeval *larger;
  struct timeval *smaller;

  int status;

  NORMALIZE_TIMEVAL(tv0);
  NORMALIZE_TIMEVAL(tv1);

  if ((tv0.tv_sec == tv1.tv_sec) && (tv0.tv_usec == tv1.tv_usec)) {
    if (delta != NULL) {
      delta->tv_sec = 0;
      delta->tv_usec = 0;
    }
    return 0;
  }

  if ((tv0.tv_sec < tv1.tv_sec) ||
      ((tv0.tv_sec == tv1.tv_sec) && (tv0.tv_usec < tv1.tv_usec))) {
    larger = &tv1;
    smaller = &tv0;
    status = -1;
  } else {
    larger = &tv0;
    smaller = &tv1;
    status = 1;
  }

  if (delta != NULL) {
    delta->tv_sec = larger->tv_sec - smaller->tv_sec;

    if (smaller->tv_usec <= larger->tv_usec)
      delta->tv_usec = larger->tv_usec - smaller->tv_usec;
    else {
      --delta->tv_sec;
      delta->tv_usec = 1000000 + larger->tv_usec - smaller->tv_usec;
    }
  }

  assert((delta == NULL) ||
         ((0 <= delta->tv_usec) && (delta->tv_usec < 1000000)));

  return status;
}

int check_create_dir(const char *file_orig)
{
  struct stat statbuf;

  char file_copy[PATH_MAX];
  char dir[PATH_MAX];
  char *fields[16];
  int fields_num;
  char *ptr;
  char *saveptr;
  int last_is_file = 1;
  int path_is_absolute = 0;
  size_t len;

  /*
   * Sanity checks first
   */
  if (file_orig == NULL)
    return -1;

  if ((len = strlen(file_orig)) < 1)
    return -1;
  else if (len >= sizeof(file_copy)) {
    ERROR("check_create_dir: name (%s) is too long.", file_orig);
    return -1;
  }

  /*
   * If `file_orig' ends in a slash the last component is a directory,
   * otherwise it's a file. Act accordingly..
   */
  if (file_orig[len - 1] == '/')
    last_is_file = 0;
  if (file_orig[0] == '/')
    path_is_absolute = 1;

  /*
   * Create a copy for `strtok_r' to destroy
   */
  sstrncpy(file_copy, file_orig, sizeof(file_copy));

  /*
   * Break into components. This will eat up several slashes in a row and
   * remove leading and trailing slashes..
   */
  ptr = file_copy;
  saveptr = NULL;
  fields_num = 0;
  while ((fields[fields_num] = strtok_r(ptr, "/", &saveptr)) != NULL) {
    ptr = NULL;
    fields_num++;

    if (fields_num >= 16)
      break;
  }

  /*
   * For each component, do..
   */
  for (int i = 0; i < (fields_num - last_is_file); i++) {
    /*
     * Do not create directories that start with a dot. This
     * prevents `../../' attacks and other likely malicious
     * behavior.
     */
    if (fields[i][0] == '.') {
      P_ERROR("Cowardly refusing to create a directory that "
              "begins with a `.' (dot): `%s'",
              file_orig);
      return -2;
    }

    /*
     * Join the components together again
     */
    dir[0] = '/';
    if (strjoin(dir + path_is_absolute,
                (size_t)(sizeof(dir) - path_is_absolute), fields,
                (size_t)(i + 1), "/") < 0) {
      P_ERROR("strjoin failed: `%s', component #%i", file_orig, i);
      return -1;
    }

    while (42) {
      if ((stat(dir, &statbuf) == -1) && (lstat(dir, &statbuf) == -1)) {
        if (errno == ENOENT) {
          if (mkdir(dir, S_IRWXU | S_IRWXG | S_IRWXO) == 0)
            break;

          /* this might happen, if a different thread created
           * the directory in the meantime
           * => call stat() again to check for S_ISDIR() */
          if (EEXIST == errno)
            continue;

          P_ERROR("check_create_dir: mkdir (%s): %s", dir, STRERRNO);
          return -1;
        } else {
          P_ERROR("check_create_dir: stat (%s): %s", dir, STRERRNO);
          return -1;
        }
      } else if (!S_ISDIR(statbuf.st_mode)) {
        P_ERROR("check_create_dir: `%s' exists but is not "
                "a directory!",
                dir);
        return -1;
      }
      break;
    }
  }

  return 0;
}

#ifdef HAVE_LIBKSTAT
int get_kstat(kstat_t **ksp_ptr, char *module, int instance, char *name)
{
  char ident[128];

  *ksp_ptr = NULL;

  if (kc == NULL)
    return -1;

  snprintf(ident, sizeof(ident), "%s,%i,%s", module, instance, name);

  *ksp_ptr = kstat_lookup(kc, module, instance, name);
  if (*ksp_ptr == NULL) {
    P_ERROR("get_kstat: Cound not find kstat %s", ident);
    return -1;
  }

  if ((*ksp_ptr)->ks_type != KSTAT_TYPE_NAMED) {
    P_ERROR("get_kstat: kstat %s has wrong type", ident);
    *ksp_ptr = NULL;
    return -1;
  }

#ifdef assert
  assert(*ksp_ptr != NULL);
  assert((*ksp_ptr)->ks_type == KSTAT_TYPE_NAMED);
#endif

  if (kstat_read(kc, *ksp_ptr, NULL) == -1) {
    P_ERROR("get_kstat: kstat %s could not be read", ident);
    return -1;
  }

  if ((*ksp_ptr)->ks_type != KSTAT_TYPE_NAMED) {
    P_ERROR("get_kstat: kstat %s has wrong type", ident);
    return -1;
  }

  return 0;
}

long long get_kstat_value(kstat_t *ksp, char *name)
{
  kstat_named_t *kn;
  long long retval = -1LL;

  if (ksp == NULL) {
    P_ERROR("get_kstat_value (\"%s\"): ksp is NULL.", name);
    return -1LL;
  } else if (ksp->ks_type != KSTAT_TYPE_NAMED) {
    P_ERROR("get_kstat_value (\"%s\"): ksp->ks_type (%#x) "
            "is not KSTAT_TYPE_NAMED (%#x).",
            name, (unsigned int)ksp->ks_type, (unsigned int)KSTAT_TYPE_NAMED);
    return -1LL;
  }

  if ((kn = (kstat_named_t *)kstat_data_lookup(ksp, name)) == NULL)
    return -1LL;

  if (kn->data_type == KSTAT_DATA_INT32)
    retval = (long long)kn->value.i32;
  else if (kn->data_type == KSTAT_DATA_UINT32)
    retval = (long long)kn->value.ui32;
  else if (kn->data_type == KSTAT_DATA_INT64)
    retval = (long long)kn->value.i64; /* According to ANSI C99 `long long' must
                                          hold at least 64 bits */
  else if (kn->data_type == KSTAT_DATA_UINT64)
    retval = (long long)kn->value.ui64; /* XXX: Might overflow! */
  else
    P_WARNING("get_kstat_value: Not a numeric value: %s", name);

  return retval;
}
#endif /* HAVE_LIBKSTAT */

int swrite(int fd, const void *buf, size_t count)
{
  const char *ptr = (const char *)buf;
  size_t nleft = count;

  if (fd < 0) {
    errno = EINVAL;
    return errno;
  }

  /* checking for closed peer connection */
  struct pollfd pfd;
  pfd.fd = fd;
  pfd.events = POLLIN | POLLHUP;
  pfd.revents = 0;
  if (poll(&pfd, 1, 0) > 0) {
    char buffer[32];
    if (recv(fd, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT) == 0) {
      /* if recv returns zero (even though poll() said there is data to be
       * read), that means the connection has been closed */
      errno = ECONNRESET;
      return -1;
    }
  }

  while (nleft > 0) {
    ssize_t status = write(fd, (const void *)ptr, nleft);

    if ((status < 0) && ((errno == EAGAIN) || (errno == EINTR)))
      continue;

    if (status < 0)
      return errno ? errno : status;

    nleft = nleft - ((size_t)status);
    ptr = ptr + ((size_t)status);
  }

  return 0;
}

#if 0
int format_values(strbuf_t *buf, metric_t const *m, bool store_rates)
{
  strbuf_printf(buf, "%.3f", CDTIME_T_TO_DOUBLE(m->time));

  if (m->family->type == METRIC_TYPE_GAUGE) {
    /* Solaris' printf tends to print NAN as "-nan", breaking unit tests, so we
     * introduce a special case here. */
    if (isnan(m->value.gauge)) {
      strbuf_print(buf, ":nan");
    } else {
      strbuf_printf(buf, ":" GAUGE_FORMAT, m->value.gauge);
    }
  } else if (store_rates) {
    double rates = NAN;
    int status = uc_get_rate(m, &rates);
    if (status != 0) {
      WARNING("format_values: uc_get_rate failed.");
      return status;
    }
    if (isnan(rates)) {
      strbuf_print(buf, ":nan");
    } else {
      strbuf_printf(buf, ":" GAUGE_FORMAT, rates);
    }
  } else if (m->family->type == METRIC_TYPE_COUNTER) {
    strbuf_printf(buf, ":%" PRIu64, m->value.counter);
  } else {
    ERROR("format_values: Unknown metric type: %d", m->family->type);
    return -1;
  }

  return 0;
}
#endif

int parse_integer(const char *value_orig, int64_t *ret_value)
{
  char *value;
  char *endptr = NULL;
  size_t value_len;

  if (value_orig == NULL)
    return EINVAL;

  value = strdup(value_orig);
  if (value == NULL)
    return ENOMEM;
  value_len = strlen(value);

  while ((value_len > 0) && isspace((int)value[value_len - 1])) {
    value[value_len - 1] = '\0';
    value_len--;
  }

  *ret_value= (int64_t)strtoull(value, &endptr, 0);

  if (value == endptr) {
    P_ERROR("parse_integer: Failed to parse string as integer: \"%s\".", value);
    sfree(value);
    return -1;
  } else if ((NULL != endptr) && ('\0' != *endptr)) {
    P_INFO("parse_value: Ignoring trailing garbage \"%s\" after integer value. "
           "Input string was \"%s\".", endptr, value_orig);
  }

  sfree(value);
  return 0;
}

int parse_integer_file(char const *path, int64_t *ret_value)
{
  FILE *fh;
  char buffer[256];

  fh = fopen(path, "r");
  if (fh == NULL)
    return -1;

  if (fgets(buffer, sizeof(buffer), fh) == NULL) {
    fclose(fh);
    return -1;
  }

  fclose(fh);

  strstripnewline(buffer);

  return parse_integer(buffer, ret_value);
}

int parse_double(const char *value_orig, double *ret_value)
{
  char *value;
  char *endptr = NULL;
  size_t value_len;

  if (value_orig == NULL)
    return EINVAL;

  value = strdup(value_orig);
  if (value == NULL)
    return ENOMEM;
  value_len = strlen(value);

  while ((value_len > 0) && isspace((int)value[value_len - 1])) {
    value[value_len - 1] = '\0';
    value_len--;
  }

  *ret_value = (double)strtod(value, &endptr);

  if (value == endptr) {
    P_ERROR("parse_double: Failed to parse string as double: \"%s\".", value);
    sfree(value);
    return -1;
  } else if ((NULL != endptr) && ('\0' != *endptr)) {
    P_INFO("parse_double: Ignoring trailing garbage \"%s\" after double value. "
           "Input string was \"%s\".", endptr, value_orig);
  }

  sfree(value);
  return 0;
}

int parse_double_file(char const *path, double *ret_value)
{
  FILE *fh;
  char buffer[256];

  fh = fopen(path, "r");
  if (fh == NULL)
    return -1;

  if (fgets(buffer, sizeof(buffer), fh) == NULL) {
    fclose(fh);
    return -1;
  }

  fclose(fh);

  strstripnewline(buffer);

  return parse_double(buffer, ret_value);
}

static inline ssize_t read_file_at(int dirfd, char const *pathname, char *buf, size_t count)
{
  int fd = openat(dirfd, pathname, O_RDONLY);
  if (fd < 0)
    return -1;

  ssize_t size = read(fd, buf, count-1);
  if (size < 0) {
    close(fd);
    return -errno;
  }

  buf[size] = '\0';
  close(fd);
  return size;
}

static inline char *strntrim(char *s, size_t n)
{
  while (n > 0) {
    if ((s[0] == ' ')  || (s[0] == '\t') ||
        (s[0] == '\n') || (s[0] == '\r')) {
      s++;
      n--;
    } else {
      break;
    }
  }

  while (n > 0) {
    if ((s[n - 1] == '\n') || (s[n -1] == '\r') ||
        (s[n - 1] == ' ')  || (s[n -1] == '\t')) {
      n--;
      s[n] = '\0';
    } else {
      break;
    }
  }

  return s;
}

int filetodouble_at(int dirfd, char const *pathname, double *ret_value)
{
  char buf[256];
  ssize_t len = read_file_at(dirfd, pathname, buf, sizeof(buf));
  if (len < 0)
    return len;
  return strtodouble(strntrim(buf, len), ret_value);
}

int filetouint_at(int dirfd, char const *pathname, uint64_t *ret_value)
{
  char buf[256];
  ssize_t len = read_file_at(dirfd, pathname, buf, sizeof(buf));
  if (len < 0)
    return len;
  return strtouint(strntrim(buf, len), ret_value);
}

#if !HAVE_GETPWNAM_R
int getpwnam_r(const char *name, struct passwd *pwbuf, char *buf, size_t buflen,
               struct passwd **pwbufp)
{
#ifndef HAVE_GETPWNAM
  return -1;
#else
  int status = 0;
  struct passwd *pw;

  memset(pwbuf, '\0', sizeof(struct passwd));

  pthread_mutex_lock(&getpwnam_r_lock);

  do {
    pw = getpwnam(name);
    if (pw == NULL) {
      status = (errno != 0) ? errno : ENOENT;
      break;
    }

#define GETPWNAM_COPY_MEMBER(member)                                           \
  if (pw->member != NULL) {                                                    \
    int len = strlen(pw->member);                                              \
    if (len >= buflen) {                                                       \
      status = ENOMEM;                                                         \
      break;                                                                   \
    }                                                                          \
    sstrncpy(buf, pw->member, buflen);                                         \
    pwbuf->member = buf;                                                       \
    buf += (len + 1);                                                          \
    buflen -= (len + 1);                                                       \
  }
    GETPWNAM_COPY_MEMBER(pw_name);
    GETPWNAM_COPY_MEMBER(pw_passwd);
    GETPWNAM_COPY_MEMBER(pw_gecos);
    GETPWNAM_COPY_MEMBER(pw_dir);
    GETPWNAM_COPY_MEMBER(pw_shell);

    pwbuf->pw_uid = pw->pw_uid;
    pwbuf->pw_gid = pw->pw_gid;

    if (pwbufp != NULL)
      *pwbufp = pwbuf;
  } while (0);

  pthread_mutex_unlock(&getpwnam_r_lock);

  return status;
#endif /* HAVE_GETPWNAM */
}
#endif /* !HAVE_GETPWNAM_R */

int walk_directory_at(int dirfd_at, const char *dir, dirwalk_callback_f callback,
                      void *user_data, int include_hidden)
{
  int dirfd = openat(dirfd_at, dir, O_RDONLY | O_DIRECTORY);
  if (dirfd < 0) {
    P_ERROR("walk_directory: Cannot open '%s': %s", dir, STRERRNO);
    return -1;
  }

  DIR *dh = fdopendir(dirfd);
  if (dh == NULL) {
    P_ERROR("walk_directory: Cannot open '%s': %s", dir, STRERRNO);
    close(dirfd);
    return -1;
  }

  int success = 0;
  int failure = 0;

  struct dirent *ent;
  while ((ent = readdir(dh)) != NULL) {
    if (include_hidden) {
      if ((strcmp(".", ent->d_name) == 0) || (strcmp("..", ent->d_name) == 0))
        continue;
    } else if (ent->d_name[0] == '.') {
      continue;
    }

    int status = (*callback)(dirfd, dir, ent->d_name, user_data);
    if (status != 0)
      failure++;
    else
      success++;
  }

  closedir(dh);

  if ((success == 0) && (failure > 0))
    return -1;
  return 0;
}

ssize_t read_file_contents(const char *filename, void *buf, size_t bufsize)
{
  FILE *fh;
  ssize_t ret;

  fh = fopen(filename, "r");
  if (fh == NULL)
    return -1;

  ret = (ssize_t)fread(buf, 1, bufsize, fh);
  if ((ret == 0) && (ferror(fh) != 0)) {
    P_ERROR("read_file_contents: Reading file \"%s\" failed.", filename);
    ret = -1;
  }

  fclose(fh);
  return ret;
}

ssize_t read_text_file_contents(const char *filename, char *buf,
                                size_t bufsize)
{
  ssize_t ret = read_file_contents(filename, buf, bufsize - 1);
  if (ret < 0)
    return ret;

  buf[ret] = '\0';
  return ret + 1;
}

uint64_t counter_diff(uint64_t old_value, uint64_t new_value)
{
  uint64_t diff;

  if (old_value > new_value) {
    if (old_value <= 4294967295U)
      diff = (4294967295U - old_value) + new_value + 1;
    else
    diff = (18446744073709551615ULL - old_value) + new_value + 1;
  } else {
    diff = new_value - old_value;
  }

  return diff;
}

int rate_to_counter(uint64_t *ret_value, double rate, cdtime_t t, rate_to_counter_state_t *state)
{
  /* Counter can't handle negative rates. Reset "last time" to zero, so that
   * the next valid rate will re-initialize the structure. */
  if (rate < 0.0) {
    memset(state, 0, sizeof(*state));
    return EINVAL;
  }

  /* Another invalid state: The time is not increasing. */
  if (t <= state->last_time) {
    memset(state, 0, sizeof(*state));
    return EINVAL;
  }

  cdtime_t delta_t = t - state->last_time;
  double delta_gauge = (rate * CDTIME_T_TO_DOUBLE(delta_t)) + state->residual;

  /* Previous value is invalid. */
  if (state->last_time == 0) {
    state->last_value = (uint64_t)rate;
    state->residual = rate - ((double)state->last_value);
    state->last_time = t;
    return EAGAIN;
  }

  uint64_t delta_counter = (uint64_t)delta_gauge;
  state->last_value += delta_counter;
  state->residual = delta_gauge - ((double)delta_counter);
  state->last_time = t;

  *ret_value = state->last_value;
  return 0;
}

int counter_to_rate(double *ret_rate, uint64_t value, cdtime_t t, counter_to_rate_state_t *state)
{
  /* Another invalid state: The time is not increasing. */
  if (t <= state->last_time) {
    memset(state, 0, sizeof(*state));
    return EINVAL;
  }

  double interval = CDTIME_T_TO_DOUBLE(t - state->last_time);

  /* Previous value is invalid. */
  if (state->last_time == 0) {
    state->last_value = value;
    state->last_time = t;
    return EAGAIN;
  }

  uint64_t diff = counter_diff(state->last_value, value);
  *ret_rate = (double)diff / interval;
  state->last_value = value;

  state->last_time = t;
  return 0;
}

int service_name_to_port_number(const char *service_name)
{ 
  struct addrinfo *ai_list;
  int status;
  int service_number;

  if (service_name == NULL)
    return -1;

  struct addrinfo ai_hints = {.ai_family = AF_UNSPEC};

  status = getaddrinfo(/* node = */ NULL, service_name, &ai_hints, &ai_list);
  if (status != 0) {
    P_ERROR("service_name_to_port_number: getaddrinfo failed: %s",
            gai_strerror(status));
    return -1;
  }

  service_number = -1;
  for (struct addrinfo *ai_ptr = ai_list; ai_ptr != NULL;
       ai_ptr = ai_ptr->ai_next) {
    if (ai_ptr->ai_family == AF_INET) {
      struct sockaddr_in *sa;

      sa = (void *)ai_ptr->ai_addr;
      service_number = (int)ntohs(sa->sin_port);
    } else if (ai_ptr->ai_family == AF_INET6) {
      struct sockaddr_in6 *sa;

      sa = (void *)ai_ptr->ai_addr;
      service_number = (int)ntohs(sa->sin6_port);
    }

    if (service_number > 0)
      break;
  }

  freeaddrinfo(ai_list);

  if (service_number > 0)
    return service_number;
  return -1;
}

void set_sock_opts(int sockfd)
{
  int status;
  int socktype;

  status = getsockopt(sockfd, SOL_SOCKET, SO_TYPE, &socktype,
                      &(socklen_t){sizeof(socktype)});
  if (status != 0) {
    P_WARNING("set_sock_opts: failed to determine socket type");
    return;
  }

  if (socktype == SOCK_STREAM) {
    status =
        setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &(int){1}, sizeof(int));
    if (status != 0)
      P_WARNING("set_sock_opts: failed to set socket keepalive flag");

#ifdef TCP_KEEPIDLE
    int tcp_keepidle = ((CDTIME_T_TO_MS(plugin_get_interval()) - 1) / 100 + 1);
    status = setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &tcp_keepidle,
                        sizeof(tcp_keepidle));
    if (status != 0)
      P_WARNING("set_sock_opts: failed to set socket tcp keepalive time");
#endif

#ifdef TCP_KEEPINTVL
    int tcp_keepintvl =
        ((CDTIME_T_TO_MS(plugin_get_interval()) - 1) / 1000 + 1);
    status = setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &tcp_keepintvl,
                        sizeof(tcp_keepintvl));
    if (status != 0)
      P_WARNING("set_sock_opts: failed to set socket tcp keepalive interval");
#endif
  }
}


int strarray_add(char ***ret_array, size_t *ret_array_len,
                 char const *str)
{
  char **array;
  size_t array_len = *ret_array_len;

  if (str == NULL)
    return EINVAL;

  array = realloc(*ret_array, (array_len + 1) * sizeof(*array));
  if (array == NULL)
    return ENOMEM;
  *ret_array = array;

  array[array_len] = strdup(str);
  if (array[array_len] == NULL)
    return ENOMEM;

  array_len++;
  *ret_array_len = array_len;
  return 0;
}

void strarray_free(char **array, size_t array_len)
{
  for (size_t i = 0; i < array_len; i++)
    sfree(array[i]);
  sfree(array);
}

#if HAVE_CAPABILITY
int check_capability(int arg)
{
  cap_value_t cap_value = (cap_value_t)arg;
  cap_t cap;
  cap_flag_value_t cap_flag_value;

  if (!CAP_IS_SUPPORTED(cap_value))
    return -1;

  if (!(cap = cap_get_proc())) {
    P_ERROR("check_capability: cap_get_proc failed.");
    return -1;
  }

  if (cap_get_flag(cap, cap_value, CAP_EFFECTIVE, &cap_flag_value) < 0) {
    P_ERROR("check_capability: cap_get_flag failed.");
    cap_free(cap);
    return -1;
  }
  cap_free(cap);

  return cap_flag_value != CAP_SET;
}
#else
int check_capability(__attribute__((unused)) int arg)
{
  P_WARNING("check_capability: unsupported capability implementation. "
            "Some plugin(s) may require elevated privileges to work properly.");
  return 0;
}
#endif /* HAVE_CAPABILITY */
