// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2006-2010 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2008 Sebastian Harl
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <curl/curl.h>

enum {
    FAM_NGINX_UP,
    FAM_NGINX_CONNECTIONS_ACTIVE,
    FAM_NGINX_CONNECTIONS_ACCEPTED,
    FAM_NGINX_CONNECTIONS_HANDLED,
    FAM_NGINX_CONNECTIONS_READING,
    FAM_NGINX_CONNECTIONS_WAITING,
    FAM_NGINX_CONNECTIONS_WRITING,
    FAM_NGINX_HTTP_REQUESTS,
    FAM_NGINX_MAX
};

static metric_family_t fams[FAM_NGINX_MAX] = {
    [FAM_NGINX_UP] = {
        .name = "nginx_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the nginx server be reached.",
    },
    [FAM_NGINX_CONNECTIONS_ACTIVE] = {
        .name = "nginx_connections_active",
        .type = METRIC_TYPE_GAUGE,
        .help = "Active client connections.",
    },
    [FAM_NGINX_CONNECTIONS_ACCEPTED] = {
        .name = "nginx_connections_accepted",
        .type = METRIC_TYPE_COUNTER,
        .help = "Accepted client connections.",
    },
    [FAM_NGINX_CONNECTIONS_HANDLED] = {
        .name = "nginx_connections_handled",
        .type = METRIC_TYPE_COUNTER,
        .help = "Handled client connections.",
    },
    [FAM_NGINX_CONNECTIONS_READING] = {
        .name = "nginx_connections_reading",
        .type = METRIC_TYPE_GAUGE,
        .help = "Connections where NGINX is reading the request header.",
    },
    [FAM_NGINX_CONNECTIONS_WAITING] = {
        .name = "nginx_connections_waiting",
        .type = METRIC_TYPE_GAUGE,
        .help = "Idle client connections.",
    },
    [FAM_NGINX_CONNECTIONS_WRITING] = {
        .name = "nginx_connections_writing",
        .type = METRIC_TYPE_GAUGE,
        .help = "Connections where NGINX is writing the response back to the client.",
    },
    [FAM_NGINX_HTTP_REQUESTS] = {
        .name = "nginx_http_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total http requests.",
    },
};

typedef struct {
    char *name;
    char *url;
    char *socket_path;
    char *user;
    char *pass;
    bool verify_peer;
    bool verify_host;
    char *cacert;
    char *ssl_ciphers;
    cdtime_t timeout;
    label_set_t labels;
    plugin_filter_t *filter;
    char nginx_buffer[16384];
    size_t nginx_buffer_len;
    char nginx_curl_error[CURL_ERROR_SIZE];
    CURL *curl;
    metric_family_t fams[FAM_NGINX_MAX];
} nginx_t;

static void nginx_free(void *arg) {
    nginx_t *st = arg;

    if (st == NULL)
        return;

    free(st->name);
    free(st->url);
    free(st->socket_path);
    free(st->user);
    free(st->pass);
    free(st->cacert);
    free(st->ssl_ciphers);
    label_set_reset(&st->labels);
    plugin_filter_free(st->filter);

    if (st->curl) {
        curl_easy_cleanup(st->curl);
        st->curl = NULL;
    }
    free(st);
}

static size_t nginx_curl_callback(void *buf, size_t size, size_t nmemb, void *user_data)
{
    nginx_t *st = user_data;
    if (unlikely(st == NULL)) {
        PLUGIN_ERROR("nginx_curl_callback: user_data pointer is NULL.");
        return 0;
    }

    size_t len = size * nmemb;
    if (unlikely(len == 0))
        return len;

    /* Check if the data fits into the memory. If not, truncate it. */
    if ((st->nginx_buffer_len + len) >= sizeof(st->nginx_buffer)) {
        assert(sizeof(st->nginx_buffer) > st->nginx_buffer_len);
        len = (sizeof(st->nginx_buffer) - 1) - st->nginx_buffer_len;
    }

    memcpy(&st->nginx_buffer[st->nginx_buffer_len], buf, len);
    st->nginx_buffer_len += len;
    st->nginx_buffer[st->nginx_buffer_len] = 0;

    return len;
}

