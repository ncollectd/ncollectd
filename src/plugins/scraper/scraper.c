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
#include "libutils/socket.h"
#include "libmetric/parser.h"

#include <curl/curl.h>

#include "curl_stats.h"

typedef struct {
    char *instance;

    char *file_path;
    char *socket_path;

    char *url;
    char *user;
    char *pass;
    char *credentials;
    bool digest;
    bool verify_peer;
    bool verify_host;
    char *cacert;

    cdtime_t timeout;

    cdtime_t interval;
    char *metric_prefix;
    label_set_t label;
    plugin_filter_t *filter;

    struct curl_slist *headers;
    char *post_body;
    uint64_t curl_stats_flags;
    CURL *curl;
    char curl_errbuf[CURL_ERROR_SIZE];

    metric_parser_t *mp;

} scraper_instance_t;

static size_t scraper_curl_callback(void *buf, size_t size, size_t nmemb, void *user_data)
{
    scraper_instance_t *target = user_data;
    if (target == NULL) {
        PLUGIN_ERROR("user_data pointer is NULL.");
        return 0;
    }

    size_t len = size * nmemb;
    size_t buf_len = len;

    int status = metric_parse_buffer(target->mp, buf, buf_len);
    if (status < 0) {
        // FIXME
    }

    return len;
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
    if (target->timeout != CDTIME_DOOMSDAY) {
        rcode = curl_easy_setopt(target->curl, CURLOPT_TIMEOUT_MS,
                                               (long)CDTIME_T_TO_MS(target->timeout));
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


static int scaper_read_url(scraper_instance_t *target)
{
    if (target->curl == NULL) {
        if (scraper_init_curl(target) < 0)
            return -1;
    }

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
        metric_parser_reset(target->mp);
        return -1;
    }

    label_set_t lstats = {0};
    label_set_clone(&lstats, target->label);
    label_set_add(&lstats, false, "instance", target->instance);
    curl_stats_dispatch(target->curl, target->curl_stats_flags,
                        target->metric_prefix != NULL ? target->metric_prefix : "scraper_",
                        &lstats);
    label_set_reset(&lstats);

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
        metric_parser_reset(target->mp);
        return -1;
    }

    metric_parser_dispatch(target->mp, plugin_dispatch_metric_family_filtered, target->filter, 0);
    metric_parser_reset(target->mp);

    return 0;
}

static int scaper_read_file(scraper_instance_t *target)
{
    int fd = open(target->file_path, O_RDONLY);
    if (fd < 0) {
        PLUGIN_ERROR("open (%s): %s", target->file_path, STRERRNO);
        return -1;
    }

    char buffer[8192];
    ssize_t len = 0;
    while ((len = read(fd, buffer, sizeof(buffer))) > 0) {
        int status = metric_parse_buffer(target->mp, buffer, len);
        if (status < 0) {
            // FIXME
        }
    }

    close(fd);

    metric_parser_dispatch(target->mp, plugin_dispatch_metric_family_filtered, target->filter, 0);
    metric_parser_reset(target->mp);

    return 0;
}

static int scaper_read_socket(scraper_instance_t *target)
{
    cdtime_t timeout = 0;

    if (target->timeout != CDTIME_DOOMSDAY) {
        timeout = target->timeout;
    } else if (target->interval > 0) {
        timeout = target->interval;
    } else {
        timeout = plugin_get_interval();
    }

    int fd = socket_connect_unix_stream(target->socket_path, timeout);
    if (fd < 0)
        return -1;

    char buffer[8192];
    ssize_t len = 0;
    while ((len = read(fd, buffer, sizeof(buffer))) > 0) {
        int status = metric_parse_buffer(target->mp, buffer, len);
        if (status < 0) {
            // FIXME
        }
    }

    close(fd);

    metric_parser_dispatch(target->mp, plugin_dispatch_metric_family_filtered, target->filter, 0);
    metric_parser_reset(target->mp);

    return 0;
}

static int scaper_read(user_data_t *ud)
{
    if ((ud == NULL) || (ud->data == NULL)) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }

    scraper_instance_t *target = (scraper_instance_t *)ud->data;

    if (target->url != NULL)
        return scaper_read_url(target);

    if (target->file_path != NULL)
        return scaper_read_file(target);

    if (target->socket_path != NULL)
        return scaper_read_socket(target);

    return 0;
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
    free(target->file_path);
    free(target->socket_path);
    free(target->url);
    free(target->user);
    free(target->pass);
    free(target->credentials);
    free(target->cacert);
    free(target->post_body);

    curl_slist_free_all(target->headers);
    free(target->metric_prefix);
    label_set_reset(&target->label);
    plugin_filter_free(target->filter);

    metric_parser_free(target->mp);

    free(target);
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

static int scraper_config_url(scraper_instance_t *target, config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("The 'url' block needs exactly one string argument.");
        return -1;
    }

    int status = cf_util_get_string(ci, &target->url);
    if (status != 0) {
        PLUGIN_ERROR("Invalid url name.");
        return -1;
    }

    target->timeout = CDTIME_DOOMSDAY;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("user", child->key) == 0) {
            status = cf_util_get_string(child, &target->user);
        } else if (strcasecmp("user-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &target->user);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &target->pass);
        } else if (strcasecmp("password-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &target->pass);
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
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &target->timeout);
        } else if (strcasecmp("collect", child->key) == 0) {
            status = curl_stats_from_config(child, &target->curl_stats_flags);
        }

        if (status != 0)
            break;
    }

    return status;
}

static int scraper_config_socket(scraper_instance_t *target, config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("The 'socket' block needs exactly one string argument.");
        return -1;
    }

    int status = cf_util_get_string(ci, &target->socket_path);
    if (status != 0) {
        PLUGIN_ERROR("Invalid socket name.");
        return -1;
    }

    target->timeout = CDTIME_DOOMSDAY;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &target->timeout);
        }

        if (status != 0)
            break;
    }

    return status;
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


    int status = cf_util_get_string(ci, &target->instance);
    if (status != 0) {
        PLUGIN_ERROR("Invalid instance name.");
        scraper_instance_free(target);
        return -1;
    }

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("url", child->key) == 0) {
            status = scraper_config_url(target, child);
        } else if (strcasecmp("file", child->key) == 0) {
            status = cf_util_get_string(child, &target->file_path);
        } else if (strcasecmp("socket", child->key) == 0) {
            status = scraper_config_socket(target, child);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &target->interval);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &target->label);
        } else if (strcasecmp("metric-prefix", child->key) == 0) {
            status = cf_util_get_string(child, &target->metric_prefix);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &target->filter);
        } else {
            PLUGIN_WARNING("Option `%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status == 0) {
        int count = 0;
        if (target->url != NULL)
            count++;
        if (target->file_path != NULL)
            count++;
        if (target->socket_path != NULL)
            count++;

        if (count == 0)  {
            PLUGIN_ERROR("Only one of 'url', 'file' or 'socket' can be set.");
            scraper_instance_free(target);
            return -1;
        }

        if (count > 1)  {
            PLUGIN_ERROR("At least one of 'url', 'file' or 'socket' must be set.");
            scraper_instance_free(target);
            return -1;
        }
    }

    if (status != 0) {
        scraper_instance_free(target);
        return -1;
    }

    target->mp = metric_parser_alloc(target->metric_prefix, &target->label);
    if (target->mp == NULL) {
        PLUGIN_ERROR("Cannot alloc metric parser.");
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
