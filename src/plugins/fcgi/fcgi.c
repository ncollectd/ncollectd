// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/time.h"
#include "libutils/buf.h"
#include "libutils/socket.h"

#include <poll.h>

#define FCGI_VERSION    1
#define FCGI_HEADER_LEN 8
#define FCGI_MAX_LENGTH 0xffff

#define FCGI_BEGIN_REQUEST      1
#define FCGI_ABORT_REQUEST      2
#define FCGI_END_REQUEST        3
#define FCGI_PARAMS             4
#define FCGI_STDIN              5
#define FCGI_STDOUT             6
#define FCGI_STDERR             7
#define FCGI_DATA               8
#define FCGI_GET_VALUES         9
#define FCGI_GET_VALUES_RESULT 10
#define FCGI_UNKNOWN_TYPE      11

#define FCGI_RESPONDER  1
#define FCGI_AUTHORIZER 2
#define FCGI_FILTER     3

#define FCGI_REQUEST_COMPLETE 0
#define FCGI_CANT_MPX_CONN    1
#define FCGI_OVERLOADED       2
#define FCGI_UNKNOWN_ROLE     3

typedef struct __attribute__((packed)) {
    uint8_t version;
    uint8_t type;
    uint16_t request_id;
    uint16_t content_length;
    uint8_t padding_length;
    uint8_t reserved;
} fcgi_header_t;

typedef struct __attribute__((packed)) {
    uint16_t role;
    uint8_t flags;
    uint8_t reserved[5];
} fcgi_begin_request_body_t;

typedef struct __attribute__((packed)) {
    fcgi_header_t header;
    fcgi_begin_request_body_t body;
} fcgi_begin_request_record_t;

typedef struct __attribute__((packed)) {
    uint32_t app_status;
    uint8_t protocol_status;
    uint8_t reserved[3];
} fcgi_end_request_body_t;

typedef struct {
    char *key;
    size_t key_len;
    char *value;
    size_t value_len;
} fcgi_param_t;

typedef struct {
    size_t num;
    fcgi_param_t *ptr;
} fcgi_param_list_t;

typedef struct {
    char *name;

    char *metric_prefix;
    label_set_t labels;
    plugin_filter_t *filter;

    char *metric_response_time;
    char *metric_response_code;

    char *host;
    int port;
    char *socket_path;

    fcgi_param_list_t params;
    char *data;

    bool response_time;
    bool response_code;
    cdtime_t timeout;

    buf_t request;
    strbuf_t response;

    plugin_match_t *matches;
} fcgi_ctx_t;

static void fcgi_free(void *arg)
{
    fcgi_ctx_t *ctx = arg;
    if (ctx == NULL)
        return;

    free(ctx->metric_prefix);
    label_set_reset(&ctx->labels);
    plugin_filter_free(ctx->filter);
    free(ctx->metric_response_time);
    free(ctx->metric_response_code);
    free(ctx->host);
    free(ctx->socket_path);
    free(ctx->data);

//    buf_reset(&ctx->request);
//    strbuf_reset(&ctx->response);

    if (ctx->matches != NULL)
        plugin_match_shutdown(ctx->matches);

    free(ctx);
}

static int fcgi_param_size(buf_t *buf, size_t len)
{
    int status = 0;

    if (len < 0x80) {
        status |= buf_putuint8(buf, (uint8_t) len);
    } else {
        status |= buf_putuint8(buf, (uint8_t) ((len >> 24) | 0x80));
        status |= buf_putuint8(buf, (uint8_t) (len >> 16));
        status |= buf_putuint8(buf, (uint8_t) (len >> 8));
        status |= buf_putuint8(buf, (uint8_t) len);
    }

    return status;
}

