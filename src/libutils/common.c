// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2005-2014 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Niki W. Waibel <niki.waibel at gmx.net>
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>
// SPDX-FileContributor: Michał Mirosław <mirq-linux at rere.qmqm.pl>
// SPDX-FileContributor: Manoj Srivastava <srivasta at google.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "log.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"

/* for getaddrinfo */
#include <netdb.h>
#include <sys/types.h>

#include <poll.h>

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

/* for ntohl and htonl */
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
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

#if !defined(HAVE_GETPWNAM_R) && defined(HAVE_GETPWNAM)
static pthread_mutex_t getpwnam_r_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

#ifndef HAVE_STRERROR_R
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
}

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
        free(alloc_buffer);
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

#ifndef HAVE_STRERROR_R
    char *temp;

    pthread_mutex_lock(&strerror_r_lock);

    temp = strerror(errnum);
    sstrncpy(buf, temp, buflen);

    pthread_mutex_unlock(&strerror_r_lock);

#elif defined(STRERROR_R_CHAR_P) || defined(HAVE_STRERROR_R_CHAR_P)
    char *temp;
    temp = strerror_r(errnum, buf, buflen);
    if (buf[0] == '\0') {
        if ((temp != NULL) && (temp != buf) && (temp[0] != '\0'))
            sstrncpy(buf, temp, buflen);
        else
            sstrncpy(buf, "strerror_r did not return an error message", buflen);
    }

#else
    if (strerror_r(errnum, buf, buflen) != 0) {
        snprintf(buf, buflen, "Error #%i; Additionally, strerror_r failed.", errnum);
    }
#endif

  return buf;
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

int strjoin(char *buffer, size_t buffer_size, char **fields, size_t fields_num, const char *sep)
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
    free(temp);
    return 0;
}

int strunescape(char *buf, size_t buf_len)
{
    for (size_t i = 0; (i < buf_len) && (buf[i] != '\0'); ++i) {
        if (buf[i] != '\\')
            continue;

        if (((i + 1) >= buf_len) || (buf[i + 1] == 0)) {
            ERROR("string unescape: backslash found at end of string.");
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

        if (smaller->tv_usec <= larger->tv_usec) {
            delta->tv_usec = larger->tv_usec - smaller->tv_usec;
        } else {
            --delta->tv_sec;
            delta->tv_usec = 1000000 + larger->tv_usec - smaller->tv_usec;
        }
    }

    assert((delta == NULL) || ((0 <= delta->tv_usec) && (delta->tv_usec < 1000000)));

    return status;
}

ssize_t swrite(int fd, const void *buf, size_t count)
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

int parse_uinteger(const char *value_orig, uint64_t *ret_value)
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

    *ret_value= (uint64_t)strtoull(value, &endptr, 0);

    if (value == endptr) {
        ERROR("parse_integer: Failed to parse string as integer: \"%s\".", value);
        free(value);
        return -1;
    } else if ((NULL != endptr) && ('\0' != *endptr)) {
        INFO("parse_value: Ignoring trailing garbage \"%s\" after integer value. "
             "Input string was \"%s\".", endptr, value_orig);
    }

    free(value);
    return 0;
}

int parse_uinteger_file(char const *path, uint64_t *ret_value)
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

    return parse_uinteger(buffer, ret_value);
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
        ERROR("parse_double: Failed to parse string as double: \"%s\".", value);
        free(value);
        return -1;
    } else if ((NULL != endptr) && ('\0' != *endptr)) {
        INFO("parse_double: Ignoring trailing garbage \"%s\" after double value. "
             "Input string was \"%s\".", endptr, value_orig);
    }

    free(value);
    return 0;
}

int parse_double_file(char const *path, double *ret_value)
{
    FILE *fh = fopen(path, "r");
    if (fh == NULL)
        return -1;

    char buffer[256];
    if (fgets(buffer, sizeof(buffer), fh) == NULL) {
        fclose(fh);
        return -1;
    }

    fclose(fh);

    strstripnewline(buffer);

    return parse_double(buffer, ret_value);
}

