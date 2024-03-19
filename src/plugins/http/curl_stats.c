// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2015 Sebastian 'tokkee' Harl
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>

#include "ncollectd.h"
#include "libutils/common.h"
#include <stdbool.h>
#include <stddef.h>

#include "curl_stats.h"

typedef struct curl_stats_s {
    char *metric_prefix;
    bool total_time;
    char *metric_total_time;
    bool namelookup_time;
    char *metric_namelookup_time;
    bool connect_time;
    char *metric_connect_time;
    bool pretransfer_time;
    char *metric_pretransfer_time;
    bool size_upload;
    char *metric_size_upload;
    bool size_download;
    char *metric_size_download;
    bool speed_download;
    char *metric_speed_download;
    bool speed_upload;
    char *metric_speed_upload;
    bool header_size;
    char *metric_header_size;
    bool request_size;
    char *metric_request_size;
    bool content_length_download;
    char *metric_content_length_download;
    bool content_length_upload;
    char *metric_content_length_upload;
    bool starttransfer_time;
    char *metric_starttransfer_time;
    bool redirect_time;
    char *metric_redirect_time;
    bool redirect_count;
    char *metric_redirect_count;
    bool num_connects;
    char *metric_num_connects;
    bool appconnect_time;
    char *metric_appconnect_time;
} curl_stats_t;

typedef enum {
    DISPATCH_SPEED = 0,
    DISPATCH_GAUGE,
    DISPATCH_OFF_T,
    DISPATCH_SIZE,
} curl_stats_type_t;

