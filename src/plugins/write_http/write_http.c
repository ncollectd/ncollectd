// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2009 Paul Sadauskas
// SPDX-FileCopyrightText: Copyright (C) 2009 Doug MacEachern
// SPDX-FileCopyrightText: Copyright (C) 2007-2020 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Doug MacEachern <dougm at hyperic.com>
// SPDX-FileContributor: Paul Sadauskas <psadauskas at gmail.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libformat/format.h"
#include "libcompress/compress.h"

#include <curl/curl.h>

#include "curl_stats.h"

#ifndef WRITE_HTTP_RESPONSE_BUFFER_SIZE
#define WRITE_HTTP_RESPONSE_BUFFER_SIZE 1024
#endif

struct wh_callback_s {
    char *name;

    char *location;
    char *user;
    char *pass;
    char *credentials;
    bool verify_peer;
    bool verify_host;
    char *cacert;
    char *capath;
    char *clientkey;
    char *clientcert;
    char *clientkeypass;
    long sslversion;
    bool store_rates;
    bool log_http_error;
    int low_speed_limit;
    time_t low_speed_time;
    int timeout;

    cdtime_t flush_timeout;

    format_stream_metric_t format_metric;
    format_notification_t format_notification;
    compress_format_t compress;
    const char *content_type;

    CURL *curl;
    uint64_t curl_stats_flags;
    struct curl_slist *headers;
    char curl_errbuf[CURL_ERROR_SIZE];

    unsigned int send_buffer_max;
    strbuf_t send_buffer;
    cdtime_t send_buffer_init_time;

    char response_buffer[WRITE_HTTP_RESPONSE_BUFFER_SIZE];
    unsigned int response_buffer_pos;
};
typedef struct wh_callback_s wh_callback_t;

/* libcurl may call this multiple times depending on how big the server's
 * http response is */
static size_t wh_curl_write_callback(char *ptr, __attribute__((unused)) size_t size,
                                     size_t nmemb, void *userdata)
{
    wh_callback_t *cb = (wh_callback_t *)userdata;
    unsigned int len = 0;

    if ((cb->response_buffer_pos + nmemb) > sizeof(cb->response_buffer))
        len = sizeof(cb->response_buffer) - cb->response_buffer_pos;
    else
        len = nmemb;

    PLUGIN_DEBUG("curl callback nmemb=%zu buffer_pos=%u write_len=%u ",
                 nmemb, cb->response_buffer_pos, len);

    memcpy(cb->response_buffer + cb->response_buffer_pos, ptr, len);
    cb->response_buffer_pos += len;
    cb->response_buffer[sizeof(cb->response_buffer) - 1] = '\0';

    /* Always return nmemb even if we write less so libcurl won't throw an error */
    return nmemb;
}

static void wh_log_http_error(wh_callback_t *cb)
{
    if (!cb->log_http_error)
        return;

    long http_code = 0;
    curl_easy_getinfo(cb->curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (http_code != 200)
        PLUGIN_INFO("HTTP Error code: %ld", http_code);
}

static int wh_post(wh_callback_t *cb, char *data, size_t data_len)
{
    if ((data == NULL) || (data_len == 0))
        return 0;

    size_t post_data_len = 0;
    char *post_data = compress(cb->compress, data, data_len, &post_data_len);
    if (post_data == NULL)
        return -1;

    CURLcode rcode = 0;

    rcode = curl_easy_setopt(cb->curl, CURLOPT_URL, cb->location);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_URL failed: %s", curl_easy_strerror(rcode));
        return -1;
    }

    curl_off_t curl_post_data_len = post_data_len;
    rcode = curl_easy_setopt(cb->curl, CURLOPT_POSTFIELDSIZE_LARGE, curl_post_data_len);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_POSTFIELDSIZE_LARGE failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(cb->curl, CURLOPT_POSTFIELDS, post_data);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_POSTFIELDS failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(cb->curl, CURLOPT_WRITEFUNCTION, wh_curl_write_callback);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEFUNCTION failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(cb->curl, CURLOPT_WRITEDATA, (void *)cb);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEDATA failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    int status = curl_easy_perform(cb->curl);

    wh_log_http_error(cb);

    if (cb->curl_stats_flags != 0) {
        label_set_t labels = {
            .ptr = (label_pair_t[]) {{.name = "instance", .value = cb->name }},
            .num = 1,
        };
        int rc = curl_stats_dispatch(cb->curl, cb->curl_stats_flags, "write_http", &labels);
        if (rc != 0)
            PLUGIN_ERROR("curl_stats_dispatch failed with status %d", rc);
    }

    if (status != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_perform failed with status %d: %s", status, cb->curl_errbuf);
        if (strlen(cb->response_buffer) > 0)
            PLUGIN_ERROR("curl_response: %s", cb->response_buffer);
    } else {
        PLUGIN_DEBUG("curl_response: %s", cb->response_buffer);
    }

    compress_free(cb->compress, post_data);

    return status;
}

