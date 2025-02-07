// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (c) 2009-2014 Kazuho Oku
// SPDX-FileCopyrightText: Copyright (c) 2009-2014 Tokuhiro Matsuno
// SPDX-FileCopyrightText: Copyright (c) 2009-2014 Daisuke Murase
// SPDX-FileCopyrightText: Copyright (c) 2009-2014 Shigeo Mitsunari
// SPDX-FileContributor: Kazuho Oku
// SPDX-FileContributor: Tokuhiro Matsuno
// SPDX-FileContributor: Daisuke Murase
// SPDX-FileContributor: Shigeo Mitsunari

/* From PicoHTTPParser */

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#ifdef __SSE4_2__
#include <x86intrin.h>
#endif

#include "libhttp/parser.h"

#pragma GCC diagnostic ignored "-Wcast-align"

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define ALIGNED(n) __attribute__((aligned(n)))

#define IS_PRINTABLE_ASCII(c) ((unsigned char)(c)-040u < 0137u)

static const char *token_char_map =
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\1\0\1\1\1\1\1\0\0\1\1\0\1\1\0\1\1\1\1\1\1\1\1\1\1\0\0\0\0\0\0"
    "\0\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\0\0\0\1\1"
    "\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\0\1\0\1\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

static const char *findchar_fast(const char *buf, const char *buf_end,
                                 const char *ranges, size_t ranges_size, int *found)
{
    *found = 0;
#ifdef __SSE4_2__
    if (likely(buf_end - buf >= 16)) {
        __m128i ranges16 = _mm_loadu_si128((const __m128i *)ranges);

        size_t left = (buf_end - buf) & ~15;
        do {
            __m128i b16 = _mm_loadu_si128((const __m128i *)buf);
            int r = _mm_cmpestri(ranges16, ranges_size, b16, 16,
                                 _SIDD_LEAST_SIGNIFICANT | _SIDD_CMP_RANGES | _SIDD_UBYTE_OPS);
            if (unlikely(r != 16)) {
                buf += r;
                *found = 1;
                break;
            }
            buf += 16;
            left -= 16;
        } while (likely(left != 0));
    }
#else
    /* suppress unused parameter warning */
    (void)buf_end;
    (void)ranges;
    (void)ranges_size;
#endif
    return buf;
}

static const char *get_token_to_eol(const char *buf, const char *buf_end,
                                    const char **token, size_t *token_len, int *ret)
{
    const char *token_start = buf;

#ifdef __SSE4_2__
    static const char ALIGNED(16) ranges1[16] = "\0\010"    /* allow HT */
                                                "\012\037"  /* allow SP and up to but not including DEL */
                                                "\177\177"; /* allow chars w. MSB set */
    int found;
    buf = findchar_fast(buf, buf_end, ranges1, 6, &found);
    if (found)
        goto found_ctl;
#else
    /* find non-printable char within the next 8 bytes, this is the hottest code; manually inlined */
    while (likely(buf_end - buf >= 8)) {
        if (unlikely(!IS_PRINTABLE_ASCII(*buf))) goto nonprintable;
        ++buf;
        if (unlikely(!IS_PRINTABLE_ASCII(*buf))) goto nonprintable;
        ++buf;
        if (unlikely(!IS_PRINTABLE_ASCII(*buf))) goto nonprintable;
        ++buf;
        if (unlikely(!IS_PRINTABLE_ASCII(*buf))) goto nonprintable;
        ++buf;
        if (unlikely(!IS_PRINTABLE_ASCII(*buf))) goto nonprintable;
        ++buf;
        if (unlikely(!IS_PRINTABLE_ASCII(*buf))) goto nonprintable;
        ++buf;
        if (unlikely(!IS_PRINTABLE_ASCII(*buf))) goto nonprintable;
        ++buf;
        if (unlikely(!IS_PRINTABLE_ASCII(*buf))) goto nonprintable;
        ++buf;
        continue;
nonprintable:
        if ((likely((unsigned char)*buf < '\040') && likely(*buf != '\011')) ||
            unlikely(*buf == '\177'))
            goto found_ctl;
        ++buf;
    }
#endif

    for (;; ++buf) {
        if (buf == buf_end) goto nodata;
        if (unlikely(!IS_PRINTABLE_ASCII(*buf))) {
            if ((likely((unsigned char)*buf < '\040') && likely(*buf != '\011')) ||
                unlikely(*buf == '\177'))
                goto found_ctl;
        }
    }

found_ctl:
    if (likely(*buf == '\015')) {
        ++buf;
        if (buf == buf_end) goto nodata;
        if (*buf++ != '\012') goto error;
        *token_len = buf - 2 - token_start;
    } else if (*buf == '\012') {
        *token_len = buf - token_start;
        ++buf;
    } else {
        goto error;
    }
    *token = token_start;

    return buf;
nodata:
    *ret = -2;
    return NULL;
error:
    *ret = -1;
    return NULL;
}

