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
#include "libutils/complain.h"
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
    char *proxy;
    bool proxy_tunnel;
    cdtime_t timeout;
    cdtime_t interval;
    uint64_t max_size;
    uint64_t read_size;
    char *metric_prefix;
    label_set_t label;
    plugin_filter_t *filter;
    struct curl_slist *headers;
    char *post_body;
    uint64_t curl_stats_flags;
    CURL *curl;
    char curl_errbuf[CURL_ERROR_SIZE];
    metric_parser_t *mp;
    unsigned int parse_errors;
    c_complain_t parse_complaint;
} scraper_instance_t;

static void scraper_parse_buffer(scraper_instance_t *target, char *buffer, size_t len)
{
    int status = metric_parse_buffer(target->mp, buffer, len);
    if (status < 0)
        target->parse_errors++;
}

static void scraper_parse_report(scraper_instance_t *target)
{
    if (target->parse_errors > 0) {
        c_complain(LOG_WARNING, &target->parse_complaint,
                   "Instance '%s': failed to parse %u chunk(s) of the response, "
                   "some metrics may be lost.", target->instance, target->parse_errors);
    } else {
        c_release(LOG_INFO, &target->parse_complaint,
                  "Instance '%s': response parsed successfully again.", target->instance);
    }
    target->parse_errors = 0;
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

    /* Do not parse the body of a non successful HTTP response.
     * The response code is zero if a non-HTTP transport was used. */
    long rc = 0;
    if (target->curl != NULL)
        curl_easy_getinfo(target->curl, CURLINFO_RESPONSE_CODE, &rc);
    if ((rc != 0) && ((rc < 200) || (rc > 299)))
        return len;

    target->read_size += len;
    if ((target->max_size > 0) && (target->read_size > target->max_size))
        return 0; /* Abort the transfer. */

    scraper_parse_buffer(target, buf, buf_len);

    return len;
}

static int scraper_curl_init(scraper_instance_t *target)
{
    if (target->curl != NULL)
        return 0;

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

        free(target->credentials);
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

    if (target->max_size > 0) {
        rcode = curl_easy_setopt(target->curl, CURLOPT_MAXFILESIZE_LARGE,
                                               (curl_off_t)target->max_size);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_MAXFILESIZE_LARGE failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }

    rcode = curl_easy_setopt(target->curl, CURLOPT_URL, target->url);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_URL failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    if (target->proxy != NULL) {
        rcode = curl_easy_setopt(target->curl, CURLOPT_PROXY, target->proxy);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_PROXY failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(target->curl, CURLOPT_HTTPPROXYTUNNEL,
                                               target->proxy_tunnel ? 1L : 0L);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_HTTPPROXYTUNNEL failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }

    return 0;
}

static void scaper_curl_cleanup(scraper_instance_t *target)
{
    if (target->curl != NULL) {
        curl_easy_cleanup(target->curl);
        target->curl = NULL;
        target->curl_errbuf[0] = '\0';
    }
}

static int scaper_read_url(scraper_instance_t *target)
{
    if (scraper_curl_init(target) != 0) {
        scaper_curl_cleanup(target);
        return 0;
    }

    target->read_size = 0;

    int status = curl_easy_perform(target->curl);
    if ((target->max_size > 0) &&
        ((status == CURLE_FILESIZE_EXCEEDED) || (target->read_size > target->max_size))) {
        PLUGIN_ERROR("Instance '%s': response size exceeds the configured 'max-size' of %" PRIu64 " bytes.",
                     target->instance, target->max_size);
        metric_parser_reset(target->mp);
        scaper_curl_cleanup(target);
        return 0;
    }

    if (status != CURLE_OK) {
        PLUGIN_ERROR("Instance '%s': curl_easy_perform failed with status %i: %s",
                     target->instance, status, target->curl_errbuf);
        metric_parser_reset(target->mp);
        target->parse_errors = 0;
        scaper_curl_cleanup(target);
        return 0;
    }

    label_set_t lstats = {0};
    label_set_clone(&lstats, target->label);
    label_set_add(&lstats, false, "instance", target->instance);
    curl_stats_dispatch(target->curl, target->curl_stats_flags, target->filter,
                        target->metric_prefix != NULL ? target->metric_prefix : "scraper_",
                        &lstats);
    label_set_reset(&lstats);

    //  if (target->stats != NULL)
    //      curl_stats_dispatch(target->stats, target->curl, cjq_host(db),
    //      "prometheus", target->instance);

    long rc = 0;
    curl_easy_getinfo(target->curl, CURLINFO_RESPONSE_CODE, &rc);
    /* The response code is zero if a non-HTTP transport was used. */
    if ((rc != 0) && ((rc < 200) || (rc > 299))) {
        PLUGIN_ERROR("Instance '%s': curl_easy_perform failed with response code %ld",
                     target->instance, rc);
        metric_parser_reset(target->mp);
        target->parse_errors = 0;
        scaper_curl_cleanup(target);
        return 0;
    }

    scraper_parse_report(target);

    /* Flush the last line if it does not end with a newline. */
    metric_parse_buffer(target->mp, NULL, 0);
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
    while (true) {
        ssize_t len = read(fd, buffer, sizeof(buffer));
        if (len < 0) {
            if (errno == EINTR)
                continue;
            PLUGIN_ERROR("read (%s): %s", target->file_path, STRERRNO);
            close(fd);
            metric_parser_reset(target->mp);
            return -1;
        }
        if (len == 0)
            break;

        scraper_parse_buffer(target, buffer, len);
    }

    close(fd);

    /* Flush the last line if it does not end with a newline. */
    metric_parse_buffer(target->mp, NULL, 0);
    scraper_parse_report(target);

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
    while (true) {
        ssize_t len = read(fd, buffer, sizeof(buffer));
        if (len < 0) {
            if (errno == EINTR)
                continue;
            PLUGIN_ERROR("read (%s): %s", target->socket_path, STRERRNO);
            close(fd);
            metric_parser_reset(target->mp);
            return -1;
        }
        if (len == 0)
            break;

        scraper_parse_buffer(target, buffer, len);
    }

    close(fd);

    /* Flush the last line if it does not end with a newline. */
    metric_parse_buffer(target->mp, NULL, 0);
    scraper_parse_report(target);

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
    free(target->proxy);

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

    target->verify_peer = true;
    target->verify_host = true;
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
        } else if (strcasecmp("proxy", child->key) == 0) {
            status = cf_util_get_string(child, &target->proxy);
        } else if (strcasecmp("proxy-tunnel", child->key) == 0) {
            status = cf_util_get_boolean(child, &target->proxy_tunnel);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed here.",
                         child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
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
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed here.",
                         child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
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
        } else if (strcasecmp("max-size", child->key) == 0) {
            unsigned int max_size = 0;
            status = cf_util_get_unsigned_int(child, &max_size);
            if (status == 0)
                target->max_size = max_size;
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
            PLUGIN_ERROR("At least one of 'url', 'file' or 'socket' must be set.");
            scraper_instance_free(target);
            return -1;
        }

        if (count > 1)  {
            PLUGIN_ERROR("Only one of 'url', 'file' or 'socket' can be set.");
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
