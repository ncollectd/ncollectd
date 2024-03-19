// SPDX-License-Identifier: GPL-2.0-only

#include "ncollectd.h"

#include <sys/un.h>

#include "libutils/common.h"
#include "libhttp/http.h"
#include "libmdb/strlist.h"
#include "libmdb/series_list.h"
#include "libmdb/family_metric_list.h"
#include "libformat/notification_json.h"

#include "ncollectdctl/client.h"

struct client_s {
    char *path;
};

client_t *client_create(char *path)
{
    char *npath = sstrdup(path);
    if (npath == NULL) {
        return NULL;
    }

    client_t *client = calloc(1, sizeof(*client));
    if (client == NULL) {
        free(npath);
        return NULL;
    }

    client->path = npath;

    return client;
}

void client_destroy(client_t *client)
{
    if (client == NULL)
        return;

    free(client->path);
    free(client);
}

int client_connect(client_t *client)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    struct sockaddr_un sa = {0};
    sa.sun_family = AF_UNIX;
    sstrncpy(sa.sun_path, client->path, sizeof(sa.sun_path) - 1);

    int status = connect(fd, (struct sockaddr *)&sa, sizeof(sa));
    if (status != 0) {
        close(fd);
        return -1;
    }

    return fd;
}

int client_http_get(client_t *client, char *path, char **content, size_t *content_length)
{
    if (client == NULL)
        return -1;

    http_header_t headers[] = {
        { .header_name = HTTP_HEADER_HOST, .value = "localhost" },
        { .header_name = HTTP_HEADER_ACCEPT, .value = "application/json" },
    };
    http_request_t request = {
        .method = HTTP_METHOD_GET,
        .path = path,
        .version = HTTP_VERSION_1_1,
        .headers.num = STATIC_ARRAY_SIZE(headers),
        .headers.ptr = headers,
        .content_length = 0,
        .content = NULL
    };
    http_response_t response = {0};
    int timeout = 30000;

    int fd = client_connect(client);
    if (fd < 0) {
        return -1;
    }

    int status = http_fetch(fd, &request, &response, &timeout);

    if (status != 0) {
        close(fd);
        return -1;
    }

    if (response.status_code != HTTP_STATUS_200) {
        if (response.content_length > 0)
            free(response.content);
        close(fd);
        return -1;
    }

    http_header_reset(&response.headers);

    *content = response.content;
    *content_length = response.content_length;

    close(fd);

    return 0;
}

int client_http_post(client_t *client, char *path,
                                       char *request_content, size_t request_content_length,
                                       char **response_content, size_t *response_content_length)
{
    if (client == NULL)
        return -1;

    http_header_t headers[] = {
        { .header_name = HTTP_HEADER_HOST, .value = "localhost" },
        { .header_name = HTTP_HEADER_ACCEPT, .value = "application/json" },
    };
    http_request_t request = {
        .method = HTTP_METHOD_POST,
        .path = path,
        .version = HTTP_VERSION_1_1,
        .headers.num = STATIC_ARRAY_SIZE(headers),
        .headers.ptr = headers,
        .content_length = request_content_length,
        .content = request_content
    };
    http_response_t response = {0};
    int timeout = 30000;

    int fd = client_connect(client);
    if (fd < 0) {
        return -1;
    }

    int status = http_fetch(fd, &request, &response, &timeout);

    if (status != 0) {
        close(fd);
        return -1;
    }

    if (response.status_code != HTTP_STATUS_200) {
        if (response.content_length > 0)
            free(response.content);
        close(fd);
        return -1;
    }

    http_header_reset(&response.headers);

    *response_content = response.content;
    *response_content_length = response.content_length;

    close(fd);

    return 0;
}

static strlist_t *client_get_plugins(client_t *client, char *path)
{
    if (client == NULL)
        return NULL;

    char *content = NULL;
    size_t content_length = 0;

    int status = client_http_get(client, path, &content, &content_length);
    if (status != 0)
        return NULL;

    if ((content_length > 0) && (content != NULL)) {
        strlist_t *list = mdb_strlist_parse(content, content_length);
        free(content);
        if (list != NULL)
            return list;
    }

    return NULL;
}

strlist_t *client_get_plugins_readers(client_t *client)
{
    return client_get_plugins(client, "/api/v1/readers");
}

strlist_t *client_get_plugins_writers(client_t *client)
{
    return client_get_plugins(client, "/api/v1/writers");
}

strlist_t *client_get_plugins_loggers(client_t *client)
{
    return client_get_plugins(client, "/api/v1/loggers");
}

strlist_t *client_get_plugins_notificators(client_t *client)
{
    return client_get_plugins(client, "/api/v1/notificators");
}