static const char *is_complete(const char *buf, const char *buf_end, size_t last_len, int *ret)
{
    int ret_cnt = 0;
    buf = last_len < 3 ? buf : buf + last_len - 3;

    while (1) {
        if (buf == buf_end) goto nodata;
        if (*buf == '\015') {
            ++buf;
            if (buf == buf_end) goto nodata;
            if (*buf++ != '\012') goto error;
            ++ret_cnt;
        } else if (*buf == '\012') {
            ++buf;
            ++ret_cnt;
        } else {
            ++buf;
            ret_cnt = 0;
        }
        if (ret_cnt == 2)
            return buf;
    }

nodata:
    *ret = -2;
    return NULL;
error:
    *ret = -1;
    return NULL;
}

/* returned pointer is always within [buf, buf_end), or null */
static const char *parse_token(const char *buf, const char *buf_end,
                               const char **token, size_t *token_len, char next_char, int *ret)
{
    /* We use pcmpestri to detect non-token characters.
     * This instruction can take no more than eight character ranges (8*2*8=128
     * bits that is the size of a SSE register).
     * Due to this restriction, characters `|` and `~` are handled in the slow loop. */
    static const char ALIGNED(16) ranges[] = "\x00 "  /* control chars and up to SP */
                                             "\"\""   /* 0x22 */
                                             "()"     /* 0x28,0x29 */
                                             ",,"     /* 0x2c */
                                             "//"     /* 0x2f */
                                             ":@"     /* 0x3a-0x40 */
                                             "[]"     /* 0x5b-0x5d */
                                             "{\xff"; /* 0x7b-0xff */
    const char *buf_start = buf;
    int found;
    buf = findchar_fast(buf, buf_end, ranges, sizeof(ranges) - 1, &found);
    if (!found) {
        if (buf == buf_end) goto nodata;
    }

    while (1) {
        if (*buf == next_char) {
            break;
        } else if (!token_char_map[(unsigned char)*buf]) {
            goto error;
        }
        ++buf;
        if (buf == buf_end) goto nodata;
    }

    *token = buf_start;
    *token_len = buf - buf_start;
    return buf;
nodata:
    *ret = -2;
    return NULL;
error:
    *ret = -1;
    return NULL;
}

/* returned pointer is always within [buf, buf_end), or null */
static const char *parse_http_version(const char *buf, const char *buf_end,
                                      http_version_t *http_version, int *minor_version, int *ret)
{
    /* we want at least [HTTP/1.<two chars>] to try to parse */
    if (buf_end - buf < 9) goto nodata;

    if (buf[0] != 'H') goto error;
    if (buf[1] != 'T') goto error;
    if (buf[2] != 'T') goto error;
    if (buf[3] != 'P') goto error;
    if (buf[4] != '/') goto error;
    if (buf[5] != '1') goto error;
    if (buf[6] != '.') goto error;

    buf += 7;

    if ((*buf < '0') || (*buf > '9')) goto error;

    *minor_version = *buf++ - '0';

    if (*minor_version == 1)
        *http_version = HTTP_VERSION_1_1;
    else if (*minor_version == 0)
        *http_version = HTTP_VERSION_1_0;
    else
        *http_version = HTTP_VERSION_UNKNOW;

    return buf;
nodata:
    *ret = -2;
    return NULL;
error:
    *ret = -1;
    return NULL;
}