static int fcgi_build_request(fcgi_ctx_t *ctx)
{
    int request_id = 1;

    fcgi_begin_request_record_t br = {
        .header.version = FCGI_VERSION,
        .header.type = FCGI_BEGIN_REQUEST,
        .header.request_id = htons(request_id),
        .header.content_length = htons(sizeof(br.body)),
        .body.role = htons(FCGI_RESPONDER),
    };

    int status = buf_put(&ctx->request, &br, sizeof(br));
    if (ctx->params.num > 0) {
        size_t params_size = 0;

        for (size_t i = 0; i < ctx->params.num; i++) {
            fcgi_param_t *param = &ctx->params.ptr[i];

            params_size += param->key_len < 0x80 ? 1 : 4;
            params_size += param->value_len < 0x80 ? 1 : 4;

            params_size += param->key_len;
            params_size += param->value_len;
        }

        if (params_size > FCGI_MAX_LENGTH) {
            PLUGIN_ERROR("Params size is greater than 65536");
            return -1;
        }

        fcgi_header_t hdr_params = {
            .version = FCGI_VERSION,
            .type = FCGI_PARAMS,
            .request_id = htons(request_id),
            .content_length = htons(params_size),
        };

        status |= buf_put(&ctx->request, &hdr_params, sizeof(hdr_params));

        for (size_t i = 0; i < ctx->params.num; i++) {
            fcgi_param_t *param = &ctx->params.ptr[i];

            fcgi_param_size(&ctx->request, param->key_len);
            fcgi_param_size(&ctx->request, param->value_len);
            buf_put(&ctx->request, param->key, param->key_len);
            buf_put(&ctx->request, param->value, param->value_len);
        }
    }

    fcgi_header_t hdr_params_end = {
        .version = FCGI_VERSION,
        .type = FCGI_PARAMS,
        .request_id = htons(request_id),
        .content_length = 0,
    };

    status |= buf_put(&ctx->request, &hdr_params_end, sizeof(hdr_params_end));

    if (ctx->data != NULL) {
        size_t data_len = strlen(ctx->data);

        if (data_len > FCGI_MAX_LENGTH) {
            PLUGIN_ERROR("Data size is greater than 65536");
            return -1;
        }

        fcgi_header_t hdr_stdin = {
            .version = FCGI_VERSION,
            .type = FCGI_STDIN,
            .request_id = htons(request_id),
            .content_length = htons(data_len),
        };

        status |= buf_put(&ctx->request, &hdr_stdin, sizeof(hdr_stdin));
        status |= buf_put(&ctx->request, ctx->data, sizeof(data_len));
    }

    fcgi_header_t hdr_stdin_end = {
        .version = FCGI_VERSION,
        .type = FCGI_STDIN,
        .request_id = htons(request_id),
        .content_length = 0,
    };

    status |= buf_put(&ctx->request, &hdr_stdin_end, sizeof(hdr_stdin_end));

    return status;
}

static ssize_t read_timeout(int fd, void *buf, size_t count, cdtime_t *timeout)
{
    struct pollfd fds = {
        .fd = fd,
        .events = POLLIN,
    };

    cdtime_t start = cdtime();

    int status = poll(&fds, 1, CDTIME_T_TO_MS(*timeout));
    if (status < 0) {
        PLUGIN_ERROR("poll error: %s", STRERRNO);
        return -1;
    }

    if (status == 0) {
        *timeout -= cdtime() - start;
        return 0;
    }

    if (fds.revents & POLLIN) {
        ssize_t size = read(fd, buf, count);
        *timeout -= cdtime() - start;
        return size;
    }

    return 0;
}

static ssize_t sread_timeout(int fd, void *buf, size_t count, cdtime_t *timeout)
{
    char *ptr = (char *)buf;
    size_t nleft = count;
    size_t bread = 0;

    while (nleft > 0) {
        ssize_t status = read_timeout(fd, (void *)ptr, nleft, timeout);

        if (*timeout <= 0)
            return -1;

        if ((status < 0) && ((errno == EAGAIN) || (errno == EINTR)))
            continue;

        if (status < 0)
            return status;

        if (status == 0)
            return -1;

        assert((0 > status) || (nleft >= (size_t)status));

        nleft -= ((size_t)status);
        ptr += ((size_t)status);
        bread += ((size_t)status);
    }

    return bread;
}