#if 0
response_values_list_t *client_get_values(client_t *client,

        start
        end
        matches_list_t
{
    "/api/v1/read"
  int64 start_timestamp_ms = 1;
  int64 end_timestamp_ms = 2;
  repeated prometheus.LabelMatcher matchers = 3;
  prometheus.ReadHints hints = 4;

    return NULL;
}

client_post_values
client_write
    "/api/v1/write"
client_post_notification
client_notification
    "/api/v1/notification"

client_query
    "/api/v1/query"

client_query_range
    "/api/v1/query_range"
#endif

mdb_series_list_t *client_get_series(client_t *client)
{
    if (client == NULL)
        return NULL;

    char *content = NULL;
    size_t content_length = 0;

    int status = client_http_get(client, "/api/v1/series", &content, &content_length);
    if (status != 0)
        return NULL;

    if ((content_length > 0) && (content != NULL)) {
        mdb_series_list_t *list = mdb_series_list_parse(content, content_length);
        free(content);
        if (list != NULL) {
            return list;
        }
    }

    return NULL;
}
#if 0
response_metric_families_list_t *client_get_metric_families(client_t *client)
{
    if (client == NULL)
        return NULL;

    char *content = NULL;
    size_t content_length = 0;

    int status = client_http_get(client, "/api/v1/metric-families", &content, &content_length);
    if (status != 0)
        return NULL;

    if ((content_length > 0) && (content != NULL)) {
        response_metric_families_list_t *list = response_metric_families_list_parse(content,
                                                                                    content_length);
        free(content);
        if (list != NULL)
            return list;
    }

    return NULL;

}
#endif

mdb_family_metric_list_t *client_get_family_metrics(client_t *client)
{
    if (client == NULL)
        return NULL;

    char *content = NULL;
    size_t content_length = 0;

    int status = client_http_get(client, "/api/v1/family_metrics", &content, &content_length);
    if (status != 0)
        return NULL;

    if ((content_length > 0) && (content != NULL)) {
        mdb_family_metric_list_t *list = mdb_family_metric_list_parse(content, content_length);
        free(content);
        if (list != NULL)
            return list;
    }

    return NULL;
}

strlist_t *client_get_metrics(client_t *client)
{
    if (client == NULL)
        return NULL;

    char *content = NULL;
    size_t content_length = 0;

    int status = client_http_get(client, "/api/v1/metrics", &content, &content_length);
    if (status != 0)
        return NULL;

    if ((content_length > 0) && (content != NULL)) {
        strlist_t *list = mdb_strlist_parse(content, content_length);
        free(content);
        if (list != NULL)
            return list;
    }

    return NULL;
}

strlist_t *client_get_metric_labels(client_t *client, char *metric)
{
    if (client == NULL)
        return NULL;
    if (metric == NULL)
        return NULL;

    char path[1024];
    ssnprintf(path, sizeof(path), "/api/v1/metric/%s/labels", metric);

    char *content = NULL;
    size_t content_length = 0;

    int status = client_http_get(client, path, &content, &content_length);
    if (status != 0)
        return NULL;

    if ((content_length > 0) && (content != NULL)) {
        strlist_t *list = mdb_strlist_parse(content, content_length);
        free(content);
        if (list != NULL)
            return list;
    }

    return NULL;
}

strlist_t *client_get_metric_label_values(client_t *client, char *metric, char *label)
{
    if (client == NULL)
        return NULL;
    if (metric == NULL)
        return NULL;
    if (label == NULL)
        return NULL;

    char path[1024];
    ssnprintf(path, sizeof(path), "/api/v1/metric/%s/label/%s", metric, label);

    char *content = NULL;
    size_t content_length = 0;

    int status = client_http_get(client, path, &content, &content_length);
    if (status != 0)
        return NULL;

    if ((content_length > 0) && (content != NULL)) {
        strlist_t *list = mdb_strlist_parse(content, content_length);
        free(content);
        if (list != NULL)
            return list;
    }

    return NULL;
}

int client_post_write(client_t *client, char *data, size_t data_len)
{
    if (data == NULL)
        return -1;
    if (data_len == 0)
        return -1;

    char *response= NULL;
    size_t response_length = 0;

    int status = client_http_post(client, "/api/v1/write",
                                          data, data_len, &response, &response_length);
    if (status != 0)
        return -1;

    if ((response_length > 0) && (response != NULL)) {

    }

    return 0;
}

int client_post_notification(client_t *client, notification_t *n)
{
    if (n == NULL)
        return -1;

    strbuf_t buf = STRBUF_CREATE;

    int status = notification_json(&buf, n);
    if (status != 0)
        return -1;

    char *response= NULL;
    size_t response_length = 0;
    status = client_http_post(client, "/api/v1/notification",
                                      buf.ptr, strbuf_len(&buf), &response, &response_length);
    if (status != 0)
        return -1;

    if ((response_length > 0) && (response != NULL)) {

    }

    strbuf_destroy(&buf);

    return 0;
}
