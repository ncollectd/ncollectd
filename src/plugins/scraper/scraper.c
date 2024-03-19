// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2006-2009 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2009 Aman Gupta
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Aman Gupta <aman at tmm1.net>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/time.h"

#include <curl/curl.h>

#include "curl_stats.h"

typedef struct {
    cdtime_t time;
    metric_family_t fam;
    strbuf_t buf;
    size_t lineno;
} scraper_parser_t;

typedef struct {
    char *instance;
    char *host;
    char *url;
    char *user;
    char *pass;
    char *credentials;
    bool digest;
    bool verify_peer;
    bool verify_host;
    char *cacert;
    cdtime_t interval;
    int timeout;

    char *metric_prefix;
    size_t metric_prefix_size;
    label_set_t label;


    struct curl_slist *headers;
    char *post_body;
    curl_stats_t *stats;
    CURL *curl;
    char curl_errbuf[CURL_ERROR_SIZE];

    scraper_parser_t parser;
} scraper_instance_t;


int metric_parse_buffer(strbuf_t *buf, char *buffer, size_t buffer_len, size_t *lineno,
                        metric_family_t *fam, const char *prefix, size_t prefix_size,
                        label_set_t *labels, cdtime_t time)
{
    while (buffer_len > 0) {
        char *end = memchr(buffer, '\n', buffer_len);
        if (end != NULL) {
            size_t line_size = end - buffer;
            *lineno += 1;
            if (line_size > 0) { // FIXME
                strbuf_putstrn(buf, buffer, line_size);

                int status = metric_parse_line(fam, plugin_dispatch_metric_family,
                                               prefix, prefix_size, labels, 0, time, buf->ptr);
                if (status < 0)
                    return status;
            }

            strbuf_reset(buf);
            buffer_len -= line_size; // FIXME +1
            buffer = end + 1;              // +1
        } else {
            strbuf_putstrn(buf, buffer, buffer_len);
            buffer_len = 0;
        }
    }

    return 0;
}

static size_t scraper_curl_callback(void *buf, size_t size, size_t nmemb, void *user_data)
{
    scraper_instance_t *target = user_data;
    if (target == NULL) {
        PLUGIN_ERROR("user_data pointer is NULL.");
        return 0;
    }

    size_t len = size * nmemb;
    size_t buf_len = len;

    int status = metric_parse_buffer(&target->parser.buf, buf, buf_len, &target->parser.lineno,
                                     &target->parser.fam,
                                     target->metric_prefix, target->metric_prefix_size,
                                     &target->label, target->parser.time);
    if (status < 0) {

    }

#if 0
    while (buf_len > 0) {
        unsigned char *end = memchr(buf, '\n', buf_len);
        if (end != NULL) {
            size_t line_size = end - (unsigned char *)buf;
            strbuf_putstrn(&target->parser.buf, buf, line_size);

            int status = metric_parse_line(&target->parser.fam,
                                            target->metric_prefix, target->metric_prefix_size,
                                            &target->label, target->parser.time,
                                            target->parser.buf.ptr);
            if (status < 0) {

            }

            strbuf_reset(&target->parser.buf);
            buf_len -= line_size; // FIXME +1
            buf = end + 1;              // +1
        } else {
            strbuf_putstrn(&target->parser.buf, buf, buf_len);
            buf_len = 0;
        }
    }
#endif
    return len;
}

static void scraper_instance_free(void *arg)
{
    scraper_instance_t *target = (scraper_instance_t *)arg;
    if (target == NULL)
        return;

    if (target->curl != NULL)
        curl_easy_cleanup(target->curl);
    target->curl = NULL;

    free(target->instance);
    free(target->url);
    free(target->user);
    free(target->pass);
    free(target->credentials);
    free(target->cacert);
    free(target->post_body);

    curl_slist_free_all(target->headers);
    free(target->metric_prefix);
    label_set_reset(&target->label);

    strbuf_destroy(&target->parser.buf);
    curl_stats_destroy(target->stats);
    free(target);
}

