// SPDX-License-Identifier: GPL-2.0-only

#include <stdlib.h>
#include <string.h>
#include <stdlib.h>

#include "libutils/common.h"
#include "libhttp/http.h"
#include "libhttp/parser.h"
#include "libutils/buf.h"

#define STATIC_ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

void http_response_reset(http_response_t *response)
{
    if (response == NULL)
        return;

    http_header_reset(&(response->headers));
}

ssize_t http_write(int fd, const void *buf, size_t count, int *timeout)
{
    if (*timeout > 0) {

    }
    return write(fd, buf, count);
}

ssize_t http_read(int fd, void *buf, size_t count, int *timeout)
{
    if (*timeout > 0) {

    }
        //while ((rret = read(sock, buf + buflen, sizeof(buf) - buflen)) == -1 && errno == EINTR)
    return read(fd, buf, count);
}

int http_write_request(int fd, http_request_t *request, int *timeout)
{
    uint8_t buffer[4096];

    const char *smethod = http_get_method(request->method);
    if (smethod == NULL)
        return -1;

    const char *sversion = http_get_version(request->version);
    if (sversion == NULL)
       return -1;

    int status = 0;
    buf_t buf = BUF_CREATE_STATIC(buffer);

    status |= buf_put(&buf, smethod, strlen(smethod));
    status |= buf_putchar(&buf, ' ');
    status |= buf_put(&buf, request->path, strlen(request->path));
    status |= buf_putchar(&buf, ' ');
    status |= buf_put(&buf, sversion, strlen(sversion));
    status |= buf_put(&buf, "\r\n", 2);

    for (size_t i=0; i < request->headers.num; i++) {
        const char *header = http_get_header(request->headers.ptr[i].header_name);
        if (header != NULL) {
            status |= buf_put(&buf, header, strlen(header));
            status |= buf_put(&buf, ": ", 2);
            status |= buf_put(&buf, request->headers.ptr[i].value,
                                    strlen(request->headers.ptr[i].value));
            status |= buf_put(&buf, "\r\n", 2);
        }
    }

    const char *header = http_get_header(HTTP_HEADER_CONTENT_LENGTH);
    if (header != NULL) {
        status |= buf_put(&buf, header, strlen(header));
        status |= buf_put(&buf, ": ", 2);
        status |= buf_putitoa(&buf, request->content_length);
        status |= buf_put(&buf, "\r\n", 2);
    }

    status |= buf_put(&buf, "\r\n", 2);
    if (status != 0)
        return -1;

    status = http_write(fd, buf.ptr, buf_len(&buf), timeout);
    if (status <= 0)
        return -1;

    if ((request->content != NULL) && (request->content_length > 0)) {
        status = http_write(fd, request->content, request->content_length, timeout);
        if (status <= 0)
            return -1;
    }

    return 0;
}

int http_read_response(int fd, http_response_t *response, int *timeout)
{
    http_parse_header_t parse_headers[32] = {0};
    http_parse_response_t parse_response = {0};

    parse_response.headers = parse_headers;
    parse_response.num_headers = STATIC_ARRAY_SIZE(parse_headers);

    char buf[4096];
    size_t buf_len = 0;
    size_t prev_buf_len = 0;
    size_t psize = 0;
    size_t rsize = 0;
    while (true) {
        /* read the request */
        ssize_t rret = http_read(fd, buf + buf_len,  sizeof(buf) - buf_len, timeout);
        if (rret <= 0)
            return -1; // IO_ERROR
        rsize += rret;
        prev_buf_len = buf_len;
        buf_len += rret;

        int status = http_parse_response(&parse_response, buf, buf_len, prev_buf_len);
        if (status > 0) {
            psize = status;
            break; /* successfully parsed the request */
        } else if (status == -1) {
            return -1; // PARSER_ERROR
        }

        /* request is incomplete, continue the loop */
        assert(status == -2);
        if (buf_len == sizeof(buf))
            return -2; // RequestIsTooLongError;
    }

    response->version = parse_response.http_version;
    response->status_code = http_get_status_code(parse_response.status);
    response->status_class = parse_response.status_class;

    size_t content_length = 0;

    if (parse_response.num_headers > 0) {
        for (size_t i = 0; i < parse_response.num_headers ; ++i) {
            http_header_append(&response->headers, parse_response.headers[i].header,
                                                   NULL, 0,
                                                   parse_response.headers[i].value,
                                                   parse_response.headers[i].value_len);
            if (parse_response.headers[i].header == HTTP_HEADER_CONTENT_LENGTH) {
                char cbuf[24];
                size_t len = sizeof(cbuf)-1;
                if (parse_response.headers[i].value_len < len)
                    len = parse_response.headers[i].value_len;
                memcpy(cbuf, parse_response.headers[i].value, len);
                cbuf[len] = '\0';
                content_length = atoi(cbuf);
            }
        }
    }

    if (content_length > 0) {
        response->content = malloc(content_length);
        if (response->content == NULL) {
            // FIXME
            return -1;
        }
        size_t buf_remain = rsize - psize;
        if (buf_remain > content_length) {
            // FIXME
            return -1;
        }

        memcpy(response->content, buf + psize, buf_remain);

        size_t remain = content_length - buf_remain;
        size_t content_pos = buf_remain;
        while (remain > 0) {
            ssize_t rrsize = http_read(fd, response->content + content_pos, remain, timeout);
            if (rrsize  < 0) {
                return -1; // IO_ERROR
            } else if (rrsize  == 0) {
                return -1; // IO_ERROR
            }
            remain -= rrsize;
            content_pos += rrsize;
        }
        response->content_length = content_length;
    }


    return 0;
}

int http_fetch(int fd, http_request_t *request, http_response_t *response, int *timeout)
{
    int status = http_write_request(fd, request, timeout);
    if (status != 0) {
        return -1;
    }

    return http_read_response(fd, response, timeout);
}
