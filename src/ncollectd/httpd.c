// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "plugin_internal.h"
#include "globals.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"
#include "libutils/strlist.h"
#include "libhttp/httpd.h"
#include "libxson/render.h"
#include "libmdb/mdb.h"
#include "libformat/notification_json.h"
#include "libmetric/parser.h"


#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX sizeof(((struct sockaddr_un *)0)->sun_path)
#endif

static httpd_listen_t *httpd_listen;
static httpd_t *httpd;

static int httpd_in_loop;
static char const *httpd_sock_file;
static pthread_t httpd_listen_thread = (pthread_t)0;

extern mdb_t *mdb;

typedef enum {
    CONTENT_TYPE_JSON,
    CONTENT_TYPE_TEXT,
    CONTENT_TYPE_PROTOB
} content_type_t;

typedef enum {
    ACCEPT_JSON,
    ACCEPT_TEXT,
    ACCEPT_PROTOB
} http_accept_t;

static int handle_family_metrics(httpd_client_t *client, http_version_t http_version, strbuf_t *buf)
{
    mdb_family_metric_list_t *faml = mdb_get_metric_family(mdb);
    if (faml == NULL) {
        httpd_response(client, http_version, HTTP_STATUS_500, NULL, NULL, 0);
        return 0;
    }

    int status = mdb_family_metric_list_to_json(faml, buf, false);
    if (status != 0) {
        mdb_family_metric_list_free(faml);
        httpd_response(client, http_version, HTTP_STATUS_500, NULL, NULL, 0);
        return 0;
    }

    mdb_family_metric_list_free(faml);

    http_header_t headers[] = {
        { .header_name = HTTP_HEADER_CONTENT_TYPE, .value = "application/json" }
    };
    http_header_set_t header_set = {.num = STATIC_ARRAY_SIZE(headers), .ptr = headers };
    httpd_response(client, http_version, HTTP_STATUS_200, &header_set, buf->ptr, strbuf_len(buf));

    return 0;
}

static int handle_series(httpd_client_t *client, http_version_t http_version, strbuf_t *buf)
{
    mdb_series_list_t *list = mdb_get_series(mdb);
    if (list == NULL) {
        httpd_response(client, http_version, HTTP_STATUS_500, NULL, NULL, 0);
        return 0;
    }

    int status = mdb_series_list_to_json(list, buf, false);
    if (status != 0) {
        mdb_series_list_free(list);
        httpd_response(client, http_version, HTTP_STATUS_500, NULL, NULL, 0);
        return 0;
    }

    mdb_series_list_free(list);

    http_header_t headers[] = {
        { .header_name = HTTP_HEADER_CONTENT_TYPE, .value = "application/json" }
    };
    http_header_set_t header_set = {.num = STATIC_ARRAY_SIZE(headers), .ptr = headers };
    httpd_response(client, http_version, HTTP_STATUS_200, &header_set, buf->ptr, strbuf_len(buf));

    return 0;
}

static int handle_strlist(httpd_client_t *client, http_version_t http_version,
                          strbuf_t *buf, strlist_t *sl)
{
    int status = mdb_strlist_to_json(sl, buf, false);
    if (status != 0) {
        strlist_free(sl);
        httpd_response(client, http_version, HTTP_STATUS_500, NULL, NULL, 0);
        return 0;
    }

    strlist_free(sl);

    http_header_t rheaders[] = {
        { .header_name = HTTP_HEADER_CONTENT_TYPE, .value = "application/json" }
    };
    http_header_set_t rheader_set = {.num = STATIC_ARRAY_SIZE(rheaders), .ptr = rheaders };
    httpd_response(client, http_version, HTTP_STATUS_200, &rheader_set, buf->ptr, strbuf_len(buf));
    return 0;
}

typedef struct {
    char *name;
    char *value;
} http_query_t;

int http_query_split(__attribute__((unused)) char *string,
                     __attribute__((unused)) http_query_t *fields,
                     __attribute__((unused)) size_t size)
{

    return 0;
}

int http_path_split(char *string, char **fields, size_t size)
{
    size_t i = 0;
    char *ptr = string;
    char *saveptr = NULL;
    while ((fields[i] = strtok_r(ptr, "/", &saveptr)) != NULL) {
        ptr = NULL;
        i++;

        if (i >= size)
            break;
    }

    return (int)i;
}


