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
    char *instance;
    char *metric_prefix;
    label_set_t labels;
    plugin_filter_t *filter;

    char *metric_response_time;
    char *metric_response_code;

    char *url;
    int address_family;
    char *user;
    char *pass;
    char *credentials;
    bool digest;
    bool verify_peer;
    bool verify_host;
    char *cacert;
    struct curl_slist *headers;
    char *post_body;
    bool response_time;
    bool response_code;
    int timeout;
    uint64_t curl_stats_flags;

    CURL *curl;
    char curl_errbuf[CURL_ERROR_SIZE];
    char *buffer;
    size_t buffer_size;
    size_t buffer_fill;

    plugin_match_t *matches;
} chttp_ctx_t;

static size_t chttp_curl_callback(void *buf, size_t size, size_t nmemb, void *user_data)
{
    size_t len;

    len = size * nmemb;
    if (len == 0)
        return len;

    chttp_ctx_t *ctx = user_data;
    if (ctx == NULL)
        return 0;

    if ((ctx->buffer_fill + len) >= ctx->buffer_size) {
        char *temp;
        size_t temp_size;

        temp_size = ctx->buffer_fill + len + 1;
        temp = realloc(ctx->buffer, temp_size);
        if (temp == NULL) {
            PLUGIN_ERROR("realloc failed.");
            return 0;
        }
        ctx->buffer = temp;
        ctx->buffer_size = temp_size;
    }

    memcpy(ctx->buffer + ctx->buffer_fill, (char *)buf, len);
    ctx->buffer_fill += len;
    ctx->buffer[ctx->buffer_fill] = 0;

    return len;
}

static void chttp_free(void *arg)
{
    chttp_ctx_t *ctx = arg;
    if (ctx == NULL)
        return;

    if (ctx->curl != NULL)
        curl_easy_cleanup(ctx->curl);
    ctx->curl = NULL;

    free(ctx->instance);
    free(ctx->metric_prefix);
    label_set_reset(&ctx->labels);
    plugin_filter_free(ctx->filter);

    free(ctx->metric_response_time);
    free(ctx->metric_response_code);
    free(ctx->url);
    free(ctx->user);
    free(ctx->pass);
    free(ctx->credentials);
    free(ctx->cacert);
    free(ctx->post_body);
    curl_slist_free_all(ctx->headers);

    free(ctx->buffer);

    if (ctx->matches != NULL)
        plugin_match_shutdown(ctx->matches);

    free(ctx);
}

static int chttp_config_append_string(const char *name, struct curl_slist **dest, config_item_t *ci)
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

static int cc_page_init_curl(chttp_ctx_t *ctx)
{
    ctx->curl = curl_easy_init();
    if (ctx->curl == NULL) {
        PLUGIN_ERROR("curl_easy_init failed.");
        return -1;
    }

    CURLcode rcode = 0;

    rcode = curl_easy_setopt(ctx->curl, CURLOPT_NOSIGNAL, 1L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_NOSIGNAL failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(ctx->curl, CURLOPT_WRITEFUNCTION, chttp_curl_callback);
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

    rcode = curl_easy_setopt(ctx->curl, CURLOPT_USERAGENT, NCOLLECTD_USERAGENT);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERAGENT failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(ctx->curl, CURLOPT_ERRORBUFFER, ctx->curl_errbuf);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_ERRORBUFFER failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
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

    rcode = curl_easy_setopt(ctx->curl, CURLOPT_IPRESOLVE, ctx->address_family);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_IPRESOLVE failed: %s",
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

        rcode = curl_easy_setopt(ctx->curl, CURLOPT_PASSWORD,
                                         (ctx->pass == NULL) ? "" : ctx->pass);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_PASSWORD failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
#else
        size_t credentials_size = strlen(ctx->user) + 2;
        if (ctx->pass != NULL)
            credentials_size += strlen(ctx->pass);

        ctx->credentials = malloc(credentials_size);
        if (ctx->credentials == NULL) {
            PLUGIN_ERROR("malloc failed.");
            return -1;
        }

        snprintf(ctx->credentials, credentials_size, "%s:%s", ctx->user,
                         (ctx->pass == NULL) ? "" : ctx->pass);
        rcode = curl_easy_setopt(ctx->curl, CURLOPT_USERPWD, ctx->credentials);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERPWD failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

#endif

        if (ctx->digest) {
            rcode = curl_easy_setopt(ctx->curl, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST);
            if (rcode != CURLE_OK) {
                PLUGIN_ERROR("curl_easy_setopt CURLOPT_HTTPAUTH failed: %s",
                             curl_easy_strerror(rcode));
                return -1;
            }
        }
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

    if (ctx->headers != NULL) {
        rcode = curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, ctx->headers);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_HTTPHEADER failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }

    if (ctx->post_body != NULL) {
        rcode = curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDS, ctx->post_body);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_POSTFIELDS failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }

#ifdef HAVE_CURLOPT_TIMEOUT_MS
    if (ctx->timeout >= 0) {
        rcode = curl_easy_setopt(ctx->curl, CURLOPT_TIMEOUT_MS,
                                            (long)ctx->timeout);
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

static int chttp_read(user_data_t *ud)
{

    if ((ud == NULL) || (ud->data == NULL)) {
        PLUGIN_ERROR("cc_read_page: Invalid user data.");
        return -1;
    }

    chttp_ctx_t *ctx = ud->data;

    cdtime_t start = 0;
    if (ctx->response_time)
        start = cdtime();

    ctx->buffer_fill = 0;

    CURLcode rcode = curl_easy_setopt(ctx->curl, CURLOPT_URL, ctx->url);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_URL failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    int status = curl_easy_perform(ctx->curl);
    if (status != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_perform failed with status %i: %s", status, ctx->curl_errbuf);
        return -1;
    }

    if (ctx->response_time) {
        metric_family_t fam = {
            .name = ctx->metric_response_time,
            .type = METRIC_TYPE_GAUGE,
        };
        double value = CDTIME_T_TO_DOUBLE(cdtime() - start);
        metric_family_append(&fam, VALUE_GAUGE(value), &ctx->labels, NULL);
        plugin_dispatch_metric_family_filtered(&fam, ctx->filter, 0);
    }

    curl_stats_dispatch(ctx->curl, ctx->curl_stats_flags,
                        ctx->metric_prefix != NULL ? ctx->metric_prefix : "http_", &ctx->labels);

    if (ctx->response_code) {
        long response_code = 0;
        status = curl_easy_getinfo(ctx->curl, CURLINFO_RESPONSE_CODE, &response_code);
        if (status != CURLE_OK) {
            PLUGIN_ERROR("Fetching response code failed with status %i: %s", status, ctx->curl_errbuf);
        } else {
            metric_family_t fam = {
             .name = ctx->metric_response_code,
             .type = METRIC_TYPE_GAUGE,
            };
            metric_family_append(&fam, VALUE_GAUGE(response_code), &ctx->labels, NULL);
            plugin_dispatch_metric_family_filtered(&fam, ctx->filter, 0);
        }
    }

    status = plugin_match(ctx->matches, ctx->buffer);
    if (status != 0)
        PLUGIN_WARNING("plugin_match failed.");

    plugin_match_dispatch(ctx->matches, ctx->filter, &ctx->labels, true);

    return 0;
}

static int chttp_config_instance(config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("`Page' blocks need exactly one string argument.");
        return -1;
    }

    chttp_ctx_t *ctx= calloc(1, sizeof(*ctx));
    if (ctx== NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }
    ctx->address_family = CURL_IPRESOLVE_WHATEVER;
    ctx->digest = false;
    ctx->verify_peer = true;
    ctx->verify_host = true;
    ctx->response_time = false;
    ctx->response_code = false;
    ctx->timeout = -1;

    ctx->instance = strdup(ci->values[0].value.string);
    if (ctx->instance == NULL) {
        PLUGIN_ERROR("strdup failed.");
        free(ctx);
        return -1;
    }

    /* Process all children */
    cdtime_t interval = 0;
    int status = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("metric-prefix", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->metric_prefix);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &ctx->labels);
        } else if (strcasecmp("url", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->url);
        } else if (strcasecmp("address-family", child->key) == 0) {
            char *af = NULL;
            status = cf_util_get_string(child, &af);
            if (status != 0 || af == NULL) {
                PLUGIN_WARNING("Cannot parse value of `%s' for instance `%s'.",
                               child->key, ctx->instance);
            } else if (strcasecmp("any", af) == 0) {
                ctx->address_family = CURL_IPRESOLVE_WHATEVER;
            } else if (strcasecmp("ipv4", af) == 0) {
                ctx->address_family = CURL_IPRESOLVE_V4;
            } else if (strcasecmp("ipv6", af) == 0) {
                /* If curl supports ipv6, use it. If not, log a warning and
                 * fall back to default - don't set status to non-zero.
                 */
                curl_version_info_data *curl_info = curl_version_info(CURLVERSION_NOW);
                if (curl_info->features & CURL_VERSION_IPV6)
                    ctx->address_family = CURL_IPRESOLVE_V6;
                else
                    PLUGIN_WARNING("IPv6 not supported by this libCURL. "
                                    "Using fallback `any'.");
            } else {
                PLUGIN_WARNING("Unsupported value of `%s' "
                                "for instance `%s'.",
                                child->key, ctx->instance);
                status = -1;
            }
            free(af);
        } else if (strcasecmp("user", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->user);
        } else if (strcasecmp("user-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &ctx->user);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->pass);
        } else if (strcasecmp("password-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &ctx->pass);
        } else if (strcasecmp("digest", child->key) == 0) {
            status = cf_util_get_boolean(child, &ctx->digest);
        } else if (strcasecmp("verify-peer", child->key) == 0) {
            status = cf_util_get_boolean(child, &ctx->verify_peer);
        } else if (strcasecmp("verify-host", child->key) == 0) {
            status = cf_util_get_boolean(child, &ctx->verify_host);
        } else if (strcasecmp("measure-response-time", child->key) == 0) {
            status = cf_util_get_boolean(child, &ctx->response_time);
        } else if (strcasecmp("measure-response-code", child->key) == 0) {
            status = cf_util_get_boolean(child, &ctx->response_code);
        } else if (strcasecmp("ca-cert", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->cacert);
        } else if (strcasecmp("match", child->key) == 0) {
            status = plugin_match_config(child, &ctx->matches);
        } else if (strcasecmp("header", child->key) == 0) {
            status = chttp_config_append_string("Header", &ctx->headers, child);
        } else if (strcasecmp("post", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->post_body);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_int(child, &ctx->timeout);
        } else if (strcasecmp("collect", child->key) == 0) {
            status = curl_stats_from_config(child, &ctx->curl_stats_flags);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &ctx->filter);
        } else {
            PLUGIN_WARNING("Option `%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    /* Additionial sanity checks and libCURL initialization. */
    while (status == 0) {
        if (ctx->url == NULL) {
            PLUGIN_WARNING("`URL' missing in `Page' block.");
            status = -1;
            break;
        }

        if ((ctx->matches == NULL) && (ctx->curl_stats_flags == 0) &&
            !ctx->response_time && !ctx->response_code) {
            assert(ctx->instance != NULL);
            PLUGIN_WARNING("No (valid) 'match' block "
                           "or 'statistics' or 'measure-response-time' or 'measure-response-code' "
                           "within `Page' block `%s'.",
                           ctx->instance);
            status = -1;
            break;
        }

        if (ctx->response_time) {
            if (ctx->metric_prefix == NULL)
                ctx->metric_response_time = strdup("http_response_time_seconds");
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
                ctx->metric_response_code = strdup("http_response_code");
            else
                ctx->metric_response_code = ssnprintf_alloc("%s_response_code",
                                                             ctx->metric_prefix);

            if (ctx->metric_response_code == NULL) {
                PLUGIN_ERROR("alloc metric response code string failed.");
                status = -1;
                break;
            }
        }

        if (status == 0)
            status = cc_page_init_curl(ctx);

        break;
    }

    if (status != 0) {
        chttp_free(ctx);
        return status;
    }

    label_set_add(&ctx->labels, true, "instance", ctx->instance);

    return plugin_register_complex_read("http", ctx->instance, chttp_read, interval,
                                        &(user_data_t){.data=ctx, .free_func=chttp_free});
}

static int chttp_config(config_item_t *ci)
{
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            int status = chttp_config_instance(child);
            if (status != 0)
                return -1;
        } else {
            PLUGIN_WARNING("Option `%s' not allowed here.", child->key);
            return -1;
        }
    }

    return 0;
}

static int chttp_init(void)
{
    curl_global_init(CURL_GLOBAL_SSL);
    return 0;
}

void module_register(void)
{
    plugin_register_config("http", chttp_config);
    plugin_register_init("http", chttp_init);
}