static int fcgi_read_response(fcgi_ctx_t *ctx, int fd)
{
    char cbuffer[65792];

    cdtime_t timeout = ctx->timeout;

    while (timeout > 0) {
        fcgi_header_t hdr;

        ssize_t size = sread_timeout(fd, &hdr, sizeof(hdr), &timeout);
        if (size != sizeof(hdr)) {
            PLUGIN_ERROR("Invalid header size, got %zd expect %zu.", size, sizeof(hdr));
            return -1;
        }

        uint16_t content_length = ntohs(hdr.content_length);
        if (content_length <= 0)
            return -1;

        ssize_t csize = content_length + hdr.padding_length;
        if ((size_t)csize > sizeof(cbuffer))
            return -1;

        ssize_t rcsize = sread_timeout(fd, cbuffer, csize, &timeout);
        if (rcsize != csize) {
            PLUGIN_ERROR("Cannot read full body, got %zd expect %zd.", rcsize, csize);
            return -1;
        }

        switch (hdr.type) {
        case FCGI_STDOUT:
            strbuf_putstrn(&ctx->response, cbuffer, content_length);
            break;
        case FCGI_STDERR:
            PLUGIN_ERROR("FCGI_STDERR: %.*s", (int)content_length, cbuffer);
            break;
        case FCGI_END_REQUEST: {
            fcgi_end_request_body_t erb;
            if (content_length != sizeof(erb)) {
                PLUGIN_ERROR("protocol error: invalid end request body size");
                return -1;
            }

            memcpy(&erb, cbuffer, sizeof(erb));
            if (erb.protocol_status != FCGI_REQUEST_COMPLETE) {
                PLUGIN_ERROR("protocol error: protocol status is not FCGI_REQUEST_COMPLETE");
                return -1;
            }
            return 0;
        }   break;
        default:
            break;
        }
    }

    return -1;
}

static void fcgi_submit_response_code(fcgi_ctx_t *ctx, int response_code)
{
    if (ctx->response_code) {
        metric_family_t fam = {
            .name = ctx->metric_response_code,
            .type = METRIC_TYPE_GAUGE,
        };
        metric_family_append(&fam, VALUE_GAUGE(response_code), &ctx->labels, NULL);
        plugin_dispatch_metric_family_filtered(&fam, ctx->filter, 0);
    }
}

static void fcgi_submit_response_time(fcgi_ctx_t *ctx, cdtime_t start)
{
    if (ctx->response_time) {
        metric_family_t fam = {
            .name = ctx->metric_response_time,
            .type = METRIC_TYPE_GAUGE,
        };
        double value = CDTIME_T_TO_DOUBLE(cdtime() - start);
        metric_family_append(&fam, VALUE_GAUGE(value), &ctx->labels, NULL);
        plugin_dispatch_metric_family_filtered(&fam, ctx->filter, 0);
    }
}

static int fcgi_read(user_data_t *ud)
{
    if ((ud == NULL) || (ud->data == NULL)) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }

    fcgi_ctx_t *ctx = (fcgi_ctx_t *)ud->data;

    strbuf_reset(&ctx->response);

    cdtime_t start = 0;
    if (ctx->response_time)
        start = cdtime();

    int fd = -1;

    if (ctx->socket_path != NULL)
        fd = socket_connect_unix_stream(ctx->socket_path, 0);
    else
        fd = socket_connect_tcp(ctx->host, ctx->port, 0, 0);

    if (fd < 0) {
        fcgi_submit_response_code(ctx, 0);
        return -1;
    }

    int status = swrite(fd, ctx->request.ptr , buf_len(&ctx->request));
    if (status != 0) {
        PLUGIN_ERROR("Instance '%s': write(2) failed: %s", ctx->name, STRERRNO);
        fcgi_submit_response_code(ctx, 0);
        close(fd);
        return 0;
    }

    status = fcgi_read_response(ctx, fd);
    if (status != 0) {
        fcgi_submit_response_code(ctx, 0);
        fcgi_submit_response_time(ctx, start);
        close(fd);
        return 0;
    }

    close(fd);

    fcgi_submit_response_time(ctx, start);

    char *body = NULL;
    char *end = strstr(ctx->response.ptr, "\r\n\r\n");
    if (end != NULL) {
        *end = '\0';
        body = end + 4;
    }

    int code = 200;
    char *ptr = ctx->response.ptr;
    char *saveptr = NULL;
    char *line;
    while ((line = strtok_r(ptr, "\r\n", &saveptr)) != NULL) {
        ptr = NULL;
        if (strncmp("Status: ", line, strlen("Status: ")) == 0) {
            code = atoi(line + strlen("Status: "));
            break;
        }
    }

    fcgi_submit_response_code(ctx, code);

    if (body != NULL) {
        status = plugin_match(ctx->matches, body);
        if (status != 0)
            PLUGIN_WARNING("plugin_match failed.");
        plugin_match_dispatch(ctx->matches, ctx->filter, &ctx->labels, true);
    }

    return 0;
}

