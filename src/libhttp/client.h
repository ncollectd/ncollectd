/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once

typedef struct {
    http_proto_t proto;
    union {
        struct {
            char *host;
            uint16_t port;
        };
        char *sun_path
    };
    http_method_t method;
    char *path,
    http_version_t version;
    http_header_list_t headers;
} http_client_request_t;

typedef struct {
    int status_code;
    http_status_class_t status_class;
    http_header_list_t headers;

} http_client_response_t;



typedef struct {
    http_header_name_t name;
    char *value;
} http_header_t;

typedef struct {
    size_t num;
    http_header_t *headers;
} http_header_list_t;




http_header_t headers[] = {
    { HTTP_HEADER_HOST,   "localhost"            },
    { HTTP_HEADER_ACCEPT, "application/protobuf" }
};

http_client_request_t request = {
    .proto       = HTTP_PROTO_UNIX,
    .sun_path    = "/var/run/ncollectd-socket",
    .method      = HTTP_METHOD_GET,
    .path        = "/",
    .version     = HTTP_VERSION_1_1,
    .headers.ptr = headers,
    .headers.num = STATIC_ARRAY_SIZE(headers),
};

int http_client_connect(

int http_client_request(http_client_request_t *request, char *buf, size_t buf_size,
                        http_client_response_t *response);