static int wh_callback_init(wh_callback_t *cb)
{
    if (cb->curl != NULL)
        return 0;

    cb->curl = curl_easy_init();
    if (cb->curl == NULL) {
        PLUGIN_ERROR("curl_easy_init failed.");
        return -1;
    }

    CURLcode rcode = 0;

    if (cb->low_speed_limit > 0 && cb->low_speed_time > 0) {
        rcode = curl_easy_setopt(cb->curl, CURLOPT_LOW_SPEED_LIMIT,
                                   (long)(cb->low_speed_limit * cb->low_speed_time));
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_LOW_SPEED_LIMIT failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
        rcode = curl_easy_setopt(cb->curl, CURLOPT_LOW_SPEED_TIME, (long)cb->low_speed_time);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_LOW_SPEED_TIME failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }

#ifdef HAVE_CURLOPT_TIMEOUT_MS
    if (cb->timeout > 0) {
        rcode = curl_easy_setopt(cb->curl, CURLOPT_TIMEOUT_MS, (long)cb->timeout);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_TIMEOUT_MS failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }
#endif

    rcode = curl_easy_setopt(cb->curl, CURLOPT_NOSIGNAL, 1L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_NOSIGNAL failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(cb->curl, CURLOPT_USERAGENT, NCOLLECTD_USERAGENT);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERAGENT failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    cb->headers = curl_slist_append(cb->headers, "Accept:  */*");

    if (cb->content_type != NULL) {
        char buffer[256];
        ssnprintf(buffer, sizeof(buffer), "Content-Type: %s", cb->content_type);
        cb->headers = curl_slist_append(cb->headers, buffer);
    }

    const char *encoding = compress_get_encoding(cb->compress);
    if (encoding != NULL) {
        char buffer[256];
        ssnprintf(buffer, sizeof(buffer), "Content-Encoding: %s", encoding);
        cb->headers = curl_slist_append(cb->headers, buffer);
    }

    cb->headers = curl_slist_append(cb->headers, "Expect:");
    rcode = curl_easy_setopt(cb->curl, CURLOPT_HTTPHEADER, cb->headers);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_HTTPHEADER failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(cb->curl, CURLOPT_ERRORBUFFER, cb->curl_errbuf);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_ERRORBUFFER failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(cb->curl, CURLOPT_FOLLOWLOCATION, 1L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_FOLLOWLOCATION failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(cb->curl, CURLOPT_MAXREDIRS, 50L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_MAXREDIRS failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    if (cb->user != NULL) {
#ifdef HAVE_CURLOPT_USERNAME
        rcode = curl_easy_setopt(cb->curl, CURLOPT_USERNAME, cb->user);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERNAME failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
        rcode = curl_easy_setopt(cb->curl, CURLOPT_PASSWORD, (cb->pass == NULL) ? "" : cb->pass);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_PASSWORD failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
#else
        size_t credentials_size;

        credentials_size = strlen(cb->user) + 2;
        if (cb->pass != NULL)
            credentials_size += strlen(cb->pass);

        cb->credentials = malloc(credentials_size);
        if (cb->credentials == NULL) {
            ERROR("curl plugin: malloc failed.");
            return -1;
        }

        snprintf(cb->credentials, credentials_size, "%s:%s", cb->user,
                         (cb->pass == NULL) ? "" : cb->pass);
        rcode = curl_easy_setopt(cb->curl, CURLOPT_USERPWD, cb->credentials);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERPWD failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
#endif
        rcode = curl_easy_setopt(cb->curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_HTTPAUTH failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }

    rcode = curl_easy_setopt(cb->curl, CURLOPT_SSL_VERIFYPEER, (long)cb->verify_peer);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_SSL_VERIFYPEER failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(cb->curl, CURLOPT_SSL_VERIFYHOST, cb->verify_host ? 2L : 0L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_SSL_VERIFYHOST failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(cb->curl, CURLOPT_SSLVERSION, cb->sslversion);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_SSLVERSION failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    if (cb->cacert != NULL) {
        rcode = curl_easy_setopt(cb->curl, CURLOPT_CAINFO, cb->cacert);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_CAINFO failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }
    if (cb->capath != NULL) {
        rcode = curl_easy_setopt(cb->curl, CURLOPT_CAPATH, cb->capath);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_CAPATH failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }

    if (cb->clientkey != NULL && cb->clientcert != NULL) {
        rcode = curl_easy_setopt(cb->curl, CURLOPT_SSLKEY, cb->clientkey);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_SSLKEY failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
        rcode = curl_easy_setopt(cb->curl, CURLOPT_SSLCERT, cb->clientcert);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_SSLCERT failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        if (cb->clientkeypass != NULL) {
            rcode = curl_easy_setopt(cb->curl, CURLOPT_SSLKEYPASSWD, cb->clientkeypass);
            if (rcode != CURLE_OK) {
                PLUGIN_ERROR("curl_easy_setopt CURLOPT_SSLKEYPASSWD failed: %s",
                             curl_easy_strerror(rcode));
                return -1;
            }
        }
    }

    return 0;
}

static int wh_flush_internal(wh_callback_t *cb, cdtime_t timeout)
{
    if (wh_callback_init(cb) != 0) {
        PLUGIN_ERROR("wh_callback_init failed.");
        return -1;
    }

    if (strbuf_len(&cb->send_buffer) == 0)
        return 0;

    /* timeout == 0  => flush unconditionally */
    if (timeout > 0) {
        if ((cb->send_buffer_init_time + timeout) > cdtime())
            return 0;
    }

    int status = wh_post(cb, cb->send_buffer.ptr, strbuf_len(&cb->send_buffer));

    strbuf_reset(&cb->send_buffer);

    return status;
}

static int wh_flush(cdtime_t timeout, user_data_t *user_data)
{
    if (user_data == NULL)
        return -EINVAL;

    wh_callback_t *cb = user_data->data;

    return wh_flush_internal(cb, timeout);
}

static void wh_callback_free(void *data)
{
    if (data == NULL)
        return;

    wh_callback_t *cb = data;

    wh_flush_internal(cb, 0);

    if (cb->curl != NULL) {
        curl_easy_cleanup(cb->curl);
        cb->curl = NULL;
    }

    if (cb->headers != NULL) {
        curl_slist_free_all(cb->headers);
        cb->headers = NULL;
    }

    strbuf_destroy(&cb->send_buffer);

    free(cb->name);
    free(cb->location);
    free(cb->user);
    free(cb->pass);
    free(cb->credentials);
    free(cb->cacert);
    free(cb->capath);
    free(cb->clientkey);
    free(cb->clientcert);
    free(cb->clientkeypass);

    free(cb);
}

static int wh_write(metric_family_t const *fam, user_data_t *user_data)
{
    if ((fam == NULL) || (user_data == NULL))
        return EINVAL;

    wh_callback_t *cb = user_data->data;

    if (strbuf_len(&cb->send_buffer) >= cb->send_buffer_max) {
        int status = wh_flush_internal(cb, 0);
        if (status != 0)
            return -1;
    }

    if (strbuf_len(&cb->send_buffer) == 0)
        cb->send_buffer_init_time = cdtime();

    format_stream_metric_ctx_t ctx = {0};

    int status = format_stream_metric_begin(&ctx, cb->format_metric, &cb->send_buffer);
    status |= format_stream_metric_family(&ctx, fam);
    status |= format_stream_metric_end(&ctx);

    return wh_flush_internal(cb, cb->flush_timeout);
}

static int wh_notify(notification_t const *n, user_data_t *ud)
{
    wh_callback_t *cb;

    if ((ud == NULL) || (ud->data == NULL))
        return EINVAL;

    cb = ud->data;

    strbuf_t buf = STRBUF_CREATE;

    int status = format_notification(cb->format_notification, &buf, n);
    if (status != 0) {
        PLUGIN_ERROR("formatting notification failed");
        strbuf_destroy(&buf);
        return status;
    }

    if (wh_callback_init(cb) != 0) {
        PLUGIN_ERROR("wh_callback_init failed.");
        strbuf_destroy(&buf);
        return -1;
    }

    status = wh_post(cb, buf.ptr, strbuf_len(&buf));

    strbuf_destroy(&buf);

    return status;
}

static int wh_config_append_string(const char *name, struct curl_slist **dest, config_item_t *ci)
{
    struct curl_slist *temp = NULL;
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("`%s' needs exactly one string argument.", name);
        return -1;
    }

    temp = curl_slist_append(*dest, ci->values[0].value.string);
    if (temp == NULL)
        return -1;

    *dest = temp;

    return 0;
}

static int wh_config_ssl_version(config_item_t *ci, long *sslversion)
{
    if ((ci == NULL) || (sslversion == NULL))
        return EINVAL;

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if (strcasecmp(ci->values[0].value.string, "default") == 0) {
        *sslversion = CURL_SSLVERSION_DEFAULT;
    } else if (strcasecmp(ci->values[0].value.string, "sslv2") == 0) {
        *sslversion = CURL_SSLVERSION_SSLv2;
    } else if (strcasecmp(ci->values[0].value.string, "sslv3") == 0) {
        *sslversion = CURL_SSLVERSION_SSLv3;
    } else if (strcasecmp(ci->values[0].value.string, "tlsv1") == 0) {
        *sslversion = CURL_SSLVERSION_TLSv1;
#if (LIBCURL_VERSION_MAJOR > 7) ||  (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR >= 34)
    } else if (strcasecmp(ci->values[0].value.string, "tlsv1_0") == 0) {
        *sslversion = CURL_SSLVERSION_TLSv1_0;
    } else if (strcasecmp(ci->values[0].value.string, "tlsv1_1") == 0) {
        *sslversion = CURL_SSLVERSION_TLSv1_1;
    } else if (strcasecmp(ci->values[0].value.string, "tlsv1_2") == 0) {
        *sslversion = CURL_SSLVERSION_TLSv1_2;
#endif
#if (LIBCURL_VERSION_MAJOR > 7) ||  (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR >= 34)
    } else if (strcasecmp(ci->values[0].value.string, "tlsv1_3") == 0) {
        *sslversion = CURL_SSLVERSION_TLSv1_3;
#endif
    } else {
        PLUGIN_ERROR("Invalid SSLVersion option: %s.", ci->values[0].value.string);
        return EINVAL;
    }

    return 0;
}

static int wh_config_instance(config_item_t *ci)
{
    wh_callback_t *cb = calloc(1, sizeof(*cb));
    if (cb == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = cf_util_get_string(ci, &cb->name);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        free(cb);
        return -1;
    }

    cb->verify_peer = true;
    cb->verify_host = true;
    cb->format_metric = FORMAT_STREAM_METRIC_OPENMETRICS_TEXT,
    cb->format_notification = FORMAT_NOTIFICATION_JSON;
    cb->sslversion = CURL_SSLVERSION_DEFAULT;
    cb->low_speed_limit = 0;
    cb->timeout = 0;
    cb->log_http_error = false;
    cb->headers = NULL;
    cb->send_buffer_max = 65536;
    cb->flush_timeout = plugin_get_interval()/2;

    cf_send_t send = SEND_METRICS;

    cdtime_t flush_interval = plugin_get_interval();
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("url", child->key) == 0) {
            status = cf_util_get_string(child, &cb->location);
        } else if (strcasecmp("user", child->key) == 0) {
            status = cf_util_get_string(child, &cb->user);
        } else if (strcasecmp("user-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &cb->user);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &cb->pass);
        } else if (strcasecmp("password-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &cb->pass);
        } else if (strcasecmp("verify-peer", child->key) == 0) {
            status = cf_util_get_boolean(child, &cb->verify_peer);
        } else if (strcasecmp("verify-host", child->key) == 0) {
            status = cf_util_get_boolean(child, &cb->verify_host);
        } else if (strcasecmp("ca-cert", child->key) == 0) {
            status = cf_util_get_string(child, &cb->cacert);
        } else if (strcasecmp("ca-path", child->key) == 0) {
            status = cf_util_get_string(child, &cb->capath);
        } else if (strcasecmp("client-key", child->key) == 0) {
            status = cf_util_get_string(child, &cb->clientkey);
        } else if (strcasecmp("client-cert", child->key) == 0) {
            status = cf_util_get_string(child, &cb->clientcert);
        } else if (strcasecmp("client-key-pass", child->key) == 0) {
            status = cf_util_get_string(child, &cb->clientkeypass);
        } else if (strcasecmp("ssl-version", child->key) == 0) {
            status = wh_config_ssl_version(child, &cb->sslversion);
        } else if (strcasecmp("format-metric", child->key) == 0) {
            status = config_format_stream_metric(child, &cb->format_metric);
        } else if (strcasecmp("format-notification", child->key) == 0) {
            status = config_format_notification(child, &cb->format_notification);
        } else if (strcasecmp("compress", child->key) == 0) {
            status = config_compress(child, &cb->compress);
        } else if (strcasecmp("collect", child->key) == 0) {
            status = curl_stats_from_config(child, &cb->curl_stats_flags);
        } else if (strcasecmp("store-rates", child->key) == 0) {
            status = cf_util_get_boolean(child, &cb->store_rates);
        } else if (strcasecmp("buffer-size", child->key) == 0) {
            status = cf_util_get_unsigned_int(child, &cb->send_buffer_max);
        } else if (strcasecmp("low-speed-limit", child->key) == 0) {
            status = cf_util_get_int(child, &cb->low_speed_limit);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_int(child, &cb->timeout);
        } else if (strcasecmp("log-http-error", child->key) == 0) {
            status = cf_util_get_boolean(child, &cb->log_http_error);
        } else if (strcasecmp("header", child->key) == 0) {
            status = wh_config_append_string("Header", &cb->headers, child);
        } else if (strcasecmp("flush-interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &flush_interval);
        } else if (strcasecmp("flush-timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &cb->flush_timeout);
        } else if (strcasecmp("write", child->key) == 0) {
            status = cf_uti_get_send(child, &send);
        } else {
            PLUGIN_ERROR("Invalid configuration option: %s.", child->key);
            status = EINVAL;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        wh_callback_free(cb);
        return status;
    }

    if (cb->location == NULL) {
        PLUGIN_ERROR("no URL defined for instance '%s'", cb->name);
        wh_callback_free(cb);
        return -1;
    }

    if (send == SEND_NOTIFICATIONS)
        cb->content_type = format_notification_content_type(cb->format_notification);
    else
        cb->content_type = format_stream_metric_content_type(cb->format_metric);

    if (cb->low_speed_limit > 0)
        cb->low_speed_time = CDTIME_T_TO_TIME_T(plugin_get_interval());

    cb->send_buffer = STRBUF_CREATE;

    PLUGIN_DEBUG("Registering write callback 'write_http/%s' with URL '%s'",
                 cb->name, cb->location);

    user_data_t user_data = { .data = cb, .free_func = wh_callback_free };

    if (send == SEND_NOTIFICATIONS)
        return plugin_register_notification("write_http", cb->name, wh_notify, &user_data);

    return plugin_register_write("write_http", cb->name, wh_write,
                                  wh_flush, flush_interval, cb->flush_timeout, &user_data);
}

static int wh_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = wh_config_instance(child);
        } else {
            PLUGIN_ERROR("Invalid configuration option: %s.", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int wh_init(void)
{
    /* Call this while ncollectd is still single-threaded to avoid
     * initialization issues in libgcrypt. */
    curl_global_init(CURL_GLOBAL_SSL);
    return 0;
}

void module_register(void)
{
    plugin_register_config("write_http", wh_config);
    plugin_register_init("write_http", wh_init);
}
