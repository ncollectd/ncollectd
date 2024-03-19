/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once

#include "libhttp/http.h"

struct httpd_listen_s;
typedef struct httpd_listen_s httpd_listen_t;

struct httpd_s;
typedef struct httpd_s httpd_t;

struct httpd_client_s;
typedef struct httpd_client_s httpd_client_t;

httpd_listen_t *httpd_listen_init(void);

void httpd_listen_free(httpd_listen_t *httpd_listen);

httpd_t *httpd_init(httpd_listen_t *httpd_listen, int max, int timeout);

int httpd_open_socket(httpd_listen_t *httpd_listen, const char *node, const char *service, int backlog);

int httpd_open_unix_socket(httpd_listen_t *httpd_listen, char const *file, int backlog,
                                                         char const *group, int perms, bool delete);

typedef int (* httpd_request_t)(httpd_client_t *client,
                                http_version_t http_version, http_method_t http_method,
                                const char *path, size_t path_len, http_header_set_t *headers,
                                void *content, size_t content_length);

int httpd_loop(httpd_t *httpd, httpd_request_t request_cb);

void httpd_stop(httpd_t *httpd);

int httpd_response(httpd_client_t *client, http_version_t version, http_status_code_t status_code,
                   http_header_set_t *headers, void *content, size_t content_length);

void httpd_free(httpd_t *httpd);