ssize_t read_file_at(int dir_fd, char const *pathname, char *buf, size_t count)
{
    int fd = openat(dir_fd, pathname, O_RDONLY);
    if (fd < 0) {
        buf[0] = '\0';
        return -1;
    }

    ssize_t size = read(fd, buf, count-1);
    if (size <= 0) {
        buf[0] = '\0';
        close(fd);
        return -errno;
    }

    buf[size] = '\0';
    close(fd);
    return size;
}

ssize_t filetodouble_at(int dir_fd, char const *pathname, double *ret_value)
{
    char buf[256];
    ssize_t len = read_file_at(dir_fd, pathname, buf, sizeof(buf));
    if (len < 0)
        return len;

    return strtodouble(strntrim(buf, (size_t)len), ret_value);
}

ssize_t filetouint_at(int dir_fd, char const *pathname, uint64_t *ret_value)
{
    char buf[256];
    ssize_t len = read_file_at(dir_fd, pathname, buf, sizeof(buf));
    if (len < 0)
        return len;

    return strtouint(strntrim(buf, (size_t)len), ret_value);
}

#ifndef HAVE_GETPWNAM_R
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

#define GETPWNAM_COPY_MEMBER(member)                  \
    if (pw->member != NULL) {                         \
        int len = strlen(pw->member);                 \
        if (len >= buflen) {                          \
            status = ENOMEM;                          \
            break;                                    \
        }                                             \
        sstrncpy(buf, pw->member, buflen);            \
        pwbuf->member = buf;                          \
        buf += (len + 1);                             \
        buflen -= (len + 1);                          \
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
#endif
}
#endif

int walk_directory_at(int dir_fd_at, const char *dir, dirwalk_callback_f callback,
                      void *user_data, int include_hidden)
{
    DIR *dh = opendirat(dir_fd_at, dir);
    if (dh == NULL) {
        ERROR("walk_directory: Cannot open '%s': %s", dir, STRERRNO);
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

        int status = (*callback)(dirfd(dh), dir, ent->d_name, user_data);
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
        ERROR("read_file_contents: Reading file \"%s\" failed.", filename);
        ret = -1;
    }

    fclose(fh);
    return ret;
}

ssize_t read_text_file_contents(const char *filename, char *buf, size_t bufsize)
{
    ssize_t ret = read_file_contents(filename, buf, bufsize - 1);
    if (ret < 0)
        return ret;

    buf[ret] = '\0';
    return ret;
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
        ERROR("service_name_to_port_number: getaddrinfo failed: %s", gai_strerror(status));
        return -1;
    }

    service_number = -1;
    for (struct addrinfo *ai_ptr = ai_list; ai_ptr != NULL; ai_ptr = ai_ptr->ai_next) {
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

int strarray_add(char ***ret_array, size_t *ret_array_len, char const *str)
{
    size_t array_len = *ret_array_len;

    if (str == NULL)
        return EINVAL;

    char **array = realloc(*ret_array, (array_len + 1) * sizeof(*array));
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
        free(array[i]);
    free(array);
}

FILE *fopenat(int dir_fd, const char *pathname, const char *mode)
{
    int flags = 0;

    switch(mode[0]) {
    case 'r':
        flags = mode[1] == '+' ?  O_RDWR : O_RDONLY;
        break;
    case 'w':
        flags = mode[1] == '+' ?  O_RDWR : O_WRONLY;
        flags |= O_CREAT | O_TRUNC;
        break;
    case 'a':
        flags = mode[1] == '+' ?  O_RDWR : O_WRONLY;
        flags |= O_CREAT | O_APPEND;
        break;
    default:
        errno = EINVAL;
        return NULL;
    }

    int fd = openat(dir_fd, pathname, flags, 0644);
    if (fd == -1)
        return NULL;

    FILE *fh = fdopen(fd, mode);
    if (fh == NULL)
        close(fd);

    return fh;
}

DIR *opendirat(int dir_fd, const char *name)
{
    int fd = openat(dir_fd, name, O_RDONLY | O_DIRECTORY);
    if (fd < 0)
        return NULL;

    DIR *dh = fdopendir(fd);
    if (dh == NULL)
        close(fd);

    return dh;
}