static int nginx_init_host(nginx_t *st)
{
    if (unlikely(st->curl != NULL))
        curl_easy_cleanup(st->curl);

    st->curl = curl_easy_init();
    if (unlikely(st->curl == NULL)) {
        PLUGIN_ERROR("curl_easy_init failed.");
        return -1;
    }

    CURLcode rcode = 0;

    rcode = curl_easy_setopt(st->curl, CURLOPT_NOSIGNAL, 1L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_NOSIGNAL failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(st->curl, CURLOPT_WRITEFUNCTION, nginx_curl_callback);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEFUNCTION failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(st->curl, CURLOPT_WRITEDATA, st);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEDATA failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(st->curl, CURLOPT_USERAGENT, NCOLLECTD_USERAGENT);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERAGENT failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(st->curl, CURLOPT_ERRORBUFFER, st->nginx_curl_error);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_ERRORBUFFER failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }


    if (st->user != NULL) {
#ifdef HAVE_CURLOPT_USERNAME
        rcode = curl_easy_setopt(st->curl, CURLOPT_USERNAME, st->user);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERNAME failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(st->curl, CURLOPT_PASSWORD, (st->pass == NULL) ? "" : st->pass);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_PASSWORD failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

#else
        static char credentials[1024];
        int status = ssnprintf(credentials, sizeof(credentials), "%s:%s", st->user,
                                            st->pass == NULL ? "" : st->pass);
        if ((status < 0) || (status >= (int)sizeof(credentials))) {
            PLUGIN_ERROR("Credentials would have been truncated.");
            curl_easy_cleanup(st->curl);
            st->curl = NULL;
            return -1;
        }
        rcode = curl_easy_setopt(st->curl, CURLOPT_USERPWD, credentials);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERPWD failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

#endif
    }

    rcode = curl_easy_setopt(st->curl, CURLOPT_FOLLOWLOCATION, 1L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_FOLLOWLOCATION failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(st->curl, CURLOPT_MAXREDIRS, 50L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_MAXREDIRS failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(st->curl, CURLOPT_SSL_VERIFYPEER, (long)st->verify_peer);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_SSL_VERIFYPEER failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(st->curl, CURLOPT_SSL_VERIFYHOST, st->verify_host ? 2L : 0L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_SSL_VERIFYHOST failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    if (st->cacert != NULL) {
        rcode = curl_easy_setopt(st->curl, CURLOPT_CAINFO, st->cacert);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_CAINFO failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }
    if (st->ssl_ciphers != NULL) {
        rcode = curl_easy_setopt(st->curl, CURLOPT_SSL_CIPHER_LIST, st->ssl_ciphers);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_SSL_CIPHER_LIST failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }

#ifdef HAVE_CURLOPT_TIMEOUT_MS
    if (st->timeout != 0) {
        rcode = curl_easy_setopt(st->curl, CURLOPT_TIMEOUT_MS,
                                   (long)CDTIME_T_TO_MS(st->timeout));
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_TIMEOUT_MS failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    } else {
        rcode = curl_easy_setopt(st->curl, CURLOPT_TIMEOUT_MS,
                                   (long)CDTIME_T_TO_MS(plugin_get_interval()));
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_TIMEOUT_MS failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }
#endif

#ifdef HAVE_CURLOPT_UNIX_SOCKET_PATH
    if (st->socket_path != NULL) {
        rcode = curl_easy_setopt(st->curl, CURLOPT_UNIX_SOCKET_PATH, st->socket_path);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_UNIX_SOCKET_PATH failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }
#endif
    return 0;
}

static int nginx_read(user_data_t *user_data)
{
    nginx_t *st = user_data->data;

    if (unlikely(st->curl == NULL)) {
        if (unlikely(nginx_init_host(st) != 0)) {
            metric_family_append(&st->fams[FAM_NGINX_UP], VALUE_GAUGE(0), &st->labels, NULL);
            plugin_dispatch_metric_family_filtered(&st->fams[FAM_NGINX_UP], st->filter, 0);
            return 0;
        }
    }

    st->nginx_buffer_len = 0;
    CURLcode rcode = curl_easy_setopt(st->curl, CURLOPT_URL, st->url);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_URL failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    if (curl_easy_perform(st->curl) != CURLE_OK) {
        PLUGIN_WARNING("curl_easy_perform failed: %s", st->nginx_curl_error);
        metric_family_append(&st->fams[FAM_NGINX_UP], VALUE_GAUGE(0), &st->labels, NULL);
        plugin_dispatch_metric_family_filtered(&st->fams[FAM_NGINX_UP], st->filter, 0);
        return 0;
    }

    metric_family_append(&st->fams[FAM_NGINX_UP], VALUE_GAUGE(1), &st->labels, NULL);

    char *lines[16];
    int lines_num = 0;
    char *ptr = st->nginx_buffer;
    char *saveptr = NULL;
    while ((lines[lines_num] = strtok_r(ptr, "\n\r", &saveptr)) != NULL) {
        ptr = NULL;
        lines_num++;

        if (lines_num >= 16)
            break;
    }

    /*
     * Active connections: 291
     * server accepts handled requests
     *  101059015 100422216 347910649
     * Reading: 6 Writing: 179 Waiting: 106
     */

    char *fields[16];
    for (int i = 0; i < lines_num; i++) {
        int fields_num = strsplit(lines[i], fields, (sizeof(fields) / sizeof(fields[0])));

        if (fields_num == 3) {
            if ((strcmp(fields[0], "Active") == 0) && (strcmp(fields[1], "connections:") == 0)) {
                metric_family_append(&st->fams[FAM_NGINX_CONNECTIONS_ACTIVE],
                                     VALUE_GAUGE((double)atoll(fields[2])), &st->labels, NULL);
            } else if ((atoll(fields[0]) != 0) && (atoll(fields[1]) != 0) &&
                       (atoll(fields[2]) != 0)) {
                metric_family_append(&st->fams[FAM_NGINX_CONNECTIONS_ACCEPTED],
                                     VALUE_COUNTER((uint64_t)atoll(fields[0])), &st->labels, NULL);
                metric_family_append(&st->fams[FAM_NGINX_CONNECTIONS_HANDLED],
                                     VALUE_COUNTER((uint64_t)atoll(fields[1])), &st->labels, NULL);
                metric_family_append(&st->fams[FAM_NGINX_HTTP_REQUESTS],
                                     VALUE_COUNTER((uint64_t)atoll(fields[2])), &st->labels, NULL);
            }
        } else if (fields_num == 6) {
            if ((strcmp(fields[0], "Reading:") == 0) && (strcmp(fields[2], "Writing:") == 0) &&
                (strcmp(fields[4], "Waiting:") == 0)) {
                metric_family_append(&st->fams[FAM_NGINX_CONNECTIONS_READING],
                                     VALUE_GAUGE((double)atoll(fields[1])), &st->labels, NULL);
                metric_family_append(&st->fams[FAM_NGINX_CONNECTIONS_WRITING],
                                     VALUE_GAUGE((double)atoll(fields[3])), &st->labels, NULL);
                metric_family_append(&st->fams[FAM_NGINX_CONNECTIONS_WAITING],
                                     VALUE_GAUGE((double)atoll(fields[5])), &st->labels, NULL);
            }
        }
    }

    st->nginx_buffer_len = 0;
    plugin_dispatch_metric_family_array_filtered(st->fams, FAM_NGINX_MAX, st->filter, 0);
    return 0;
}

static int nginx_config_instance(config_item_t *ci)
{
    nginx_t *st = calloc(1, sizeof(*st));
    if (st == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = cf_util_get_string(ci, &st->name);
    if (status != 0) {
        free(st);
        return status;
    }
    assert(st->name != NULL);

    memcpy(st->fams, fams, sizeof(st->fams[0])*FAM_NGINX_MAX);

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("url", child->key) == 0) {
            status = cf_util_get_string(child, &st->url);
        } else if (strcasecmp("socket-path", child->key) == 0) {
            status = cf_util_get_string(child, &st->socket_path);
        } else if (strcasecmp("user", child->key) == 0) {
            status = cf_util_get_string(child, &st->user);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &st->pass);
        } else if (strcasecmp("verify-peer", child->key) == 0) {
            status = cf_util_get_boolean(child, &st->verify_peer);
        } else if (strcasecmp("verify-host", child->key) == 0) {
            status = cf_util_get_boolean(child, &st->verify_host);
        } else if (strcasecmp("ca-cert", child->key) == 0) {
            status = cf_util_get_string(child, &st->cacert);
        } else if (strcasecmp("ssl-ciphers", child->key) == 0) {
            status = cf_util_get_string(child, &st->ssl_ciphers);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &st->timeout);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &st->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &st->filter);
        } else {
            PLUGIN_ERROR("Option `%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }
    /* Check if struct is complete.. */
    if ((status == 0) && (st->url == NULL)) {
        PLUGIN_ERROR("Instance `%s': No URL has been configured.", st->name);
        status = -1;
    }

    if (status != 0) {
        nginx_free(st);
        return -1;
    }

    label_set_add(&st->labels, true, "instance", st->name);

    return plugin_register_complex_read("nginx", st->name, nginx_read, interval,
                                        &(user_data_t){.data = st, .free_func=nginx_free});
}

static int nginx_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = nginx_config_instance(child);
        } else {
            PLUGIN_ERROR("The configuration option '%s' is not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int nginx_init(void)
{
    /* Call this while ncollectd is still single-threaded to avoid
     * initialization issues in libgcrypt. */
    curl_global_init(CURL_GLOBAL_SSL);
    return 0;
}

void module_register(void)
{
    plugin_register_config("nginx", nginx_config);
    plugin_register_init("nginx", nginx_init);
}
