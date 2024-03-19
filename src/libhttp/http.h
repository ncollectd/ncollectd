/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once

#include <stddef.h>

#include "libhttp/header.h"

typedef enum {
    HTTP_STATUS_100,
    HTTP_STATUS_101,
    HTTP_STATUS_103,

    HTTP_STATUS_200,
    HTTP_STATUS_201,
    HTTP_STATUS_202,
    HTTP_STATUS_203,
    HTTP_STATUS_204,
    HTTP_STATUS_205,
    HTTP_STATUS_206,

    HTTP_STATUS_300,
    HTTP_STATUS_301,
    HTTP_STATUS_302,
    HTTP_STATUS_303,
    HTTP_STATUS_304,
    HTTP_STATUS_305,
    HTTP_STATUS_307,
    HTTP_STATUS_308,

    HTTP_STATUS_400,
    HTTP_STATUS_401,
    HTTP_STATUS_402,
    HTTP_STATUS_403,
    HTTP_STATUS_404,
    HTTP_STATUS_405,
    HTTP_STATUS_406,
    HTTP_STATUS_407,
    HTTP_STATUS_408,
    HTTP_STATUS_409,
    HTTP_STATUS_410,
    HTTP_STATUS_411,
    HTTP_STATUS_412,
    HTTP_STATUS_413,
    HTTP_STATUS_414,
    HTTP_STATUS_415,
    HTTP_STATUS_416,
    HTTP_STATUS_417,
    HTTP_STATUS_421,
    HTTP_STATUS_425,
    HTTP_STATUS_426,
    HTTP_STATUS_428,
    HTTP_STATUS_429,
    HTTP_STATUS_431,
    HTTP_STATUS_451,

    HTTP_STATUS_500,
    HTTP_STATUS_501,
    HTTP_STATUS_502,
    HTTP_STATUS_503,
    HTTP_STATUS_504,
    HTTP_STATUS_505,
    HTTP_STATUS_506,
    HTTP_STATUS_510,
    HTTP_STATUS_511
} http_status_code_t;

typedef enum {
    HTTP_STATUS_UNKNOW,
    HTTP_STATUS_1xx,
    HTTP_STATUS_2xx,
    HTTP_STATUS_3xx,
    HTTP_STATUS_4xx,
    HTTP_STATUS_5xx
} http_status_class_t;

typedef enum {
    HTTP_METHOD_UNKNOW,
    HTTP_METHOD_GET,
    HTTP_METHOD_PUT,
    HTTP_METHOD_POST,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_PATCH,
    HTTP_METHOD_TRACE,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_OPTIONS,
    HTTP_METHOD_CONNECT
} http_method_t;

typedef enum {
    HTTP_VERSION_UNKNOW,
    HTTP_VERSION_1_0,
    HTTP_VERSION_1_1
} http_version_t;

typedef enum {
    HTTP_PROTO_UNIX,
    HTTP_PROTO_HTTP,
    HTTP_PROTO_HTTPS
} http_proto_t;

typedef struct {
    http_method_t method;
    char *path;
    http_version_t version;
    http_header_set_t headers;
    size_t content_length;
    void *content;
} http_request_t;

typedef struct {
    http_version_t version;
    http_status_code_t status_code;
    http_status_class_t status_class;
    http_header_set_t headers;
    size_t content_length;
    char *content;
} http_response_t;

const char *http_get_status_reason(http_status_code_t status_code);

int http_get_status(http_status_code_t status_code);

http_status_code_t http_get_status_code(int status_code);

const char *http_get_method(http_method_t method);

const char *http_get_version(http_version_t version);

void http_response_reset(http_response_t *response);

int http_write_request(int fd, http_request_t *request, int *timeout);

int http_read_response(int fd, http_response_t *response, int *timeout);

int http_fetch(int fd, http_request_t *request, http_response_t *response, int *timeout);