static inline http_method_t parse_http_method(const char *m, size_t len)
{
    switch (len) {
    case 3:
        if (memcmp(m, "GET", 3) == 0)
            return HTTP_METHOD_GET;
        else if  (memcmp(m, "PUT", 3) == 0)
            return HTTP_METHOD_PUT;
        break;
    case 4:
        if (memcmp(m, "POST", 4) == 0)
            return HTTP_METHOD_POST;
        else if  (memcmp(m, "HEAD", 4) == 0)
            return HTTP_METHOD_HEAD;
        break;
    case 5:
        if (memcmp(m, "PATCH", 5) == 0)
            return HTTP_METHOD_PATCH;
        else if  (memcmp(m, "TRACE", 5) == 0)
            return HTTP_METHOD_TRACE;
        break;
    case 6:
        if (memcmp(m, "DELETE", 6) == 0)
            return HTTP_METHOD_DELETE;
        break;
    case 7:
        if (memcmp(m, "OPTIONS", 7) == 0)
            return HTTP_METHOD_OPTIONS;
        else if (memcmp(m, "CONNECT", 7) == 0)
            return HTTP_METHOD_CONNECT;
        break;
    }

    return HTTP_METHOD_UNKNOW;
}

static const char *parse_headers(const char *buf, const char *buf_end,
                                 http_parse_header_t *headers, size_t *num_headers,
                                 size_t max_headers, int *ret)
{
    for (;; ++*num_headers) {
        if (buf == buf_end) goto nodata;
        if (*buf == '\015') {
            ++buf;
            if (buf == buf_end) goto nodata;
            if (*buf++ != '\012') goto error;
            break;
        } else if (*buf == '\012') {
            ++buf;
            break;
        }

        if (*num_headers == max_headers)
            goto error;

        if (!(*num_headers != 0 && (*buf == ' ' || *buf == '\t'))) {
            /* parsing name, but do not discard SP before colon, see
             * http://www.mozilla.org/security/announce/2006/mfsa2006-33.html */
            buf = parse_token(buf, buf_end,
                              &headers[*num_headers].name, &headers[*num_headers].name_len,
                              ':', ret);
            if (buf == NULL)
                return NULL;

            if (headers[*num_headers].name_len == 0)
                goto error;

            headers[*num_headers].header = http_get_header_name(headers[*num_headers].name,
                                                                headers[*num_headers].name_len);
            ++buf;
            for (;; ++buf) {
                if (buf == buf_end) goto nodata;
                if (!(*buf == ' ' || *buf == '\t')) {
                    break;
                }
            }
        } else {
            headers[*num_headers].header = HTTP_HEADER_UNKOWN;
            headers[*num_headers].name = NULL;
            headers[*num_headers].name_len = 0;
        }

        const char *value;
        size_t value_len;
        buf = get_token_to_eol(buf, buf_end, &value, &value_len, ret);
        if (buf == NULL)
            return NULL;

        /* remove trailing SPs and HTABs */
        const char *value_end = value + value_len;
        for (; value_end != value; --value_end) {
            const char c = *(value_end - 1);
            if (!(c == ' ' || c == '\t')) {
                break;
            }
        }
        headers[*num_headers].value = value;
        headers[*num_headers].value_len = value_end - value;
    }
    return buf;
nodata:
    *ret = -2;
    return NULL;
error:
    *ret = -1;
    return NULL;
}

