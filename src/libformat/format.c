// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "log.h"
#include "libutils/config.h"
#include "libmetric/metric.h"
#include "libmetric/notification.h"
#include "libformat/format.h"
#include "libformat/influxdb.h"
#include "libformat/graphite_line.h"
#include "libformat/json.h"
#include "libformat/opentsdb_json.h"
#include "libformat/opentsdb_telnet.h"
#include "libformat/openmetrics_text.h"
#include "libformat/openmetrics_proto.h"
#include "libformat/opentelemetry_json.h"
#include "libformat/remote_proto.h"
#include "libformat/notification_text.h"
#include "libformat/notification_json.h"
#include "libformat/notification_proto.h"
#include "libformat/log_text.h"
#include "libformat/log_logfmt.h"
#include "libformat/log_json.h"
#include "libformat/log_logstash.h"

int config_format_stream_metric(config_item_t *ci, format_stream_metric_t *format)
{
    if ((ci->values_num < 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("The '%s' config option needs at least one string argument.", ci->key);
        return -1;
    }

    char *fmt = ci->values[0].value.string;

    if (strcasecmp(fmt, "influxdb") == 0) {
        if (ci->values_num == 1) {
            *format = FORMAT_STREAM_METRIC_INFLUXDB_SEC;
            return 0;
        }

        if ((ci->values_num != 2) || (ci->values[1].type != CONFIG_TYPE_STRING)) {
            PLUGIN_WARNING("The '%s'influxdb config option needs "
                           "at least one string argument.", ci->key);
            return -1;
        }

        char *opt = ci->values[1].value.string;
        if (strcasecmp(opt, "sec") == 0){
            *format = FORMAT_STREAM_METRIC_INFLUXDB_SEC;
            return 0;
        } else if (strcasecmp(opt, "msec") == 0) {
            *format = FORMAT_STREAM_METRIC_INFLUXDB_MSEC;
            return 0;
        } else if (strcasecmp(opt, "usec") == 0) {
            *format = FORMAT_STREAM_METRIC_INFLUXDB_USEC;
            return 0;
        } else if (strcasecmp(opt, "nsec") == 0) {
            *format = FORMAT_STREAM_METRIC_INFLUXDB_NSEC;
            return 0;
        } else {
            return -1;
        }
    } else if (strcasecmp(fmt, "graphite") == 0) {
        *format = FORMAT_STREAM_METRIC_GRAPHITE_LINE;
        return 0;
    } else if (strcasecmp(fmt, "json") == 0) {
        *format = FORMAT_STREAM_METRIC_JSON;
        return 0;
    } else if (strcasecmp(fmt, "kairosdb") == 0) {
        if (ci->values_num == 1) {
            *format = FORMAT_STREAM_METRIC_KAIROSDB_TELNET_SEC;
            return 0;
        }

        if (ci->values[1].type != CONFIG_TYPE_STRING) {
            PLUGIN_WARNING("The '%s' kairosdb config option needs "
                           "at least one string argument.", ci->key);
            return -1;
        }
        char *opt = ci->values[1].value.string;

        if (strcasecmp(opt, "telnet") == 0) {
            if (ci->values_num == 2) {
                *format = FORMAT_STREAM_METRIC_KAIROSDB_TELNET_SEC;
                return 0;
            }
            if (ci->values[2].type != CONFIG_TYPE_STRING) {
                PLUGIN_WARNING("The '%s' kairosdb config option needs "
                               "at least one string argument.", ci->key);
                return -1;
            }
            opt = ci->values[2].value.string;

            if (strcasecmp(opt, "sec") == 0) {
                *format = FORMAT_STREAM_METRIC_KAIROSDB_TELNET_SEC;
                return 0;
            } else if (strcasecmp(opt, "msec") == 0) {
                *format = FORMAT_STREAM_METRIC_KAIROSDB_TELNET_MSEC;
                return 0;
            } else {
                return -1;
            }
        } else if (strcasecmp(opt, "json") == 0) {
            *format = FORMAT_STREAM_METRIC_KAIROSDB_JSON;
            return 0;
        } else {
            return -1;
        }
    } else if (strcasecmp(fmt, "opentsdb") == 0) {
        if (ci->values_num == 1) {
            *format = FORMAT_STREAM_METRIC_OPENTSDB_TELNET;
            return 0;
        }

        if ((ci->values_num != 2) || (ci->values[1].type != CONFIG_TYPE_STRING)) {
            PLUGIN_WARNING("The '%s' opentsdb config option needs "
                           "at least one string argument.", ci->key);
            return -1;
        }

        char *opt = ci->values[1].value.string;
        if (strcasecmp(opt, "json") == 0) {
            *format = FORMAT_STREAM_METRIC_OPENTSDB_JSON;
            return 0;
        } else if (strcasecmp(opt, "telnet") == 0) {
            *format = FORMAT_STREAM_METRIC_OPENTSDB_TELNET;
            return 0;
        } else {
            return -1;
        }
    } else if (strcasecmp(fmt, "openmetrics") == 0) {
        if (ci->values_num == 1) {
            *format = FORMAT_STREAM_METRIC_OPENMETRICS_TEXT;
            return 0;
        }

        if ((ci->values_num != 2) || (ci->values[1].type != CONFIG_TYPE_STRING)) {
            PLUGIN_WARNING("The `%s' openmetrics config option needs "
                           "at least one string argument.", ci->key);
            return -1;
        }

        char *opt = ci->values[1].value.string;
        if (strcasecmp(opt, "text") == 0) {
            *format = FORMAT_STREAM_METRIC_OPENMETRICS_TEXT;
            return 0;
        } else if (strcasecmp(opt, "protob") == 0) {
            *format = FORMAT_STREAM_METRIC_OPENMETRICS_PROTOB;
            return 0;
        } else {
            return -1;
        }
        return 0;
    } else if (strcasecmp(fmt, "opentelemetry") == 0) {
        *format = FORMAT_STREAM_METRIC_OPENTELEMETRY_JSON;
        return 0;
    } else if (strcasecmp(fmt, "remote") == 0) {
        if (ci->values_num == 1) {
            *format = FORMAT_STREAM_METRIC_REMOTE_WRITE_NOMETADATA;
            return 0;
        }

        if ((ci->values_num != 2) || (ci->values[1].type != CONFIG_TYPE_STRING)) {
            PLUGIN_WARNING("The '%s' remote config option needs "
                           "at least one string argument.", ci->key);
            return -1;
        }

        char *opt = ci->values[1].value.string;
        if (strcasecmp(opt, "metadata") == 0) {
            *format = FORMAT_STREAM_METRIC_REMOTE_WRITE_METADATA;
            return 0;
        } else {
            return -1;
        }
        return 0;
    }

    PLUGIN_ERROR("Invalid format string: %s", fmt);
    return -1;
}

int format_stream_metric_begin(format_stream_metric_ctx_t *ctx, format_stream_metric_t format,
                                                                strbuf_t *buf)
{
    ctx->format = format;
    ctx->buf = buf;

    switch(format) {
    case FORMAT_STREAM_METRIC_INFLUXDB_SEC:
    case FORMAT_STREAM_METRIC_INFLUXDB_MSEC:
    case FORMAT_STREAM_METRIC_INFLUXDB_USEC:
    case FORMAT_STREAM_METRIC_INFLUXDB_NSEC:
        break;
    case FORMAT_STREAM_METRIC_GRAPHITE_LINE:
        break;
    case FORMAT_STREAM_METRIC_JSON:
        break;
    case FORMAT_STREAM_METRIC_KAIROSDB_TELNET_SEC:
    case FORMAT_STREAM_METRIC_KAIROSDB_TELNET_MSEC:
    case FORMAT_STREAM_METRIC_KAIROSDB_JSON:
    case FORMAT_STREAM_METRIC_OPENTSDB_TELNET:
    case FORMAT_STREAM_METRIC_OPENTSDB_JSON:
        break;
    case FORMAT_STREAM_METRIC_OPENMETRICS_TEXT:
        break;
    case FORMAT_STREAM_METRIC_OPENMETRICS_PROTOB:
        break;
    case FORMAT_STREAM_METRIC_OPENTELEMETRY_JSON:
        break;
    case FORMAT_STREAM_METRIC_REMOTE_WRITE_METADATA:
    case FORMAT_STREAM_METRIC_REMOTE_WRITE_NOMETADATA:
        break;
    }
    return 0;
}

int format_stream_metric_family(format_stream_metric_ctx_t *ctx, metric_family_t const *fam)
{

    switch(ctx->format) {
    case FORMAT_STREAM_METRIC_INFLUXDB_SEC:
        return influxdb_metric_family(ctx->buf, fam, FMT_INFLUXDB_SEC);
        break;
    case FORMAT_STREAM_METRIC_INFLUXDB_MSEC:
        return influxdb_metric_family(ctx->buf, fam, FMT_INFLUXDB_MSEC);
        break;
    case FORMAT_STREAM_METRIC_INFLUXDB_USEC:
        return influxdb_metric_family(ctx->buf, fam, FMT_INFLUXDB_USEC);
        break;
    case FORMAT_STREAM_METRIC_INFLUXDB_NSEC:
        return influxdb_metric_family(ctx->buf, fam, FMT_INFLUXDB_NSEC);
        break;
    case FORMAT_STREAM_METRIC_GRAPHITE_LINE:
        return graphite_line_metric_family(ctx->buf, fam);
        break;
    case FORMAT_STREAM_METRIC_JSON:
        return json_metric_family(ctx->buf, fam);
        break;
    case FORMAT_STREAM_METRIC_KAIROSDB_TELNET_SEC:
    case FORMAT_STREAM_METRIC_OPENTSDB_TELNET:
        return opentsdb_telnet_metric_family(ctx->buf, fam, 0, FMT_OPENTSDB_SEC);
        break;
    case FORMAT_STREAM_METRIC_KAIROSDB_TELNET_MSEC:
        return opentsdb_telnet_metric_family(ctx->buf, fam, 0, FMT_OPENTSDB_MSEC);
        break;
    case FORMAT_STREAM_METRIC_KAIROSDB_JSON:
    case FORMAT_STREAM_METRIC_OPENTSDB_JSON:
        return opentsdb_json_metric_family(ctx->buf, fam, 0);
        break;
    case FORMAT_STREAM_METRIC_OPENMETRICS_TEXT:
        return openmetrics_text_metric_family(ctx->buf, fam);
        break;
    case FORMAT_STREAM_METRIC_OPENMETRICS_PROTOB:
//        return openmetrics_proto_metric_family(buf_t *buf, fam);
        break;
    case FORMAT_STREAM_METRIC_OPENTELEMETRY_JSON:
        return opentelemetry_json_metric_family(ctx->buf, fam);
        break;
    case FORMAT_STREAM_METRIC_REMOTE_WRITE_METADATA:
    case FORMAT_STREAM_METRIC_REMOTE_WRITE_NOMETADATA: {
        buf_t buf = {0};
        strbuf2buf(&buf, ctx->buf);
        bool meta = ctx->format == FORMAT_STREAM_METRIC_REMOTE_WRITE_METADATA ? true : false;
        int status = remote_proto_metric_family(&buf, fam, meta);
        buf2strbuf(ctx->buf, &buf);
        return status;
    }   break;
    }
    return 0;
}

char *format_stream_metric_content_type(format_stream_metric_t format)
{
    switch(format) {
    case FORMAT_STREAM_METRIC_INFLUXDB_SEC:
    case FORMAT_STREAM_METRIC_INFLUXDB_MSEC:
    case FORMAT_STREAM_METRIC_INFLUXDB_USEC:
    case FORMAT_STREAM_METRIC_INFLUXDB_NSEC:
        return "application/influxdb";
        break;
    case FORMAT_STREAM_METRIC_GRAPHITE_LINE:
        return "application/graphite";
        break;
    case FORMAT_STREAM_METRIC_JSON:
        return "application/json";
        break;
    case FORMAT_STREAM_METRIC_KAIROSDB_TELNET_SEC:
    case FORMAT_STREAM_METRIC_KAIROSDB_TELNET_MSEC:
        return "text/kairosdb";
        break;
    case FORMAT_STREAM_METRIC_KAIROSDB_JSON:
        return "application/json";
        break;
    case FORMAT_STREAM_METRIC_OPENTSDB_TELNET:
        return "text/opentsdb";
        break;
    case FORMAT_STREAM_METRIC_OPENTSDB_JSON:
        return "application/json";
        break;
    case FORMAT_STREAM_METRIC_OPENMETRICS_TEXT:
        return "application/openmetrics-text; version=1.0.0; charset=utf-8";
        break;
    case FORMAT_STREAM_METRIC_OPENMETRICS_PROTOB:
        return "application/openmetrics-protobuf; version=1.0.0";
        break;
    case FORMAT_STREAM_METRIC_OPENTELEMETRY_JSON:
        return "application/json";
        break;
    case FORMAT_STREAM_METRIC_REMOTE_WRITE_METADATA:
    case FORMAT_STREAM_METRIC_REMOTE_WRITE_NOMETADATA:
        // application/x-protobuf // FIXME
        return "protobuf/remote";
        break;
    }
    return NULL;
}

int format_stream_metric_end(format_stream_metric_ctx_t *ctx)
{
    switch(ctx->format) {
    case FORMAT_STREAM_METRIC_INFLUXDB_SEC:
    case FORMAT_STREAM_METRIC_INFLUXDB_MSEC:
    case FORMAT_STREAM_METRIC_INFLUXDB_USEC:
    case FORMAT_STREAM_METRIC_INFLUXDB_NSEC:
        break;
    case FORMAT_STREAM_METRIC_GRAPHITE_LINE:
        break;
    case FORMAT_STREAM_METRIC_JSON:
        break;
    case FORMAT_STREAM_METRIC_KAIROSDB_TELNET_SEC:
    case FORMAT_STREAM_METRIC_KAIROSDB_TELNET_MSEC:
        break;
    case FORMAT_STREAM_METRIC_KAIROSDB_JSON:
        break;
    case FORMAT_STREAM_METRIC_OPENTSDB_TELNET:
        break;
    case FORMAT_STREAM_METRIC_OPENTSDB_JSON:
        break;
    case FORMAT_STREAM_METRIC_OPENMETRICS_TEXT:
        break;
    case FORMAT_STREAM_METRIC_OPENMETRICS_PROTOB:
        break;
    case FORMAT_STREAM_METRIC_OPENTELEMETRY_JSON:
        break;
    case FORMAT_STREAM_METRIC_REMOTE_WRITE_METADATA:
    case FORMAT_STREAM_METRIC_REMOTE_WRITE_NOMETADATA:
        break;
    }
    return 0;
}

int config_format_dgram_metric(config_item_t *ci, format_dgram_metric_t *format)
{
    if ((ci->values_num < 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("The `%s' config option needs at least one string argument.", ci->key);
        return -1;
    }

    char *fmt = ci->values[0].value.string;

    if (strcasecmp(fmt, "influxdb") == 0) {
        if (ci->values_num == 1) {
            *format = FORMAT_DGRAM_METRIC_INFLUXDB_SEC;
            return 0;
        }

        if ((ci->values_num != 2) || (ci->values[1].type != CONFIG_TYPE_STRING)) {
            PLUGIN_WARNING("The `%s'influxdb config option needs "
                           "at least one string argument.", ci->key);
            return -1;
        }

        char *opt = ci->values[1].value.string;
        if (strcasecmp(opt, "sec") == 0){
            *format = FORMAT_DGRAM_METRIC_INFLUXDB_SEC;
            return 0;
        } else if (strcasecmp(opt, "msec") == 0) {
            *format = FORMAT_DGRAM_METRIC_INFLUXDB_MSEC;
            return 0;
        } else if (strcasecmp(opt, "usec") == 0) {
            *format = FORMAT_DGRAM_METRIC_INFLUXDB_USEC;
            return 0;
        } else if (strcasecmp(opt, "nsec") == 0) {
            *format = FORMAT_DGRAM_METRIC_INFLUXDB_NSEC;
            return 0;
        } else {
            return -1;
        }
    } else if (strcasecmp(fmt, "graphite") == 0) {
        *format = FORMAT_DGRAM_METRIC_GRAPHITE_LINE;
        return 0;
    } else if (strcasecmp(fmt, "kairosdb") == 0) {
        if (ci->values_num == 1) {
            *format = FORMAT_DGRAM_METRIC_KAIROSDB_TELNET_SEC;
            return 0;
        }

        if ((ci->values_num != 2) || (ci->values[1].type != CONFIG_TYPE_STRING)) {
            PLUGIN_WARNING("The `%s' kairosdb config option needs "
                           "at least one string argument.", ci->key);
            return -1;
        }

        char *opt = ci->values[1].value.string;
        if (strcasecmp(opt, "sec") == 0){
            *format = FORMAT_DGRAM_METRIC_KAIROSDB_TELNET_SEC;
            return 0;
        } else if (strcasecmp(opt, "msec") == 0) {
            *format = FORMAT_DGRAM_METRIC_KAIROSDB_TELNET_MSEC;
            return 0;
        } else {
            return -1;
        }
    } else if (strcasecmp(fmt, "opentsdb") == 0) {
        *format = FORMAT_DGRAM_METRIC_OPENTSDB_TELNET;
        return 0;
    }

    PLUGIN_ERROR("Invalid format string: %s", fmt);
    return -1;
}

int format_dgram_metric(format_dgram_metric_t format, strbuf_t *buf,
                        metric_family_t const *fam, metric_t const *m)
{
    switch(format) {
    case FORMAT_DGRAM_METRIC_INFLUXDB_SEC:
        return influxdb_metric(buf, fam, m, FMT_INFLUXDB_SEC);
        break;
    case FORMAT_DGRAM_METRIC_INFLUXDB_MSEC:
        return influxdb_metric(buf, fam, m, FMT_INFLUXDB_MSEC);
        break;
    case FORMAT_DGRAM_METRIC_INFLUXDB_USEC:
        return influxdb_metric(buf, fam, m, FMT_INFLUXDB_USEC);
        break;
    case FORMAT_DGRAM_METRIC_INFLUXDB_NSEC:
        return influxdb_metric(buf, fam, m, FMT_INFLUXDB_NSEC);
        break;
    case FORMAT_DGRAM_METRIC_GRAPHITE_LINE:
        return graphite_line_metric(buf, fam, m);
        break;
    case FORMAT_DGRAM_METRIC_KAIROSDB_TELNET_SEC:
        return opentsdb_telnet_metric(buf, fam, m, 0, FMT_OPENTSDB_SEC);
        break;
    case FORMAT_DGRAM_METRIC_KAIROSDB_TELNET_MSEC:
        return opentsdb_telnet_metric(buf, fam, m, 0, FMT_OPENTSDB_MSEC);
        break;
    case FORMAT_DGRAM_METRIC_OPENTSDB_TELNET:
        break;
    }
    return 0;
}

int config_format_notification(config_item_t *ci, format_notification_t *format)
{
    if ((ci->values_num < 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("The `%s' config option needs at least one string argument.", ci->key);
        return -1;
    }

    char *fmt = ci->values[0].value.string;
    if (strcasecmp(fmt, "text") == 0) {
        *format = FORMAT_NOTIFICATION_TEXT;
        return 0;
    } else if (strcasecmp(fmt, "json") == 0) {
        *format = FORMAT_NOTIFICATION_JSON;
        return 0;
    } else if (strcasecmp(fmt, "protob") == 0) {
        *format = FORMAT_NOTIFICATION_PROTOB;
        return 0;
    }

    PLUGIN_ERROR("Invalid format string: %s", fmt);
    return -1;
}

int format_notification(format_notification_t format, strbuf_t *buf, notification_t const *n)
{
    switch(format) {
    case FORMAT_NOTIFICATION_TEXT:
        return notification_text(buf, n);
        break;
    case FORMAT_NOTIFICATION_JSON:
        return notification_json(buf, n);
        break;
    case FORMAT_NOTIFICATION_PROTOB:
        break;
    }

    return 0;
}

char *format_notification_content_type(format_notification_t format)
{
    switch(format) {
    case FORMAT_NOTIFICATION_TEXT:
        return "text/plain";
        break;
    case FORMAT_NOTIFICATION_JSON:
        return "application/json";
        break;
    case FORMAT_NOTIFICATION_PROTOB:
        return "application/x-protobuf";
        break;
    }

    return NULL;
}

int format_log(strbuf_t *buf, format_log_t fmt, size_t flags, const log_msg_t *msg)
{
    if (msg == NULL)
        return -1;

    int status = 0;
    switch (fmt) {
    case FORMAT_LOG_TEXT:
        status = log_text(buf, flags, msg);
        break;
    case FORMAT_LOG_LOGFMT:
        status = log_logfmt(buf, flags, msg);
        break;
    case FORMAT_LOG_JSON:
        status = log_json(buf, flags, msg);
        break;
    case FORMAT_LOG_LOGSTASH:
        status = log_logstash(buf, flags, msg);
        break;
    }

    return status;
}

int config_format_log(config_item_t *ci, format_log_t *format, size_t *flags)
{
    if ((ci->values_num < 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("The `%s' config option in %s:%d needs at least one string argument.",
                       ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    char *fmt = ci->values[0].value.string;
    if (strcasecmp(fmt, "text") == 0) {
        *format = FORMAT_LOG_TEXT;
    } else if (strcasecmp(fmt, "logfmt") == 0) {
        *format = FORMAT_LOG_LOGFMT;
    } else if (strcasecmp(fmt, "json") == 0) {
        *format = FORMAT_LOG_JSON;
    } else if (strcasecmp(fmt, "logstash") == 0) {
        *format = FORMAT_LOG_LOGSTASH;
    } else {
        PLUGIN_ERROR("Invalid log format option: %s in %s:%d.",
                      fmt, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    for (int i = 1; i < ci->values_num; i++) {
        if (ci->values[i].type != CONFIG_TYPE_STRING) {
            PLUGIN_ERROR("The %d argument of '%s' option in %s:%d must be a string.",
                         i+1, ci->key, cf_get_file(ci), cf_get_lineno(ci));
            return -1;
        }

        char *option = ci->values[i].value.string;

        bool negate = false;
        if (option[0] == '!') {
            negate = true;
            option++;
        }

        if (!strcasecmp("all", option)) {
            if (negate) {
                *flags = 0;
            } else {
                *flags = ~(size_t)0;
            }
        } else if (!strcasecmp("timestamp", option)) {
            if (negate) {
                *flags &= ~LOG_PRINT_TIMESTAMP;
            } else {
                *flags |= LOG_PRINT_TIMESTAMP;
            }
        } else if (!strcasecmp("severity", option)) {
            if (negate) {
                *flags &= ~LOG_PRINT_SEVERITY;
            } else {
                *flags |= LOG_PRINT_SEVERITY;
            }
        } else if (!strcasecmp("plugin", option)) {
            if (negate) {
                *flags &= ~LOG_PRINT_PLUGIN;
            } else {
                *flags |= LOG_PRINT_PLUGIN;
            }
        } else if (!strcasecmp("file", option)) {
            if (negate) {
                *flags &= ~LOG_PRINT_FILE;
            } else {
                *flags |= LOG_PRINT_FILE;
            }
        } else if (!strcasecmp("line", option)) {
            if (negate) {
                *flags &= ~LOG_PRINT_LINE;
            } else {
                *flags |= LOG_PRINT_LINE;
            }
        } else if (!strcasecmp("function", option)) {
            if (negate) {
                *flags &= ~LOG_PRINT_FUNCTION;
            } else {
                *flags |= LOG_PRINT_FUNCTION;
            }
        } else if (!strcasecmp("message", option)) {
            if (negate) {
                *flags &= ~LOG_PRINT_MESSAGE;
            } else {
                *flags |= LOG_PRINT_MESSAGE;
            }
        } else {
            PLUGIN_ERROR("Invalid log format flags: %s in %s:%d.",
                         ci->values[i].value.string, cf_get_file(ci), cf_get_lineno(ci));
            return -1;
        }
    }

    return 0;
}