static int scaper_read(user_data_t *ud)
{
    if ((ud == NULL) || (ud->data == NULL)) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }

    scraper_instance_t *target = (scraper_instance_t *)ud->data;

    strbuf_reset(&target->parser.buf);

    CURLcode rcode = curl_easy_setopt(target->curl, CURLOPT_URL, target->url);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_URL failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    int status = curl_easy_perform(target->curl);
    if (status != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_perform failed with status %i: %s (%s)",
                    status, target->curl_errbuf, target->url);
        return -1;
    }

    //  if (target->stats != NULL)
    //      curl_stats_dispatch(target->stats, target->curl, cjq_host(db),
    //      "prometheus", target->instance);

    char *url = NULL;
    curl_easy_getinfo(target->curl, CURLINFO_EFFECTIVE_URL, &url);
    long rc = 0;
    curl_easy_getinfo(target->curl, CURLINFO_RESPONSE_CODE, &rc);
    /* The response code is zero if a non-HTTP transport was used. */
    if ((rc != 0) && (rc != 200)) {
        PLUGIN_ERROR("curl_easy_perform failed with response code %ld (%s)", rc, url);
        return -1;
    }

    /* parse remain line */
    //status = prom_parser(&target->parser, NULL);
    //if (status < 0) {
    //}
    //prom_push_metric(&target->parser, NULL, 0);

    return 0;
}

