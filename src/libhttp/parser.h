/* SPDX-License-Identifier: GPL-2.0-only OR MIT                     */
/* SPDX-FileCopyrightText: Copyright (c) 2009-2014 Kazuho Oku       */
/* SPDX-FileCopyrightText: Copyright (c) 2009-2014 Tokuhiro Matsuno */
/* SPDX-FileCopyrightText: Copyright (c) 2009-2014 Daisuke Murase   */
/* SPDX-FileCopyrightText: Copyright (c) 2009-2014 Shigeo Mitsunari */
/* SPDX-FileContributor: Kazuho Oku                                 */
/* SPDX-FileContributor: Tokuhiro Matsuno                           */
/* SPDX-FileContributor: Daisuke Murase                             */
/* SPDX-FileContributor: Shigeo Mitsunari                           */
#pragma once

#include <sys/types.h>

#include "libhttp/header.h"
#include "libhttp/http.h"

/* contains name and value of a header (name == NULL if is a continuing line
 * of a multiline header */

typedef struct {
    http_header_name_t header;
    const char *name;
    size_t name_offset;
    size_t name_len;
    const char *value;
    size_t value_offset;
    size_t value_len;
} http_parse_header_t;

typedef struct {
    http_method_t http_method;
    const char *method;
    size_t method_offset;
    size_t method_len;
    const char *path;
    size_t path_offset;
    size_t path_len;
    http_version_t http_version;
    int minor_version;
    http_parse_header_t *headers;
    size_t num_headers;
} http_parse_request_t;

/* returns number of bytes consumed if successful, -2 if request is partial,
 * -1 if failed */
int http_parse_request(http_parse_request_t *request,
                       const char *buf, size_t len, size_t last_len);

typedef struct {
    http_version_t http_version;
    int minor_version;
    http_status_class_t status_class;
    int status;
    const char *msg;
    size_t msg_len;
    http_parse_header_t *headers;
    size_t num_headers;
} http_parse_response_t;

/* ditto */
int http_parse_response(http_parse_response_t *response,
                        const char *_buf, size_t len, size_t last_len);


/* ditto */
int http_parse_headers(const char *buf, size_t len,
                       http_parse_header_t *headers, size_t *num_headers, size_t last_len);

/* should be zero-filled before start */
struct http_chunked_decoder {
    size_t bytes_left_in_chunk; /* number of bytes left in current chunk */
    char consume_trailer;       /* if trailing headers should be consumed */
    char _hex_count;
    char _state;
};

/* the function rewrites the buffer given as (buf, bufsz) removing the chunked-
 * encoding headers.  When the function returns without an error, bufsz is
 * updated to the length of the decoded data available.  Applications should
 * repeatedly call the function while it returns -2 (incomplete) every time
 * supplying newly arrived data.  If the end of the chunked-encoded data is
 * found, the function returns a non-negative number indicating the number of
 * octets left undecoded, that starts from the offset returned by `*bufsz`.
 * Returns -1 on error.
 */
ssize_t http_decode_chunked(struct http_chunked_decoder *decoder, char *buf, size_t *bufsz);

/* returns if the chunked decoder is in middle of chunked data */
int http_decode_chunked_is_in_data(struct http_chunked_decoder *decoder);
