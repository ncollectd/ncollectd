// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2006-2010  Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2007 Florent EppO Monbillard
// SPDX-FileCopyrightText: Copyright (C) 2009 Amit Gupta
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Florent EppO Monbillard <eppo at darox.net>
// SPDX-FileContributor: Amit Gupta <amit.gupta221 at gmail.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <curl/curl.h>

enum {
    FAM_APACHE_UP,
    FAM_APACHE_REQUESTS,
    FAM_APACHE_BYTES,
    FAM_APACHE_WORKERS,
    FAM_APACHE_SCOREBOARD,
    FAM_APACHE_CONNECTIONS,
    FAM_APACHE_PROCESSES,
    FAM_APACHE_UPTIME,
    FAM_APACHE_MAX
};

static metric_family_t fams[FAM_APACHE_MAX] = {
    [FAM_APACHE_UP] = {
        .name = "apache_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the apache server be reached.",
    },
    [FAM_APACHE_REQUESTS] = {
        .name = "apache_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Apache total requests.",
    },
    [FAM_APACHE_BYTES] = {
        .name = "apache_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Apache total bytes sent.",
    },
    [FAM_APACHE_WORKERS] = {
        .name = "apache_workers",
        .type = METRIC_TYPE_GAUGE,
        .help = "Apache current number of workers.",
    },
    [FAM_APACHE_SCOREBOARD] = {
        .name = "apache_scoreboard",
        .type = METRIC_TYPE_GAUGE,
        .help = "Apache scoreboard statuses.",
    },
    [FAM_APACHE_CONNECTIONS] = {
        .name = "apache_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Apache current number of connections.",
    },
    [FAM_APACHE_PROCESSES] = {
        .name = "apache_processes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Apache current number of processes.",
    },
    [FAM_APACHE_UPTIME] = {
        .name = "apache_uptime_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Apache server uptime.",
    },
};

enum server_enum {
    APACHE = 0,
    LIGHTTPD
};

typedef struct {
    int server_type;
    char *name;
    char *url;
    char *user;
    char *pass;
    bool verify_peer;
    bool verify_host;
    char *cacert;
    char *ssl_ciphers;
    char *server; /* user specific server type */
    label_set_t labels;
    plugin_filter_t *filter;

    char *apache_buffer;
    char apache_curl_error[CURL_ERROR_SIZE];
    size_t apache_buffer_size;
    size_t apache_buffer_fill;
    cdtime_t timeout;
    CURL *curl;

    metric_family_t fams[FAM_APACHE_MAX];
} apache_ctx_t;

static void apache_free(void *arg)
{
    apache_ctx_t *ctx = arg;

    if (ctx == NULL)
        return;

    free(ctx->name);
    free(ctx->url);
    free(ctx->user);
    free(ctx->pass);
    free(ctx->cacert);
    free(ctx->ssl_ciphers);
    free(ctx->server);
    free(ctx->apache_buffer);
    label_set_reset(&ctx->labels);
    if (ctx->curl) {
        curl_easy_cleanup(ctx->curl);
        ctx->curl = NULL;
    }
    plugin_filter_free(ctx->filter);
    free(ctx);
}

static size_t apache_curl_callback(void *buf, size_t size, size_t nmemb, void *user_data)
{
    apache_ctx_t *ctx = user_data;
    if (ctx == NULL) {
        PLUGIN_ERROR("user_data pointer is NULL.");
        return 0;
    }

    size_t len = size * nmemb;
    if (len == 0)
        return len;

    if ((ctx->apache_buffer_fill + len) >= ctx->apache_buffer_size) {
        char *temp = realloc(ctx->apache_buffer, ctx->apache_buffer_fill + len + 1);
        if (temp == NULL) {
            PLUGIN_ERROR("realloc failed.");
            return 0;
        }
        ctx->apache_buffer = temp;
        ctx->apache_buffer_size = ctx->apache_buffer_fill + len + 1;
    }

    memcpy(ctx->apache_buffer + ctx->apache_buffer_fill, (char *)buf, len);
    ctx->apache_buffer_fill += len;
    ctx->apache_buffer[ctx->apache_buffer_fill] = 0;

    return len;
}