static const char *parse_request(http_parse_request_t *request,
                                 const char *buf, const char *buf_end,
                                 size_t max_headers, int *ret)
{
    /* skip first empty line (some clients add CRLF after POST content) */
    if (buf == buf_end) goto nodata;
    if (*buf == '\015') {
        ++buf;
        if (buf == buf_end) goto nodata;
        if (*buf++ != '\012') goto error;
    } else if (*buf == '\012') {
        ++buf;
    }

    /* parse request line */
    buf = parse_token(buf, buf_end, &request->method, &request->method_len, ' ', ret);
    if (buf == NULL)
        return NULL;

    request->http_method = parse_http_method(request->method, request->method_len);

    do {
        ++buf;
        if (buf == buf_end) goto nodata;
    } while (*buf == ' ');

    const char *tok_start = buf;
    static const char ALIGNED(16) ranges[16] = "\000\040\177\177";

    int found;
    buf = findchar_fast(buf, buf_end, ranges, 4, &found);
    if (!found) {
        if (buf == buf_end) goto nodata;
    }

    while (1) {
        if (*buf == ' ') {
            break;
        } else if (unlikely(!IS_PRINTABLE_ASCII(*buf))) {
            if ((unsigned char)*buf < '\040' || *buf == '\177')
                goto error;
        }
        ++buf;
        if (buf == buf_end) goto nodata;
    }
    request->path = tok_start;
    request->path_len = buf - tok_start;

    do {
        ++buf;
        if (buf == buf_end) goto nodata;
    } while (*buf == ' ');

    if ((request->method_len == 0) || (request->path_len == 0))
        goto error;

    buf = parse_http_version(buf, buf_end, &request->http_version, &request->minor_version, ret);
    if (buf == NULL)
        return NULL;

    if (*buf == '\015') {
        ++buf;
        if (buf == buf_end) goto nodata;
        if (*buf++ != '\012') goto error;
    } else if (*buf == '\012') {
        ++buf;
    } else {
        *ret = -1;
        return NULL;
    }

    buf = parse_headers(buf, buf_end, request->headers, &request->num_headers, max_headers, ret);
    return buf;
nodata:
     *ret = -2;
     return NULL;
error:
     *ret = -1;
     return NULL;
}


/* returns number of bytes consumed if successful, -2 if request is partial,
 * -1 if failed */
int http_parse_request(http_parse_request_t *request,
                       const char *buf_start, size_t len, size_t last_len)
{
    const char *buf = buf_start, *buf_end = buf_start + len;
    int r;

    request->http_method = HTTP_METHOD_UNKNOW;
    request->method = NULL;
    request->method_offset = 0;
    request->method_len = 0;
    request->path = NULL;
    request->path_offset = 0;
    request->path_len = 0;
    request->http_version = HTTP_VERSION_UNKNOW;
    request->minor_version = -1;
    size_t max_headers = request->num_headers;
    request->num_headers = 0;

    /* if last_len != 0, check if the request is complete (a fast countermeasure
       againt slowloris */
    if (last_len != 0 && is_complete(buf, buf_end, last_len, &r) == NULL) {
        return r;
    }

    buf = parse_request(request, buf, buf_end, max_headers, &r);
    if (buf == NULL)
        return r;

    if (request->method != NULL)
        request->method_offset = request->method - buf_start;

    if (request->path != NULL)
        request->path_offset = request->path - buf_start;

    for (size_t i = 0; i < request->num_headers; i++) {
        http_parse_header_t *header = &request->headers[i];
        if (header->name != NULL)
            header->name_offset = header->name - buf_start;
        if (header->value != NULL)
            header->value_offset = header->value - buf_start;
    }

    return (int)(buf - buf_start);
}

