/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "libconfig/config.h"
#include "libutils/strbuf.h"
#include "libutils/buf.h"
#include "libmetric/metric.h"
#include "libmetric/notification.h"

typedef enum {
    FORMAT_STREAM_METRIC_INFLUXDB_SEC,
    FORMAT_STREAM_METRIC_INFLUXDB_MSEC,
    FORMAT_STREAM_METRIC_INFLUXDB_USEC,
    FORMAT_STREAM_METRIC_INFLUXDB_NSEC,
    FORMAT_STREAM_METRIC_GRAPHITE_LINE,
    FORMAT_STREAM_METRIC_JSON,
    FORMAT_STREAM_METRIC_KAIROSDB_TELNET_SEC,
    FORMAT_STREAM_METRIC_KAIROSDB_TELNET_MSEC,
    FORMAT_STREAM_METRIC_KAIROSDB_JSON,
    FORMAT_STREAM_METRIC_OPENTSDB_TELNET,
    FORMAT_STREAM_METRIC_OPENTSDB_JSON,
    FORMAT_STREAM_METRIC_OPENMETRICS_TEXT,
    FORMAT_STREAM_METRIC_OPENMETRICS_PROTOB,
    FORMAT_STREAM_METRIC_OPENTELEMETRY_JSON,
    FORMAT_STREAM_METRIC_REMOTE_WRITE_METADATA,
    FORMAT_STREAM_METRIC_REMOTE_WRITE_NOMETADATA,
} format_stream_metric_t;

typedef struct {
    format_stream_metric_t format;
    strbuf_t *buf;
} format_stream_metric_ctx_t;

int config_format_stream_metric(config_item_t *ci, format_stream_metric_t *format);
int format_stream_metric_begin(format_stream_metric_ctx_t *ctx, format_stream_metric_t format, strbuf_t *buf);
int format_stream_metric_family(format_stream_metric_ctx_t *ctx, metric_family_t const *fam);
int format_stream_metric_end(format_stream_metric_ctx_t *ctx);
char *format_stream_metric_content_type(format_stream_metric_t format);

typedef enum {
    FORMAT_DGRAM_METRIC_INFLUXDB_SEC,
    FORMAT_DGRAM_METRIC_INFLUXDB_MSEC,
    FORMAT_DGRAM_METRIC_INFLUXDB_USEC,
    FORMAT_DGRAM_METRIC_INFLUXDB_NSEC,
    FORMAT_DGRAM_METRIC_GRAPHITE_LINE,
    FORMAT_DGRAM_METRIC_KAIROSDB_TELNET_SEC,
    FORMAT_DGRAM_METRIC_KAIROSDB_TELNET_MSEC,
    FORMAT_DGRAM_METRIC_OPENTSDB_TELNET,
} format_dgram_metric_t;

int config_format_dgram_metric(config_item_t *ci, format_dgram_metric_t *format);
int format_dgram_metric(format_dgram_metric_t format, strbuf_t *buf,
                        metric_family_t const *fam, metric_t const *m);

typedef enum {
    FORMAT_NOTIFICATION_TEXT,
    FORMAT_NOTIFICATION_JSON,
    FORMAT_NOTIFICATION_PROTOB
} format_notification_t;

int format_notification(format_notification_t format, strbuf_t *buf, notification_t const *n);
int config_format_notification(config_item_t *ci, format_notification_t *format);
char *format_notification_content_type(format_notification_t format);

typedef enum {
    FORMAT_LOG_TEXT,
    FORMAT_LOG_LOGFMT,
    FORMAT_LOG_JSON,
    FORMAT_LOG_LOGSTASH,
} format_log_t;

typedef enum {
    LOG_PRINT_TIMESTAMP = 1 << 1,
    LOG_PRINT_SEVERITY  = 1 << 2,
    LOG_PRINT_PLUGIN    = 1 << 3,
    LOG_PRINT_FILE      = 1 << 4,
    LOG_PRINT_LINE      = 1 << 5,
    LOG_PRINT_FUNCTION  = 1 << 6,
    LOG_PRINT_MESSAGE   = 1 << 7,
} log_print_flags_t;

#define LOG_PRINT_ALL (LOG_PRINT_TIMESTAMP | LOG_PRINT_SEVERITY | LOG_PRINT_PLUGIN | \
                       LOG_PRINT_FILE | LOG_PRINT_LINE | LOG_PRINT_FUNCTION | LOG_PRINT_MESSAGE)

int format_log(strbuf_t *buf, format_log_t fmt, size_t flags, const log_msg_t *msg);
int config_format_log(config_item_t *ci, format_log_t *format, size_t *flags);
