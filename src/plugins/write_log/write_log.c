// SPDX-License-Identifier: GPL-2.0-only OR MIT
// Copyright (C) 2015       Pierre-Yves Ritschard
// Authors:
//   Pierre-Yves Ritschard <pyr at spootnik.org>

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

#include "utils/format_graphite/format_graphite.h"
#include "utils/format_json/format_json.h"
#include "utils/strbuf/strbuf.h"

#include <netdb.h>

typedef enum {
  WL_FORMAT_OPENMETRICS = 0,
  WL_FORMAT_GRAPHITE,
  WL_FORMAT_JSON,
} wl_format_e;

wl_format_e wl_format;

static int wl_write_graphite(metric_family_t const *fam)
{
  char const *prefix = "";
  char const *suffix = "";
  char escape_char = '_';
  unsigned int flags = 0;

  strbuf_t buf = STRBUF_CREATE;

  for (size_t i = 0; i < fam->metric.num; i++) {
    metric_t const *m = fam->metric.ptr + i;
    int status = format_graphite(&buf, m, prefix, suffix, escape_char, flags);
    if (status != 0) {
      ERROR("write_log plugin: format_graphite failed: %d", status);
    } else {
      INFO("write_log values:\n%s", buf.ptr);
    }

    strbuf_reset(&buf);
  }

  strbuf_destroy(&buf);
  return 0;
}

static int wl_write_json(metric_family_t const *fam)
{
  strbuf_t buf = STRBUF_CREATE;

  int status = format_json_metric_family(&buf, fam, /* store rates = */ false);
  if (status != 0) {
    ERROR("write_log plugin: format_json_metric_family failed: %d", status);
  } else {
    INFO("write_log values:\n%s", buf.ptr);
  }

  strbuf_destroy(&buf);
  return 0;
}

static int wl_write_openmetrics(metric_family_t const *fam)
{
  if (fam->metric.num == 0)
   return 0;

  strbuf_t buf = STRBUF_CREATE;

  for (size_t i = 0; i < fam->metric.num; i++) {
    metric_t *m = &fam->metric.ptr[i];

    int status = metric_identity(&buf, m);

    switch (fam->type) {
    case METRIC_TYPE_UNKNOWN:
      if (m->value.unknown.type == UNKNOWN_FLOAT64) {
        status = status | strbuf_printf(&buf, " " GAUGE_FORMAT, m->value.unknown.float64);
      } else if (m->value.unknown.type == UNKNOWN_INT64) {
        status = status | strbuf_printf(&buf, " %" PRIi64, m->value.unknown.int64);
      }
    case METRIC_TYPE_GAUGE:
      if (m->value.gauge.type == GAUGE_FLOAT64) {
        status = status | strbuf_printf(&buf, " " GAUGE_FORMAT, m->value.gauge.float64);
      } else if (m->value.gauge.type == GAUGE_INT64) {
        status = status | strbuf_printf(&buf, " %" PRIi64, m->value.gauge.int64);
      }
      break;
    case METRIC_TYPE_COUNTER:
      if (m->value.counter.type == COUNTER_UINT64) {
        status = status | strbuf_printf(&buf, " " COUNTER_FORMAT, m->value.counter.uint64);
      } else if (m->value.counter.type == COUNTER_FLOAT64) {
        status = status | strbuf_printf(&buf, " " GAUGE_FORMAT, m->value.counter.float64);
      }
      break;
    case METRIC_TYPE_STATE_SET:
      break;
    case METRIC_TYPE_INFO:
      break;
    case METRIC_TYPE_DISTRIBUTION:
      break;
    }

    if (m->time > 0)
      status = status | strbuf_printf(&buf, " %" PRIi64, CDTIME_T_TO_MS(m->time));

    if (status != 0) {
      ERROR("write_log plugin: format_json_metric_family failed: %d", status);
      continue;
    }
    INFO("%s", buf.ptr);
    strbuf_reset(&buf);
  }

  strbuf_destroy(&buf);
  return 0;
}

static int wl_write(metric_family_t const *fam,
                    __attribute__((unused)) user_data_t *user_data)
{
  switch (wl_format) {
    case WL_FORMAT_GRAPHITE:
      return wl_write_graphite(fam);
    case WL_FORMAT_JSON:
      return wl_write_json(fam);
    case WL_FORMAT_OPENMETRICS:
      return wl_write_openmetrics(fam);
  }

  return EIO;
}

static int wl_config(oconfig_item_t *ci)
{
  bool format_seen = false;

  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = ci->children + i;

    if (strcasecmp("Format", child->key) == 0) {
      char str[16];

      if (cf_util_get_string_buffer(child, str, sizeof(str)) != 0)
        continue;

      if (format_seen) {
        WARNING("write_log plugin: Redefining option `%s'.", child->key);
      }
      format_seen = true;

      if (strcasecmp("Graphite", str) == 0)
        wl_format = WL_FORMAT_GRAPHITE;
      else if (strcasecmp("JSON", str) == 0)
        wl_format = WL_FORMAT_JSON;
      else if (strcasecmp("OpenMetrics", str) == 0)
        wl_format = WL_FORMAT_OPENMETRICS;
      else {
        ERROR("write_log plugin: Unknown format `%s' for option `%s'.", str,
              child->key);
        return -EINVAL;
      }
    } else {
      ERROR("write_log plugin: Invalid configuration option: `%s'.",
            child->key);
      return -EINVAL;
    }
  }

  return 0;
}

void module_register(void)
{
  plugin_register_complex_config("write_log", wl_config);
  plugin_register_write("write_log", wl_write, NULL);
}