static size_t apache_header_callback(void *buf, size_t size, size_t nmemb, void *user_data)
{
    apache_ctx_t *ctx = user_data;
    if (ctx == NULL) {
        PLUGIN_ERROR("user_data pointer is NULL.");
        return 0;
    }

    size_t len = size * nmemb;
    if (len == 0)
        return len;

    /* look for the Server header */
    if (strncasecmp(buf, "Server: ", strlen("Server: ")) != 0)
        return len;

    if (strstr(buf, "Apache") != NULL) {
        ctx->server_type = APACHE;
    } else if (strstr(buf, "lighttpd") != NULL) {
        ctx->server_type = LIGHTTPD;
    } else if (strstr(buf, "IBM_HTTP_Server") != NULL) {
        ctx->server_type = APACHE;
    } else {
        const char *hdr = buf;
        hdr += strlen("Server: ");
        PLUGIN_NOTICE("Unknown server software: %s", hdr);
    }

    return len;
}

/* initialize curl for each host */
static int apache_init_instance(apache_ctx_t *ctx)
{
    assert(ctx->url != NULL);
    /* (Assured by `config_add') */

    if (ctx->curl != NULL) {
        curl_easy_cleanup(ctx->curl);
        ctx->curl = NULL;
    }

    if ((ctx->curl = curl_easy_init()) == NULL) {
        PLUGIN_ERROR("'curl_easy_init' failed.");
        return -1;
    }

    CURLcode rcode = 0;

    rcode = curl_easy_setopt(ctx->curl, CURLOPT_NOSIGNAL, 1L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_NOSIGNAL failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(ctx->curl, CURLOPT_WRITEFUNCTION, apache_curl_callback);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEFUNCTION failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, ctx);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEDATA failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }


    /* not set as yet if the user specified string doesn't match apache or
     * lighttpd, then ignore it. Headers will be parsed to find out the
     * server type */
    ctx->server_type = -1;

    if (ctx->server != NULL) {
        if (strcasecmp(ctx->server, "apache") == 0)
            ctx->server_type = APACHE;
        else if (strcasecmp(ctx->server, "lighttpd") == 0)
            ctx->server_type = LIGHTTPD;
        else if (strcasecmp(ctx->server, "ibm_http_server") == 0)
            ctx->server_type = APACHE;
        else
            PLUGIN_WARNING("Unknown 'Server' setting: %s", ctx->server);
    }

    /* if not found register a header callback to determine the server_type */
    if (ctx->server_type == -1) {
        rcode = curl_easy_setopt(ctx->curl, CURLOPT_HEADERFUNCTION, apache_header_callback);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_HEADERFUNCTION failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(ctx->curl, CURLOPT_WRITEHEADER, ctx);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEHEADER failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

    }

    rcode = curl_easy_setopt(ctx->curl, CURLOPT_USERAGENT, NCOLLECTD_USERAGENT);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERAGENT failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(ctx->curl, CURLOPT_ERRORBUFFER, ctx->apache_curl_error);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_ERRORBUFFER failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }


    if (ctx->user != NULL) {
#ifdef HAVE_CURLOPT_USERNAME
        rcode = curl_easy_setopt(ctx->curl, CURLOPT_USERNAME, ctx->user);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERNAME failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(ctx->curl, CURLOPT_PASSWORD, (ctx->pass == NULL) ? "" : ctx->pass);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_PASSWORD failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

#else
        static char credentials[1024];
        int status = snprintf(credentials, sizeof(credentials), "%s:%s", ctx->user,
                                           (ctx->pass == NULL) ? "" : ctx->pass);
        if ((status < 0) || ((size_t)status >= sizeof(credentials))) {
            PLUGIN_ERROR("Returning an error because the credentials have been truncated.");
            curl_easy_cleanup(ctx->curl);
            ctx->curl = NULL;
            return -1;
        }

        rcode = curl_easy_setopt(ctx->curl, CURLOPT_USERPWD, credentials);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERPWD failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

#endif
    }

    rcode = curl_easy_setopt(ctx->curl, CURLOPT_FOLLOWLOCATION, 1L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_FOLLOWLOCATION failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(ctx->curl, CURLOPT_MAXREDIRS, 50L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_MAXREDIRS failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }


    rcode = curl_easy_setopt(ctx->curl, CURLOPT_SSL_VERIFYPEER, (long)ctx->verify_peer);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_SSL_VERIFYPEER failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(ctx->curl, CURLOPT_SSL_VERIFYHOST, ctx->verify_host ? 2L : 0L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_SSL_VERIFYHOST failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    if (ctx->cacert != NULL) {
        rcode = curl_easy_setopt(ctx->curl, CURLOPT_CAINFO, ctx->cacert);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_CAINFO failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }

    if (ctx->ssl_ciphers != NULL) {
        rcode = curl_easy_setopt(ctx->curl, CURLOPT_SSL_CIPHER_LIST, ctx->ssl_ciphers);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_SSL_CIPHER_LIST failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }


#ifdef HAVE_CURLOPT_TIMEOUT_MS
    if (ctx->timeout != 0) {
        rcode = curl_easy_setopt(ctx->curl, CURLOPT_TIMEOUT_MS,
                                            (long)CDTIME_T_TO_MS(ctx->timeout));
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_TIMEOUT_MS failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    } else {
        rcode = curl_easy_setopt(ctx->curl, CURLOPT_TIMEOUT_MS,
                                            (long)CDTIME_T_TO_MS(plugin_get_interval()));
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_TIMEOUT_MS failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }
#endif

    return 0;
}

static void submit_scoreboard(char *buf, apache_ctx_t *ctx, metric_family_t *fam_scoreboard)
{
    /*
     * Scoreboard Key:
     * "_" Waiting for Connection, "S" Starting up,
     * "R" Reading Request for apache and read-POST for lighttpd,
     * "W" Sending Reply, "K" Keepalive (read), "D" DNS Lookup,
     * "C" Closing connection, "L" Logging, "G" Gracefully finishing,
     * "I" Idle cleanup of worker, "." Open slot with no current process
     * Lighttpd specific legends -
     * "E" hard error, "." connect, "h" handle-request,
     * "q" request-start, "Q" request-end, "s" response-start
     * "S" response-end, "r" read
     */
    long long open = 0LL;
    long long waiting = 0LL;
    long long starting = 0LL;
    long long reading = 0LL;
    long long sending = 0LL;
    long long keepalive = 0LL;
    long long dnslookup = 0LL;
    long long closing = 0LL;
    long long logging = 0LL;
    long long finishing = 0LL;
    long long idle_cleanup = 0LL;

    /* lighttpd specific */
    long long hard_error = 0LL;
    long long lighttpd_read = 0LL;
    long long handle_request = 0LL;
    long long request_start = 0LL;
    long long request_end = 0LL;
    long long response_start = 0LL;
    long long response_end = 0LL;

    for (int i = 0; buf[i] != '\0'; i++) {
        switch (buf[i]) {
        case '.':
            open++;
            break;
        case '_':
            waiting++;
            break;
        case 'S':
            if (ctx->server_type == LIGHTTPD)
                response_end++;
            else if (ctx->server_type == APACHE)
                starting++;
            break;
        case 'R':
            reading++;
            break;
        case 'W':
            sending++;
            break;
        case 'K':
            keepalive++;
            break;
        case 'D':
            dnslookup++;
            break;
        case 'C':
            closing++;
            break;
        case 'L':
            logging++;
            break;
        case 'G':
            finishing++;
            break;
        case 'I':
            idle_cleanup++;
            break;
        case 'r':
            lighttpd_read++;
            break;
        case 'h':
            handle_request++;
            break;
        case 'E':
            hard_error++;
            break;
        case 'q':
            request_start++;
            break;
        case 'Q':
            request_end++;
            break;
        case 's':
            response_start++;
            break;
        }
    }

    if (ctx->server_type == APACHE) {
        metric_family_append(fam_scoreboard, VALUE_GAUGE(open), &ctx->labels,
                             &(label_pair_const_t){.name="state", .value="open"}, NULL);
        metric_family_append(fam_scoreboard, VALUE_GAUGE(waiting), &ctx->labels,
                             &(label_pair_const_t){.name="state", .value="waiting"}, NULL);
        metric_family_append(fam_scoreboard, VALUE_GAUGE(starting), &ctx->labels,
                             &(label_pair_const_t){.name="state", .value="starting"}, NULL);
        metric_family_append(fam_scoreboard, VALUE_GAUGE(reading), &ctx->labels,
                             &(label_pair_const_t){.name="state", .value="reading"}, NULL);
        metric_family_append(fam_scoreboard, VALUE_GAUGE(sending), &ctx->labels,
                             &(label_pair_const_t){.name="state", .value="sending"}, NULL);
        metric_family_append(fam_scoreboard, VALUE_GAUGE(keepalive), &ctx->labels,
                             &(label_pair_const_t){.name="state", .value="keepalive"}, NULL);
        metric_family_append(fam_scoreboard, VALUE_GAUGE(dnslookup), &ctx->labels,
                             &(label_pair_const_t){.name="state", .value="dnslookup"}, NULL);
        metric_family_append(fam_scoreboard, VALUE_GAUGE(closing), &ctx->labels,
                             &(label_pair_const_t){.name="state", .value="closing"}, NULL);
        metric_family_append(fam_scoreboard, VALUE_GAUGE(logging), &ctx->labels,
                             &(label_pair_const_t){.name="state", .value="logging"}, NULL);
        metric_family_append(fam_scoreboard, VALUE_GAUGE(finishing), &ctx->labels,
                             &(label_pair_const_t){.name="state", .value="finishing"}, NULL);
        metric_family_append(fam_scoreboard, VALUE_GAUGE(idle_cleanup), &ctx->labels,
                             &(label_pair_const_t){.name="state", .value="idle_cleanup"}, NULL);
    } else {
        metric_family_append(fam_scoreboard, VALUE_GAUGE(open), &ctx->labels,
                             &(label_pair_const_t){.name="state", .value="connect"}, NULL);
        metric_family_append(fam_scoreboard, VALUE_GAUGE(closing), &ctx->labels,
                             &(label_pair_const_t){.name="state", .value="close"}, NULL);
        metric_family_append(fam_scoreboard, VALUE_GAUGE(hard_error), &ctx->labels,
                             &(label_pair_const_t){.name="state", .value="hard_error"}, NULL);
        metric_family_append(fam_scoreboard, VALUE_GAUGE(lighttpd_read), &ctx->labels,
                             &(label_pair_const_t){.name="state", .value="read"}, NULL);
        metric_family_append(fam_scoreboard, VALUE_GAUGE(reading), &ctx->labels,
                             &(label_pair_const_t){.name="state", .value="read_post"}, NULL);
        metric_family_append(fam_scoreboard, VALUE_GAUGE(sending), &ctx->labels,
                             &(label_pair_const_t){.name="state", .value="write"}, NULL);
        metric_family_append(fam_scoreboard, VALUE_GAUGE(handle_request), &ctx->labels,
                             &(label_pair_const_t){.name="state", .value="handle_request"}, NULL);
        metric_family_append(fam_scoreboard, VALUE_GAUGE(request_start), &ctx->labels,
                             &(label_pair_const_t){.name="state", .value="request_start"}, NULL);
        metric_family_append(fam_scoreboard, VALUE_GAUGE(request_end), &ctx->labels,
                             &(label_pair_const_t){.name="state", .value="request_end"}, NULL);
        metric_family_append(fam_scoreboard, VALUE_GAUGE(response_start), &ctx->labels,
                             &(label_pair_const_t){.name="state", .value="response_start"}, NULL);
        metric_family_append(fam_scoreboard, VALUE_GAUGE(response_end), &ctx->labels,
                             &(label_pair_const_t){.name="state", .value="response_end"}, NULL);
    }
}

static int apache_read(user_data_t *user_data)
{
    apache_ctx_t *ctx = user_data->data;

    assert(ctx->url != NULL);
    /* (Assured by `config_add') */

    if (ctx->curl == NULL) {
        if (apache_init_instance(ctx) != 0)
            return -1;
    }
    assert(ctx->curl != NULL);

    ctx->apache_buffer_fill = 0;

    CURLcode rcode = curl_easy_setopt(ctx->curl, CURLOPT_URL, ctx->url);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_URL failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    cdtime_t submit = cdtime();
    if (curl_easy_perform(ctx->curl) != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_perform failed: %s", ctx->apache_curl_error);
        metric_family_append(&ctx->fams[FAM_APACHE_UP], VALUE_GAUGE(0), &ctx->labels, NULL);
        plugin_dispatch_metric_family(&ctx->fams[FAM_APACHE_UP], 0);
        return 0;
    }

    /* fallback - server_type to apache if not set at this time */
    if (ctx->server_type == -1) {
        PLUGIN_WARNING("Unable to determine server software automatically. Will assume Apache.");
        ctx->server_type = APACHE;
    }

    char *content_type;
    static const char *text_plain = "text/plain";
    int status = curl_easy_getinfo(ctx->curl, CURLINFO_CONTENT_TYPE, &content_type);
    if ((status == CURLE_OK) && (content_type != NULL) &&
        (strncasecmp(content_type, text_plain, strlen(text_plain)) != 0)) {
        PLUGIN_WARNING("`Content-Type' response header is not `%s' "
                        "(received: `%s'). Expecting unparsable data. Please check `URL' "
                        "parameter (missing `?auto' suffix ?)",
                        text_plain, content_type);
    }

    char *ptr = ctx->apache_buffer;
    char *saveptr = NULL;
    char *line;
    /* Apache http mod_status added a second set of BusyWorkers, IdleWorkers in
     * https://github.com/apache/httpd/commit/6befc18
     * For Apache 2.4.35 and up we need to ensure only one key is used.
     * S.a. https://bz.apache.org/bugzilla/show_bug.cgi?id=63300
     */
    int apache_connections_submitted = 0, apache_idle_workers_submitted = 0;

    metric_family_append(&ctx->fams[FAM_APACHE_UP], VALUE_GAUGE(1), &ctx->labels, NULL);

    while ((line = strtok_r(ptr, "\n\r", &saveptr)) != NULL) {
        ptr = NULL;
        char *fields[4];

        int fields_num = strsplit(line, fields, STATIC_ARRAY_SIZE(fields));

        if (fields_num == 3) {
            if ((strcmp(fields[0], "Total") == 0) && (strcmp(fields[1], "Accesses:") == 0)) {
                metric_family_append(&ctx->fams[FAM_APACHE_REQUESTS],
                                     VALUE_COUNTER((uint64_t)atoll(fields[2])),
                                     &ctx->labels, NULL);
            } else if ((strcmp(fields[0], "Total") == 0) && (strcmp(fields[1], "kBytes:") == 0)) {
                metric_family_append(&ctx->fams[FAM_APACHE_BYTES],
                                     VALUE_COUNTER((uint64_t)(1024LL * atoll(fields[2]))),
                                     &ctx->labels, NULL);
            }
        } else if (fields_num == 2) {
            if (strcmp(fields[0], "Scoreboard:") == 0) {
                submit_scoreboard(fields[1], ctx, &ctx->fams[FAM_APACHE_SCOREBOARD]);
            } else if (!apache_connections_submitted &&
                       ((strcmp(fields[0], "BusyServers:") == 0) /* Apache 1.* */
                       || (strcmp(fields[0], "BusyWorkers:") == 0)) /* Apache 2.* */) {
                metric_family_append(&ctx->fams[FAM_APACHE_WORKERS],
                                     VALUE_GAUGE(atol(fields[1])), &ctx->labels,
                                     &(label_pair_const_t){.name="state", .value="busy"}, NULL);
                apache_connections_submitted++;
            } else if (!apache_idle_workers_submitted &&
                       ((strcmp(fields[0], "IdleServers:") == 0) /* Apache 1.x */
                       ||(strcmp(fields[0], "IdleWorkers:") == 0)) /* Apache 2.x */) {
                metric_family_append(&ctx->fams[FAM_APACHE_WORKERS],
                                     VALUE_GAUGE(atol(fields[1])), &ctx->labels,
                                     &(label_pair_const_t){.name="state", .value="idle"}, NULL);
                apache_idle_workers_submitted++;
            } else if (strcmp(fields[0], "ConnsTotal:") == 0) {
                metric_family_append(&ctx->fams[FAM_APACHE_CONNECTIONS],
                                     VALUE_GAUGE(atol(fields[1])), &ctx->labels,
                                     &(label_pair_const_t){.name="state", .value="total"}, NULL);
            } else if (strcmp(fields[0], "ConnsAsyncWriting:") == 0) {
                metric_family_append(&ctx->fams[FAM_APACHE_CONNECTIONS],
                                     VALUE_GAUGE(atol(fields[1])), &ctx->labels,
                                     &(label_pair_const_t){.name="state", .value="writing"}, NULL);
            } else if (strcmp(fields[0], "ConnsAsyncKeepAlive:") == 0) {
                metric_family_append(&ctx->fams[FAM_APACHE_CONNECTIONS],
                                     VALUE_GAUGE(atol(fields[1])), &ctx->labels,
                                     &(label_pair_const_t){.name="state", .value="keepalive"}, NULL);
            } else if (strcmp(fields[0], "ConnsAsyncClosing:") == 0) {
                metric_family_append(&ctx->fams[FAM_APACHE_CONNECTIONS],
                                     VALUE_GAUGE(atol(fields[1])), &ctx->labels,
                                     &(label_pair_const_t){.name="state", .value="closing"}, NULL);
            } else if (strcmp(fields[0], "Processes:") == 0) {
                metric_family_append(&ctx->fams[FAM_APACHE_PROCESSES],
                                     VALUE_GAUGE(atol(fields[1])), &ctx->labels, NULL);
            } else if (strcmp(fields[0], "ServerUptimeSeconds:") == 0) {
                metric_family_append(&ctx->fams[FAM_APACHE_UPTIME],
                                     VALUE_GAUGE(atol(fields[1])), &ctx->labels, NULL);
            }

        }
    }

    ctx->apache_buffer_fill = 0;

    plugin_dispatch_metric_family_array_filtered(ctx->fams, FAM_APACHE_MAX, ctx->filter, submit);

    return 0;
}

static int apache_config_instance (config_item_t *ci)
{
    apache_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    memcpy(ctx->fams, fams, sizeof(ctx->fams[0])*FAM_APACHE_MAX);

    int status = cf_util_get_string(ci, &ctx->name);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        free(ctx);
        return status;
    }
    assert(ctx->name != NULL);

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("url", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->url);
        } else if (strcasecmp("user", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->user);
        } else if (strcasecmp("user-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &ctx->user);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->pass);
        } else if (strcasecmp("password-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &ctx->pass);
        } else if (strcasecmp("verify-peer", child->key) == 0) {
            status = cf_util_get_boolean(child, &ctx->verify_peer);
        } else if (strcasecmp("verify-host", child->key) == 0) {
            status = cf_util_get_boolean(child, &ctx->verify_host);
        } else if (strcasecmp("ca-cert", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->cacert);
        } else if (strcasecmp("ssl-ciphers", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->ssl_ciphers);
        } else if (strcasecmp("server", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->server);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &ctx->timeout);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &ctx->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp(child->key, "filter") == 0) {
            status = plugin_filter_configure(child, &ctx->filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    /* Check if struct is complete.. */
    if ((status == 0) && (ctx->url == NULL)) {
        PLUGIN_ERROR("Instance '%s': No 'url' has been configured.", ctx->name);
        status = -1;
    }

    if (status != 0) {
        apache_free(ctx);
        return -1;
    }

    label_set_add(&ctx->labels, true, "instance", ctx->name);

    return plugin_register_complex_read("apache", ctx->name, apache_read, interval,
                                        &(user_data_t){.data = ctx, .free_func = apache_free});
}

static int apache_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = apache_config_instance(child);
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

static int apache_init(void)
{
    /* Call this while ncollectd is still single-threaded to avoid
     * initialization issues in libgcrypt. */
    curl_global_init(CURL_GLOBAL_SSL);
    return 0;
}

void module_register(void)
{
    plugin_register_config("apache", apache_config);
    plugin_register_init("apache", apache_init);
}