static const char *parse_response(http_parse_response_t *response,
                                  const char *buf, const char *buf_end,
                                  size_t max_headers, int *ret)
{
    /* parse "HTTP/1.x" */
    buf = parse_http_version(buf, buf_end, &response->http_version, &response->minor_version, ret);
    if (buf == NULL)
        return NULL;

    /* skip space */
    if (*buf != ' ') goto error;

    do {
        ++buf;
        if (buf == buf_end) goto nodata;
    } while (*buf == ' ');

    /* parse status code, we want at least [:digit:][:digit:][:digit:]<other char> to try to parse */
    if (buf_end - buf < 4) goto nodata;

    if ((buf[0] < '0') || (buf[0] > '9')) goto error;
    if ((buf[1] < '0') || (buf[1] > '9')) goto error;
    if ((buf[2] < '0') || (buf[2] > '9')) goto error;
    response->status = buf[0] -'0';

    switch(response->status) {
    case '1':
        response->status_class = HTTP_STATUS_1xx;
        break;
    case '2':
        response->status_class = HTTP_STATUS_2xx;
        break;
    case '3':
        response->status_class = HTTP_STATUS_3xx;
        break;
    case '4':
        response->status_class = HTTP_STATUS_4xx;
        break;
    case '5':
        response->status_class = HTTP_STATUS_5xx;
        break;
    default:
        response->status_class = HTTP_STATUS_UNKNOW;
        break;
    }

    response->status = response->status*100 + (buf[1] -'0')*10 + (buf[2] -'0');

    buf += 3;

    /* get message including preceding space */
    buf = get_token_to_eol(buf, buf_end, &response->msg, &response->msg_len, ret);
    if (buf == NULL)
        return NULL;

    if (response->msg_len == 0) {
        /* ok */
    } else if (*response->msg == ' ') {
        /* Remove preceding space. Successful return from `get_token_to_eol`
         * guarantees that we would hit something other than SP
         * before running past the end of the given buffer. */
        do {
            ++response->msg;
            --response->msg_len;
        } while (*response->msg == ' ');
    } else {
        /* garbage found after status code */
        goto error;
    }

    return parse_headers(buf, buf_end, response->headers, &response->num_headers, max_headers, ret);
nodata:
     *ret = -2;
     return NULL;
error:
    *ret = -1;
    return NULL;
}

int http_parse_response(http_parse_response_t *response,
                        const char *buf_start, size_t len, size_t last_len)
{
    const char *buf = buf_start, *buf_end = buf + len;
    int r;

    response->http_version = HTTP_VERSION_UNKNOW;
    response->minor_version = -1;
    response->status_class = HTTP_STATUS_UNKNOW;
    response->status = 0;
    response->msg = NULL;
    response->msg_len = 0;
    size_t max_headers = response->num_headers;
    response->num_headers = 0;

    /* if last_len != 0, check if the response is complete (a fast countermeasure
       against slowloris */
    if (last_len != 0 && is_complete(buf, buf_end, last_len, &r) == NULL) {
        return r;
    }

    buf = parse_response(response, buf, buf_end, max_headers, &r);
    if (buf == NULL)
        return r;

    return (int)(buf - buf_start);
}

int http_parse_headers(const char *buf_start, size_t len,
                       http_parse_header_t *headers, size_t *num_headers, size_t last_len)
{
    const char *buf = buf_start, *buf_end = buf + len;
    int r;

    *num_headers = 0;

    /* if last_len != 0, check if the response is complete (a fast countermeasure
       against slowloris */
    if (last_len != 0 && is_complete(buf, buf_end, last_len, &r) == NULL) {
        return r;
    }

    size_t max_headers = *num_headers;
    buf = parse_headers(buf, buf_end, headers, num_headers, max_headers, &r);
    if (buf == NULL)
        return r;

    return (int)(buf - buf_start);
}

enum {
    CHUNKED_IN_CHUNK_SIZE,
    CHUNKED_IN_CHUNK_EXT,
    CHUNKED_IN_CHUNK_DATA,
    CHUNKED_IN_CHUNK_CRLF,
    CHUNKED_IN_TRAILERS_LINE_HEAD,
    CHUNKED_IN_TRAILERS_LINE_MIDDLE
};

static int decode_hex(int ch)
{
    if ('0' <= ch && ch <= '9') {
        return ch - '0';
    } else if ('A' <= ch && ch <= 'F') {
        return ch - 'A' + 0xa;
    } else if ('a' <= ch && ch <= 'f') {
        return ch - 'a' + 0xa;
    } else {
        return -1;
    }
}