static int httpd_request(httpd_client_t *client,
                         http_version_t http_version, http_method_t http_method,
                         const char *path, size_t path_len, http_header_set_t *headers,
                         void *content, size_t content_length)
{
#if 0
    http_accept_t accept = ACCEPT_JSON;
#endif
    if (headers != NULL) {
        for(size_t i = 0; i < headers->num; i++) {
            if (headers->ptr[i].header_name == HTTP_HEADER_ACCEPT) {
                if (strstr(headers->ptr[i].value, "application/x-www-form-urlencoded") != NULL) {

                } else if (strstr(headers->ptr[i].value, "application/json") != NULL) {

                }
                break;
            }
        }
    }

#if 0
    char *query = strchr(path, '?');
    if (query != NULL) {
        *query = '\0';
        query++;
    }
#endif
    char *xpath = sstrndup(path, path_len);
fprintf(stderr, "httpd_request : %s\n", xpath);
    char *pfields[8];
    int npfields = http_path_split(xpath, pfields, STATIC_ARRAY_SIZE(pfields));
fprintf(stderr, "httpd_request : %d\n", npfields);

    strbuf_t buf = STRBUF_CREATE;

    if (npfields < 3)
        goto error_404;
    if (strcmp(pfields[0], "api") != 0)
        goto error_404;
    if (strcmp(pfields[1], "v1") != 0)
        goto error_404;

    size_t plen = strlen(pfields[2]);
fprintf(stderr, "httpd_request : [%s] %d\n", pfields[2], (int)plen);
    int status = 0;

    switch(plen) {
    case 4:
        if (strcmp(pfields[2], "read") == 0) {

        }
        break;
    case 5:
        if (strcmp(pfields[2], "write") == 0) {
            if (http_method != HTTP_METHOD_POST)
                goto error_501;

            metric_parser_t *mp = metric_parser_alloc(NULL, NULL);
            if (mp == NULL)
                goto error_501;

            plugin_ctx_t ctx = { .interval = interval_g };
            plugin_ctx_t old_ctx = plugin_set_ctx(ctx);

            size_t buffer_len = content_length;
            char *buffer = content;
            cdtime_t time = cdtime();
fprintf(stderr, "httpd_request:  write: %d\n",(int)content_length);
            size_t lineno = 0;


            while (buffer_len > 0) {
                char *end = memchr(buffer, '\n', buffer_len);
                if (end == NULL)
                    break;

                size_t line_size = end - buffer;
                char *line = buffer;
                buffer[line_size] = '\0';
                lineno += 1;
                if (line_size > 0) {
                    status = metric_parse_line(mp, line);
                    if (status < 0)
                        break;
                }

                buffer_len -= line_size; // FIXME +1
                buffer = end + 1;              // +1
            }

            metric_parser_dispatch(mp, plugin_dispatch_metric_family_filtered, NULL, time);

            metric_parser_free(mp);

            plugin_set_ctx(old_ctx);

            status = httpd_response(client, http_version, HTTP_STATUS_200, NULL, NULL, 0);
            strbuf_destroy(&buf);
            free(xpath);
            return status;
        } else if (strcmp(pfields[2], "query") == 0) {

        }
        break;
    case 6:
        if (strcmp(pfields[2], "series") == 0) {
            if (http_method != HTTP_METHOD_GET)
                goto error_501;
            status = handle_series(client, http_version, &buf);
            strbuf_destroy(&buf);
            free(xpath);
            return status;
        } else if (strcmp(pfields[2], "metric") == 0) {
            if (npfields == 5) {
                if (strcmp(pfields[4], "labels") == 0) {
                    if (http_method != HTTP_METHOD_GET)
                        goto error_501;
#if 0
                    if (accept != ACCEPT_JSON)
                        goto error_415;
#endif
                    strlist_t *list = mdb_get_metric_label(mdb, pfields[3]);
                    if (list == NULL)
                        goto error_500;

                    status = handle_strlist(client, http_version, &buf, list);
                    strbuf_destroy(&buf);
                    free(xpath);
                    return status;
                }
            } else if (npfields == 6) {
                if (strcmp(pfields[4], "label") == 0) {
                    if (http_method != HTTP_METHOD_GET)
                        goto error_501;
#if 0
                    if (accept != ACCEPT_JSON)
                        goto error_415;
#endif
                    strlist_t *list = mdb_get_metric_label_value(mdb, pfields[3], pfields[5]);
                    if (list == NULL)
                        goto error_500;

                    status = handle_strlist(client, http_version, &buf, list);
                    strbuf_destroy(&buf);
                    free(xpath);
                    return status;
                }
            }
        }
        break;
    case 7:
        if (strcmp(pfields[2], "readers") == 0) {
            if (http_method != HTTP_METHOD_GET)
                goto error_501;

            strlist_t *sl = plugin_get_readers();
            if (sl == NULL)
                goto error_500;

            status = handle_strlist(client, http_version, &buf, sl);
            free(xpath);
            strbuf_destroy(&buf);
            return status;
        } else if (strcmp(pfields[2], "writers") == 0) {
            if (http_method != HTTP_METHOD_GET)
                goto error_501;

            strlist_t *sl = plugin_get_writers();
            if (sl == NULL)
                goto error_500;

            status = handle_strlist(client, http_version, &buf, sl);
            free(xpath);
            strbuf_destroy(&buf);
            return status;
        } else if (strcmp(pfields[2], "loggers") == 0) {
            if (http_method != HTTP_METHOD_GET)
                goto error_501;

            strlist_t *sl = plugin_get_loggers();
            if (sl == NULL)
                goto error_500;

            status = handle_strlist(client, http_version, &buf, sl);
            free(xpath);
            strbuf_destroy(&buf);
            return status;
        } else if (strcmp(pfields[2], "metrics") == 0) {
            if (http_method != HTTP_METHOD_GET)
                goto error_501;
#if 0
            if (accept != ACCEPT_JSON)
                goto error_415;
#endif
            strlist_t *list = mdb_get_metrics(mdb);
            if (list == NULL)
                goto error_500;

            status = handle_strlist(client, http_version, &buf, list);
            free(xpath);
            strbuf_destroy(&buf);
            return status;
        }
        break;
    case 11:
        if (strcmp(pfields[2], "query_range") == 0) {

        }
        break;
    case 12:
        if (strcmp(pfields[2], "notification") == 0) {
            if (http_method != HTTP_METHOD_POST)
                goto error_501;

            notification_t *notif = notification_json_parse(content, content_length);
            if (notif == NULL)
                goto error_500;

            plugin_ctx_t ctx = { .interval = interval_g };
            plugin_ctx_t old_ctx = plugin_set_ctx(ctx);

            status = plugin_dispatch_notification(notif);
            notification_free(notif);
            plugin_set_ctx(old_ctx);

            if (status != 0)
                goto error_500;

            status = httpd_response(client, http_version, HTTP_STATUS_200, NULL, NULL, 0);
            free(xpath);
            strbuf_destroy(&buf);
            return status;
        } else if (strcmp(pfields[2], "notificators") == 0) {
            if (http_method != HTTP_METHOD_GET)
                goto error_501;

            strlist_t *sl = plugin_get_notificators();
            if (sl == NULL)
                goto error_500;

            status = handle_strlist(client, http_version, &buf, sl);
            free(xpath);
            strbuf_destroy(&buf);
            return status;
        }
        break;
    case 14:
        if (strcmp(pfields[2], "family_metrics") == 0) {
            if (http_method != HTTP_METHOD_GET)
                goto error_501;

            status = handle_family_metrics(client, http_version, &buf);
            free(xpath);
            strbuf_destroy(&buf);
            return status;
        }
        break;
    }

error_404:
    free(xpath);
    strbuf_destroy(&buf);
    httpd_response(client, http_version, HTTP_STATUS_404, NULL, NULL, 0);
    return 0;
#if 0
error_415:
    free(xpath);
    strbuf_destroy(&buf);
    httpd_response(client, http_version, HTTP_STATUS_415, NULL, NULL, 0);
    return 0;
#endif
error_500:
    free(xpath);
    strbuf_destroy(&buf);
    httpd_response(client, http_version, HTTP_STATUS_500, NULL, NULL, 0);
    return 0;
error_501:
    free(xpath);
    strbuf_destroy(&buf);
    httpd_response(client, http_version, HTTP_STATUS_501, NULL, NULL, 0);
    return 0;
}

