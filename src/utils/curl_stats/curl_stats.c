/**
 * collectd - src/utils_curl_stats.c
 * Copyright (C) 2015       Sebastian 'tokkee' Harl
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Sebastian Harl <sh@tokkee.org>
 **/

#include "collectd.h"

#include "utils/common/common.h"
#include "utils/curl_stats/curl_stats.h"

#include <stdbool.h>
#include <stddef.h>

struct curl_stats_s {
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
};

/*
 * Private functions
 */
typedef enum {
  DISPATCH_SPEED = 0,
  DISPATCH_GAUGE,
  DISPATCH_SIZE,
} dispatch_type_e;

static struct {
  const char *name;
  const char *config_key;
  size_t offset_enable;
  size_t offset_metric;
  CURLINFO info;
  dispatch_type_e type;
  char *metric_name;
} field_specs[] = {
#define SPEC(name, config_key, info, dispatcher, metric)                         \
  { #name, config_key, offsetof(curl_stats_t, name), offsetof(curl_stats_t, metric_ ## name), info, dispatcher, metric }
    SPEC(total_time             , "TotalTime"            , CURLINFO_TOTAL_TIME,              DISPATCH_GAUGE, "total_seconds"          ),
    SPEC(namelookup_time        , "NamelookupTime"       , CURLINFO_NAMELOOKUP_TIME,         DISPATCH_GAUGE, "namelookup_seconds"     ),
    SPEC(connect_time           , "ConnectTime"          , CURLINFO_CONNECT_TIME,            DISPATCH_GAUGE, "connect_seconds"        ),
    SPEC(pretransfer_time       , "PretransferTime"      , CURLINFO_PRETRANSFER_TIME,        DISPATCH_GAUGE, "pretransfer_seconds"    ),
    SPEC(size_upload            , "SizeUpload"           , CURLINFO_SIZE_UPLOAD,             DISPATCH_GAUGE, "upload_bytes"           ),
    SPEC(size_download          , "SizeDownload"         , CURLINFO_SIZE_DOWNLOAD,           DISPATCH_GAUGE, "download_bytes"         ),
    SPEC(speed_download         , "SpeedDownload"        , CURLINFO_SPEED_DOWNLOAD,          DISPATCH_SPEED, "download_bitrate"       ),
    SPEC(speed_upload           , "SpeedUpload"          , CURLINFO_SPEED_UPLOAD,            DISPATCH_SPEED, "upload_bitrate"         ),
    SPEC(header_size            , "HeaderSize"           , CURLINFO_HEADER_SIZE,             DISPATCH_SIZE,  "header_bytes"           ),
    SPEC(request_size           , "RequestSize"          , CURLINFO_REQUEST_SIZE,            DISPATCH_SIZE,  "request_bytes"          ),
    SPEC(content_length_download, "ContentLengthDownload", CURLINFO_CONTENT_LENGTH_DOWNLOAD, DISPATCH_GAUGE, "download_content_bytes" ),
    SPEC(content_length_upload  , "ContentLengthUpload"  , CURLINFO_CONTENT_LENGTH_UPLOAD,   DISPATCH_GAUGE, "upload_content_bytes"   ),
    SPEC(starttransfer_time     , "StarttransferTime"    , CURLINFO_STARTTRANSFER_TIME,      DISPATCH_GAUGE, "start_transfer_seconds" ),
    SPEC(redirect_time          , "RedirectTime"         , CURLINFO_REDIRECT_TIME,           DISPATCH_GAUGE, "redirect_seconds"       ),
    SPEC(redirect_count         , "RedirectCount"        , CURLINFO_REDIRECT_COUNT,          DISPATCH_SIZE,  "redirects"              ),
    SPEC(num_connects           , "NumConnects"          , CURLINFO_NUM_CONNECTS,            DISPATCH_SIZE,  "connects"               ),
#ifdef HAVE_CURLINFO_APPCONNECT_TIME
    SPEC(appconnect_time        , "AppconnectTime"       , CURLINFO_APPCONNECT_TIME,         DISPATCH_GAUGE, "appconnect_seconds"     ),
#endif
#undef SPEC
};

static void enable_field(curl_stats_t *s, size_t offset) {
  *(bool *)((char *)s + offset) = true;
} /* enable_field */

static bool field_enabled(curl_stats_t *s, size_t offset) {
  return *(bool *)((char *)s + offset);
} /* field_enabled */

static char *field_metric_get(curl_stats_t *s, size_t offset) {
  return (char *)((char *)s + offset);
} /* field_metric_get */

static void field_metric_set(curl_stats_t *s, size_t offset, char *metric) {
 *(char **)((char *)s + offset) = metric;
} /* field_metric_set */

/*
 * Public API
 */

void curl_stats_destroy(curl_stats_t *s) {
  if (s == NULL)
    return;

  sfree(s->metric_prefix);
  sfree(s->metric_total_time);
  sfree(s->metric_namelookup_time);
  sfree(s->metric_connect_time);
  sfree(s->metric_pretransfer_time);
  sfree(s->metric_size_upload);
  sfree(s->metric_size_download);
  sfree(s->metric_speed_download);
  sfree(s->metric_speed_upload);
  sfree(s->metric_header_size);
  sfree(s->metric_request_size);
  sfree(s->metric_content_length_download);
  sfree(s->metric_content_length_upload);
  sfree(s->metric_starttransfer_time);
  sfree(s->metric_redirect_time);
  sfree(s->metric_redirect_count);
  sfree(s->metric_num_connects);
  sfree(s->metric_appconnect_time);

  sfree(s);
} /* curl_stats_destroy */

curl_stats_t *curl_stats_from_config(oconfig_item_t *ci) {
  if (ci == NULL)
    return NULL;

  curl_stats_t *s = calloc(1, sizeof(*s));
  if (s == NULL)
    return NULL;

  for (int i = 0; i < ci->children_num; ++i) {
    oconfig_item_t *c = ci->children + i;

    bool enabled = 0;

    size_t field;
    for (field = 0; field < STATIC_ARRAY_SIZE(field_specs); ++field) {
      if (!strcasecmp(c->key, field_specs[field].config_key))
        break;
      if (!strcasecmp(c->key, field_specs[field].name))
        break;
    }

    if (field >= STATIC_ARRAY_SIZE(field_specs)) {
      if (!strcasecmp(c->key, "MetricPrefix")) {
        int status = cf_util_get_string(c, &s->metric_prefix);
        if (status != 0) {
          curl_stats_destroy(s);
          return NULL;
        }
      } else {
        ERROR("curl stats: Unknown field name %s", c->key);
        curl_stats_destroy(s);
        return NULL;
      }
    } else {
      if (cf_util_get_boolean(c, &enabled) != 0) {
        free(s);
        return NULL;
      }
      if (enabled)
        enable_field(s, field_specs[field].offset_enable);
    }
  }

  if (s->metric_prefix == NULL)
    s->metric_prefix = strdup("curl_");

  for (size_t field = 0; field < STATIC_ARRAY_SIZE(field_specs); ++field) {
    if (field_enabled(s, field_specs[field].offset_enable)) {
      char *metric = ssnprintf_alloc("%s%s", s->metric_prefix, field_specs[field].metric_name);
      if (metric == NULL) {
        ERROR("curl stats: Not enough memory to alloc metric name");
        curl_stats_destroy(s);
        return NULL;
      }
      field_metric_set(s, field_specs[field].offset_metric, metric);
    }
  }

  return s;
} /* curl_stats_from_config */

int curl_stats_dispatch(curl_stats_t *s, CURL *curl, metric_t *tmpl) {
  if (s == NULL)
    return 0;

  if (curl == NULL) {
    ERROR("curl stats: dispatch() called with missing arguments "
          "(curl=%p)", curl);
    return -1;
  }

  for (size_t field = 0; field < STATIC_ARRAY_SIZE(field_specs); ++field) {
    if (!field_enabled(s, field_specs[field].offset_enable))
      continue;

    CURLcode code;
    value_t value;
    switch (field_specs[field].type) {
      case DISPATCH_SPEED:
        code = curl_easy_getinfo(curl, field_specs[field].info, &value.gauge);
        if (code != CURLE_OK)
          continue;
        value.gauge *= 8;
        break;
      case DISPATCH_GAUGE:
        code = curl_easy_getinfo(curl, field_specs[field].info, &value.gauge);
        if (code != CURLE_OK)
          continue;
        break;
      case DISPATCH_SIZE:
      {
        long raw;
        code = curl_easy_getinfo(curl, field_specs[field].info, &raw);
        if (code != CURLE_OK)
          continue;
        value.gauge = (double)raw;
      }
        break;
    }

    metric_family_t fam = {0};
    fam.name = field_metric_get(s, field_specs[field].offset_metric);
    fam.type = METRIC_TYPE_GAUGE;

    metric_family_append(&fam, NULL, NULL, value, tmpl);

    int status = plugin_dispatch_metric_family(&fam);
    if (status != 0) {
       ERROR("curl stats: plugin_dispatch_metric_family failed: %s", STRERROR(status));
    }
    metric_family_metric_reset(&fam);
  }

  return 0;
} /* curl_stats_dispatch */