static struct {
    const char *name;
    const char *config_key;
    size_t offset_enable;
    size_t offset_metric;
    CURLINFO info;
    curl_stats_type_t type;
    char *metric_name;
} curl_stats_field_specs[] = {
#define CURL_STATS_SPEC(name, config_key, info, dispatcher, metric) \
    { #name, config_key, offsetof(curl_stats_t, name), offsetof(curl_stats_t, metric_ ## name), info, dispatcher, metric }
    CURL_STATS_SPEC(total_time, "total-time",
                    CURLINFO_TOTAL_TIME, DISPATCH_GAUGE, "total_seconds"),
    CURL_STATS_SPEC(namelookup_time, "namelookup-time",
                    CURLINFO_NAMELOOKUP_TIME, DISPATCH_GAUGE, "namelookup_seconds"),
    CURL_STATS_SPEC(connect_time, "connect-time",
                    CURLINFO_CONNECT_TIME, DISPATCH_GAUGE, "connect_seconds"),
    CURL_STATS_SPEC(pretransfer_time, "pretransfer-time",
                    CURLINFO_PRETRANSFER_TIME, DISPATCH_GAUGE, "pretransfer_seconds"),
#ifdef HAVE_CURLINFO_SIZE_UPLOAD_T
    CURL_STATS_SPEC(size_upload, "size-upload",
                    CURLINFO_SIZE_UPLOAD_T, DISPATCH_OFF_T, "upload_bytes"),
#else
    CURL_STATS_SPEC(size_upload, "size-upload",
                    CURLINFO_SIZE_UPLOAD, DISPATCH_GAUGE, "upload_bytes"),
#endif
#ifdef HAVE_CURLINFO_SIZE_DOWNLOAD_T
    CURL_STATS_SPEC(size_download, "size-download",
                    CURLINFO_SIZE_DOWNLOAD_T, DISPATCH_OFF_T, "download_bytes"),
#else
    CURL_STATS_SPEC(size_download, "size-download",
                    CURLINFO_SIZE_DOWNLOAD, DISPATCH_GAUGE, "download_bytes"),
#endif
#ifdef HAVE_CURLINFO_SPEED_DOWNLOAD_T
    CURL_STATS_SPEC(speed_download, "speed-download",
                    CURLINFO_SPEED_DOWNLOAD_T, DISPATCH_OFF_T, "download_bitrate"),
#else
    CURL_STATS_SPEC(speed_download, "speed-download",
                    CURLINFO_SPEED_DOWNLOAD, DISPATCH_SPEED, "download_bitrate"),
#endif
#ifdef HAVE_CURLINFO_SPEED_UPLOAD_T
    CURL_STATS_SPEC(speed_upload, "speed-upload",
                    CURLINFO_SPEED_UPLOAD_T, DISPATCH_OFF_T, "upload_bitrate"),
#else
    CURL_STATS_SPEC(speed_upload, "speed-upload",
                    CURLINFO_SPEED_UPLOAD, DISPATCH_SPEED, "upload_bitrate"),
#endif
    CURL_STATS_SPEC(header_size, "header-size",
                    CURLINFO_HEADER_SIZE, DISPATCH_SIZE,  "header_bytes"),
    CURL_STATS_SPEC(request_size, "request-size",
                    CURLINFO_REQUEST_SIZE, DISPATCH_SIZE,  "request_bytes"),
#ifdef HAVE_CURLINFO_CONTENT_LENGTH_DOWNLOAD_T
    CURL_STATS_SPEC(content_length_download, "sontent-length-download",
                    CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, DISPATCH_OFF_T, "download_content_bytes"),
#else
    CURL_STATS_SPEC(content_length_download, "sontent-length-download",
                    CURLINFO_CONTENT_LENGTH_DOWNLOAD, DISPATCH_GAUGE, "download_content_bytes"),
#endif
#ifdef HAVE_CURLINFO_CONTENT_LENGTH_UPLOAD_T
    CURL_STATS_SPEC(content_length_upload, "sontent-length-upload",
                    CURLINFO_CONTENT_LENGTH_UPLOAD_T, DISPATCH_OFF_T, "upload_content_bytes"),
#else
    CURL_STATS_SPEC(content_length_upload, "sontent-length-upload",
                    CURLINFO_CONTENT_LENGTH_UPLOAD, DISPATCH_GAUGE, "upload_content_bytes"),
#endif
    CURL_STATS_SPEC(starttransfer_time, "starttransfer-time",
                    CURLINFO_STARTTRANSFER_TIME, DISPATCH_GAUGE, "start_transfer_seconds" ),
    CURL_STATS_SPEC(redirect_time, "redirect-time",
                    CURLINFO_REDIRECT_TIME, DISPATCH_GAUGE, "redirect_seconds"),
    CURL_STATS_SPEC(redirect_count, "redirect-count",
                    CURLINFO_REDIRECT_COUNT, DISPATCH_SIZE,  "redirects"),
    CURL_STATS_SPEC(num_connects, "num-connects",
                    CURLINFO_NUM_CONNECTS, DISPATCH_SIZE,  "connects"),
#ifdef HAVE_CURLINFO_APPCONNECT_TIME
    CURL_STATS_SPEC(appconnect_time, "appconnect-time",
                    CURLINFO_APPCONNECT_TIME, DISPATCH_GAUGE, "appconnect_seconds"),
#endif
#undef CURL_STATS_SPEC
};

static void curl_stats_enable_field(curl_stats_t *s, size_t offset)
{
    *(bool *)((char *)s + offset) = true;
}

static bool curl_stats_field_enabled(curl_stats_t *s, size_t offset)
{
    return *(bool *)((char *)s + offset);
}

static char *curl_stats_field_metric_get(curl_stats_t *s, size_t offset)
{
    return (char *)((char *)s + offset);
}

static void curl_stats_field_metric_set(curl_stats_t *s, size_t offset, char *metric)
{
    *(char **)((char *)s + offset) = metric;
}

void curl_stats_destroy(curl_stats_t *s)
{
    if (s == NULL)
        return;

    free(s->metric_prefix);
    free(s->metric_total_time);
    free(s->metric_namelookup_time);
    free(s->metric_connect_time);
    free(s->metric_pretransfer_time);
    free(s->metric_size_upload);
    free(s->metric_size_download);
    free(s->metric_speed_download);
    free(s->metric_speed_upload);
    free(s->metric_header_size);
    free(s->metric_request_size);
    free(s->metric_content_length_download);
    free(s->metric_content_length_upload);
    free(s->metric_starttransfer_time);
    free(s->metric_redirect_time);
    free(s->metric_redirect_count);
    free(s->metric_num_connects);
    free(s->metric_appconnect_time);

    free(s);
}

int curl_stats_from_config(config_item_t *ci, char *prefix, curl_stats_t **curl_stats)
{
    if (ci == NULL)
        return -1;

    curl_stats_t *s = calloc(1, sizeof(*s));
    if (s == NULL)
        return -1;

    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *c = ci->children + i;

        bool enabled = 0;

        size_t field;
        for (field = 0; field < STATIC_ARRAY_SIZE(curl_stats_field_specs); ++field) {
            if (!strcasecmp(c->key, curl_stats_field_specs[field].config_key))
                break;
            if (!strcasecmp(c->key, curl_stats_field_specs[field].name))
                break;
        }

        if (field >= STATIC_ARRAY_SIZE(curl_stats_field_specs)) {
            if (!strcasecmp(c->key, "metric-prefix")) {
                int status = cf_util_get_string(c, &s->metric_prefix);
                if (status != 0) {
                    curl_stats_destroy(s);
                    return -1;
                }
            } else {
                PLUGIN_ERROR("Unknown field name %s in %s:%d.",
                             c->key, cf_get_file(c), cf_get_lineno(c));
                curl_stats_destroy(s);
                return -1;
            }
        } else {
            if (cf_util_get_boolean(c, &enabled) != 0) {
                curl_stats_destroy(s);
                return -1;
            }
            if (enabled)
                curl_stats_enable_field(s, curl_stats_field_specs[field].offset_enable);
        }
    }

    if ((s->metric_prefix == NULL) && (prefix != NULL)) {
        s->metric_prefix = strdup(prefix);
        if (s->metric_prefix == NULL) {
            PLUGIN_ERROR("strdup failed.");
            curl_stats_destroy(s);
            return -1;
        }
    }

    if (s->metric_prefix == NULL) {
        s->metric_prefix = strdup("http_");
        if (s->metric_prefix == NULL) {
            PLUGIN_ERROR("strdup failed.");
            curl_stats_destroy(s);
            return -1;
        }
    }


    for (size_t field = 0; field < STATIC_ARRAY_SIZE(curl_stats_field_specs); ++field) {
        if (curl_stats_field_enabled(s, curl_stats_field_specs[field].offset_enable)) {
            char *metric = ssnprintf_alloc("%s%s", s->metric_prefix,
                                                   curl_stats_field_specs[field].metric_name);
            if (metric == NULL) {
                PLUGIN_ERROR("Not enough memory to alloc metric name");
                curl_stats_destroy(s);
                return -1;
            }
            curl_stats_field_metric_set(s, curl_stats_field_specs[field].offset_metric, metric);
        }
    }

    *curl_stats = s;

    return 0;
}