static void *httpd_server(__attribute__((unused)) void *arg)
{
    int status = 0;

    while (httpd_in_loop != 0) {
        httpd_loop(httpd, httpd_request);
    }

    status = unlink(httpd_sock_file);
    if (status != 0)
        NOTICE("unlink (%s) failed: %s", httpd_sock_file, STRERRNO);

    return (void *)0;
}

#define HTTP_MAX_CONNECTIONS 256

int http_server_init(void)
{
    httpd_sock_file = global_option_get("socket-file");
    char const *group = global_option_get("socket-group");
    int perms = (int)strtol(global_option_get("socket-perms"), NULL, 8);
    bool delete = IS_TRUE(global_option_get("socket-delete"));


    httpd_listen = httpd_listen_init();

    int status = httpd_open_unix_socket(httpd_listen, httpd_sock_file, 128, group, perms, delete);
    if (status != 0) {

    }

    httpd = httpd_init(httpd_listen, HTTP_MAX_CONNECTIONS, -1);
    if (httpd == NULL) {

    }

    httpd_in_loop = 1;

    const char *name = "httpd listen";

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    set_thread_setaffinity(&attr, name);

    status = pthread_create(&httpd_listen_thread, &attr, httpd_server, NULL);
    pthread_attr_destroy(&attr);
    if (status != 0) {
        ERROR("pthread_create failed: %s", STRERRNO);
        // clean FIXME
        return -1;
    }

    set_thread_name(httpd_listen_thread, name);

    return 0;
}

int http_server_shutdown(void)
{
    httpd_in_loop = 0;

    if (httpd_listen_thread != (pthread_t)0) {
        httpd_stop(httpd);
        pthread_kill(httpd_listen_thread, SIGTERM);
        void *ret;
        pthread_join(httpd_listen_thread, &ret);
        httpd_listen_thread = (pthread_t)0;
    }

    httpd_listen_free(httpd_listen);
    httpd_listen = NULL;
    httpd_free(httpd);
    httpd = NULL;

    return 0;
}