ssize_t http_decode_chunked(struct http_chunked_decoder *decoder, char *buf, size_t *_bufsz)
{
    size_t dst = 0, src = 0, bufsz = *_bufsz;
    ssize_t ret = -2; /* incomplete */

    while (true) {
        switch (decoder->_state) {
        case CHUNKED_IN_CHUNK_SIZE:
            for (;; ++src) {
                int v;
                if (src == bufsz)
                    goto Exit;
                if ((v = decode_hex(buf[src])) == -1) {
                    if (decoder->_hex_count == 0) {
                        ret = -1;
                        goto Exit;
                    }
                    break;
                }
                if (decoder->_hex_count == sizeof(size_t) * 2) {
                    ret = -1;
                    goto Exit;
                }
                decoder->bytes_left_in_chunk = decoder->bytes_left_in_chunk * 16 + v;
                ++decoder->_hex_count;
            }
            decoder->_hex_count = 0;
            decoder->_state = CHUNKED_IN_CHUNK_EXT;
        /* fallthru */
        case CHUNKED_IN_CHUNK_EXT:
            /* RFC 7230 A.2 "Line folding in chunk extensions is disallowed" */
            for (;; ++src) {
                if (src == bufsz)
                    goto Exit;
                if (buf[src] == '\012')
                    break;
            }
            ++src;
            if (decoder->bytes_left_in_chunk == 0) {
                if (decoder->consume_trailer) {
                    decoder->_state = CHUNKED_IN_TRAILERS_LINE_HEAD;
                    break;
                } else {
                    goto Complete;
                }
            }
            decoder->_state = CHUNKED_IN_CHUNK_DATA;
        /* fallthru */
        case CHUNKED_IN_CHUNK_DATA: {
            size_t avail = bufsz - src;
            if (avail < decoder->bytes_left_in_chunk) {
                if (dst != src)
                    memmove(buf + dst, buf + src, avail);
                src += avail;
                dst += avail;
                decoder->bytes_left_in_chunk -= avail;
                goto Exit;
            }
            if (dst != src)
                memmove(buf + dst, buf + src, decoder->bytes_left_in_chunk);
            src += decoder->bytes_left_in_chunk;
            dst += decoder->bytes_left_in_chunk;
            decoder->bytes_left_in_chunk = 0;
            decoder->_state = CHUNKED_IN_CHUNK_CRLF;
        }
        /* fallthru */
        case CHUNKED_IN_CHUNK_CRLF:
            for (;; ++src) {
                if (src == bufsz)
                    goto Exit;
                if (buf[src] != '\015')
                    break;
            }
            if (buf[src] != '\012') {
                ret = -1;
                goto Exit;
            }
            ++src;
            decoder->_state = CHUNKED_IN_CHUNK_SIZE;
            break;
        case CHUNKED_IN_TRAILERS_LINE_HEAD:
            for (;; ++src) {
                if (src == bufsz)
                    goto Exit;
                if (buf[src] != '\015')
                    break;
            }
            if (buf[src++] == '\012')
                goto Complete;
            decoder->_state = CHUNKED_IN_TRAILERS_LINE_MIDDLE;
        /* fallthru */
        case CHUNKED_IN_TRAILERS_LINE_MIDDLE:
            for (;; ++src) {
                if (src == bufsz)
                    goto Exit;
                if (buf[src] == '\012')
                    break;
            }
            ++src;
            decoder->_state = CHUNKED_IN_TRAILERS_LINE_HEAD;
            break;
        default:
            assert(!"decoder is corrupt");
        }
    }

Complete:
    ret = bufsz - src;
Exit:
    if (dst != src)
        memmove(buf + dst, buf + src, bufsz - src);
    *_bufsz = dst;
    return ret;
}

int http_decode_chunked_is_in_data(struct http_chunked_decoder *decoder)
{
    return decoder->_state == CHUNKED_IN_CHUNK_DATA;
}