static int scraper_init_curl(scraper_instance_t *target)
{
    target->curl = curl_easy_init();
    if (target->curl == NULL) {
        PLUGIN_ERROR("curl_easy_init failed.");
        return -1;
    }

    CURLcode rcode = 0;

    rcode = curl_easy_setopt(target->curl, CURLOPT_NOSIGNAL, 1L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_NOSIGNAL failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(target->curl, CURLOPT_WRITEFUNCTION, scraper_curl_callback);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEFUNCTION failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(target->curl, CURLOPT_WRITEDATA, target);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEDATA failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(target->curl, CURLOPT_USERAGENT, NCOLLECTD_USERAGENT);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERAGENT failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(target->curl, CURLOPT_ERRORBUFFER, target->curl_errbuf);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_ERRORBUFFER failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(target->curl, CURLOPT_FOLLOWLOCATION, 1L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_FOLLOWLOCATION failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(target->curl, CURLOPT_MAXREDIRS, 50L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_MAXREDIRS failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }


    if (target->user != NULL) {
#ifdef HAVE_CURLOPT_USERNAME
        rcode = curl_easy_setopt(target->curl, CURLOPT_USERNAME, target->user);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERNAME failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
        rcode = curl_easy_setopt(target->curl, CURLOPT_PASSWORD,
                                               (target->pass == NULL) ? "" : target->pass);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_PASSWORD failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
#else
        size_t credentials_size;

        credentials_size = strlen(target->user) + 2;
        if (target->pass != NULL)
            credentials_size += strlen(target->pass);

        target->credentials = malloc(credentials_size);
        if (target->credentials == NULL) {
            PLUGIN_ERROR("malloc failed.");
            return -1;
        }

        snprintf(target->credentials, credentials_size, "%s:%s", target->user,
                         (target->pass == NULL) ? "" : target->pass);
        rcode = curl_easy_setopt(target->curl, CURLOPT_USERPWD, target->credentials);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERPWD failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
#endif

        if (target->digest) {
            rcode = curl_easy_setopt(target->curl, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST);
            if (rcode != CURLE_OK) {
                PLUGIN_ERROR("curl_easy_setopt CURLOPT_HTTPAUTH failed: %s",
                             curl_easy_strerror(rcode));
                return -1;
            }
        }
    }

    rcode = curl_easy_setopt(target->curl, CURLOPT_SSL_VERIFYPEER,
                                     (long)target->verify_peer);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_SSL_VERIFYPEER failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(target->curl, CURLOPT_SSL_VERIFYHOST,
                                     target->verify_host ? 2L : 0L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_SSL_VERIFYHOST failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    if (target->cacert != NULL) {
        rcode = curl_easy_setopt(target->curl, CURLOPT_CAINFO, target->cacert);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_CAINFO failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }
    if (target->headers != NULL) {
        rcode = curl_easy_setopt(target->curl, CURLOPT_HTTPHEADER, target->headers);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_HTTPHEADER failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }
    if (target->post_body != NULL) {
        rcode = curl_easy_setopt(target->curl, CURLOPT_POSTFIELDS, target->post_body);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_POSTFIELDS failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }

#ifdef HAVE_CURLOPT_TIMEOUT_MS
    if (target->timeout >= 0) {
        rcode = curl_easy_setopt(target->curl, CURLOPT_TIMEOUT_MS, (long)target->timeout);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_TIMEOUT_MS failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    } else if (target->interval > 0) {
        rcode = curl_easy_setopt(target->curl, CURLOPT_TIMEOUT_MS,
                                         (long)CDTIME_T_TO_MS(target->interval));
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_TIMEOUT_MS failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    } else {
        rcode = curl_easy_setopt(target->curl, CURLOPT_TIMEOUT_MS,
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

static int scraper_config_append_string(const char *name, struct curl_slist **dest,
                                        config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("'%s' needs exactly one string argument.", name);
        return -1;
    }

    struct curl_slist *temp = curl_slist_append(*dest, ci->values[0].value.string);
    if (temp == NULL)
        return -1;

    *dest = temp;

    return 0;
}

static int scraper_config_target(config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("The 'instance' block needs exactly one string argument.");
        return -1;
    }

    scraper_instance_t *target = calloc(1, sizeof(scraper_instance_t));
    if (target == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    target->timeout = -1;

    int status = cf_util_get_string(ci, &target->instance);
    if (status != 0) {
        PLUGIN_ERROR("Invalid instance name.");
        scraper_instance_free(target);
        return -1;
    }

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("url", child->key) == 0) {
            status = cf_util_get_string(child, &target->url);
        } else if (strcasecmp("user", child->key) == 0) {
            status = cf_util_get_string(child, &target->user);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &target->pass);
        } else if (strcasecmp("digest", child->key) == 0) {
            status = cf_util_get_boolean(child, &target->digest);
        } else if (strcasecmp("verify-peer", child->key) == 0) {
            status = cf_util_get_boolean(child, &target->verify_peer);
        } else if (strcasecmp("verify-host", child->key) == 0) {
            status = cf_util_get_boolean(child, &target->verify_host);
        } else if (strcasecmp("ca-cert", child->key) == 0) {
            status = cf_util_get_string(child, &target->cacert);
        } else if (strcasecmp("header", child->key) == 0) {
            status = scraper_config_append_string("Header", &target->headers, child);
        } else if (strcasecmp("post", child->key) == 0) {
            status = cf_util_get_string(child, &target->post_body);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &target->interval);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_int(child, &target->timeout);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &target->label);
        } else if (strcasecmp("metric-prefix", child->key) == 0) {
            status = cf_util_get_string(child, &target->metric_prefix);
        } else if (strcasecmp("statistics", child->key) == 0) {
            status = curl_stats_from_config(child, "scraper_", &target->stats);
        } else {
            PLUGIN_WARNING("Option `%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (target->metric_prefix != NULL)
        target->metric_prefix_size = strlen(target->metric_prefix);

    if (status == 0 && target->url)
        status = scraper_init_curl(target);

    if (status != 0) {
        scraper_instance_free(target);
        return -1;
    }

    return plugin_register_complex_read("scraper", target->instance, scaper_read, target->interval,
                                 &(user_data_t){.data = target,.free_func = scraper_instance_free});
}

static int scraper_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = scraper_config_target(child);
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

static int scraper_init(void)
{
    /* Call this while ncollectd is still single-threaded to avoid
     * initialization issues in libgcrypt. */
    curl_global_init(CURL_GLOBAL_SSL);

    return 0;
}

void module_register(void)
{
    plugin_register_config("scraper", scraper_config);
    plugin_register_init("scraper", scraper_init);
}