int curl_stats_dispatch(curl_stats_t *s, CURL *curl, label_set_t *labels)
{
    if (s == NULL)
        return 0;

    if (curl == NULL) {
        PLUGIN_ERROR("dispatch() called with missing arguments (curl=%p)", curl);
        return -1;
    }

    for (size_t field = 0; field < STATIC_ARRAY_SIZE(curl_stats_field_specs); ++field) {
        if (!curl_stats_field_enabled(s, curl_stats_field_specs[field].offset_enable))
            continue;

        CURLcode code;
        value_t value = {0};
        switch (curl_stats_field_specs[field].type) {
            case DISPATCH_SPEED: {
                double raw;
                code = curl_easy_getinfo(curl, curl_stats_field_specs[field].info, &raw);
                if (code != CURLE_OK)
                    continue;
                value = VALUE_GAUGE(raw * 8);
            }   break;
            case DISPATCH_GAUGE: {
                double raw;
                code = curl_easy_getinfo(curl, curl_stats_field_specs[field].info, &raw);
                if (code != CURLE_OK)
                    continue;
                value = VALUE_GAUGE(raw);
            }   break;
            case DISPATCH_OFF_T: {
                curl_off_t raw;
                code = curl_easy_getinfo(curl, curl_stats_field_specs[field].info, &raw);
                if (code != CURLE_OK)
                    continue;
                value = VALUE_GAUGE(raw);
            }   break;
            case DISPATCH_SIZE: {
                long raw;
                code = curl_easy_getinfo(curl, curl_stats_field_specs[field].info, &raw);
                if (code != CURLE_OK)
                    continue;
                value = VALUE_GAUGE(raw);
            }   break;
        }

        metric_family_t fam = {0};
        fam.name = curl_stats_field_metric_get(s, curl_stats_field_specs[field].offset_metric);
        fam.type = METRIC_TYPE_GAUGE;

        metric_family_append(&fam, value, labels, NULL);

        plugin_dispatch_metric_family(&fam, 0);
    }

    return 0;
}