static int fcgi_config_param(config_item_t *ci, fcgi_param_list_t *params)
{
    if ((ci->values_num != 2) ||
        ((ci->values_num == 2) && ((ci->values[0].type != CONFIG_TYPE_STRING) ||
                                   (ci->values[1].type != CONFIG_TYPE_STRING)))) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly two string arguments.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    fcgi_param_t *tmp = realloc(params->ptr, sizeof(*params->ptr) * (params->num +1));
    if (tmp == NULL) {
        PLUGIN_ERROR("realloc failed");
        return -1;
    }

    params->ptr = tmp;

    fcgi_param_t *param = &tmp[params->num];

    param->key = strdup(ci->values[0].value.string);
    param->key_len = strlen(ci->values[0].value.string);
    param->value = strdup(ci->values[1].value.string);
    param->value_len = strlen(ci->values[1].value.string);

    if ((param->key == NULL) || (param->value == NULL)) {
        free(param->key);
        free(param->value);
        PLUGIN_ERROR("strdup failed");
        return -1;
    }

    params->num++;

    return 0;
}

static int fcgi_config_instance(config_item_t *ci)
{
    fcgi_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = cf_util_get_string(ci, &ctx->name);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        free(ctx);
        return status;
    }

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("host", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->host);
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_port_number(child, &ctx->port);
        } else if (strcasecmp("socket-path", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->socket_path);
        } else if (strcasecmp("metric-prefix", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->metric_prefix);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &ctx->labels);
        } else if (strcasecmp("measure-response-time", child->key) == 0) {
            status = cf_util_get_boolean(child, &ctx->response_time);
        } else if (strcasecmp("measure-response-code", child->key) == 0) {
            status = cf_util_get_boolean(child, &ctx->response_code);
        } else if (strcasecmp("match", child->key) == 0) {
            status = plugin_match_config(child, &ctx->matches);
        } else if (strcasecmp("param", child->key) == 0) {
            status = fcgi_config_param(child, &ctx->params);
        } else if (strcasecmp("data", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->data);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &ctx->timeout);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &ctx->filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                         child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    while (status == 0) {
        if ((ctx->host == NULL) && (ctx->socket_path == NULL)) {
            PLUGIN_WARNING("'host' or 'socket-path' missing in 'instance' block.");
            status = -1;
            break;
        }

        if ((ctx->host != NULL) && (ctx->port == 0)) {
            PLUGIN_WARNING("'port' missing in 'instance' block.");
            status = -1;
            break;
        }

        if ((ctx->matches == NULL) && !ctx->response_time && !ctx->response_code) {
            PLUGIN_WARNING("No (valid) 'match' block or 'statistics' or 'measure-response-time' "
                           "or 'measure-response-code' within  block '%s'.", ctx->name);
            status = -1;
            break;
        }

        if (ctx->response_time) {
            if (ctx->metric_prefix == NULL)
                ctx->metric_response_time = strdup("fcgi_response_time_seconds");
            else
                ctx->metric_response_time = ssnprintf_alloc("%s_response_time_seconds",
                                                             ctx->metric_prefix);

            if (ctx->metric_response_time == NULL) {
                PLUGIN_ERROR("alloc metric response time string failed.");
                status = -1;
                break;
            }
        }

        if (ctx->response_code) {
            if (ctx->metric_prefix == NULL)
                ctx->metric_response_code = strdup("fcgi_response_code");
            else
                ctx->metric_response_code = ssnprintf_alloc("%s_response_code",
                                                             ctx->metric_prefix);

            if (ctx->metric_response_code == NULL) {
                PLUGIN_ERROR("alloc metric response code string failed.");
                status = -1;
                break;
            }
        }

        break;
    }

    if (status != 0) {
        fcgi_free(ctx);
        return status;
    }

    status = fcgi_build_request(ctx);
    if (status != 0) {
        fcgi_free(ctx);
        return status;
    }

    if (ctx->timeout == 0)
        ctx->timeout = interval != 0 ? interval/2 : plugin_get_interval()/2;

    label_set_add(&ctx->labels, true, "instance", ctx->name);

    return plugin_register_complex_read("fcgi", ctx->name, fcgi_read, interval,
                                        &(user_data_t){.data=ctx, .free_func=fcgi_free});
}

static int fcgi_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp("instance", child->key) == 0) {
            status = fcgi_config_instance(child);
        } else {
            PLUGIN_ERROR("The configuration option '%s' in %s:%d is not allowed here.",
                         child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("fcgi", fcgi_config);
}
