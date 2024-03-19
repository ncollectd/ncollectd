// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "libutils/common.h"
#include "libutils/buf_pb.h"
#include "libmetric/metric.h"


/*
 *  message Label {
 *      string name  = 1; // Required.
 *      string value = 2; // Required.
 *  }
 */
static size_t openmetrics_label_size(int field, label_set_t labels)
{
    size_t len = 0;
    for (size_t i = 0; i < labels.num; i++) {
        size_t size = buf_pb_size_str(1, labels.ptr[i].name) +
                      buf_pb_size_str(2, labels.ptr[i].value);
        len += buf_pb_size_type(field, PB_WIRE_TYPE_LENDELIM);
        len += buf_pb_size_varint(size);
        len += size;
    }
    return len;
}

static int openmetrics_label(buf_t *buf, int field, label_set_t labels)
{
    int status = 0;

    for (size_t i = 0; i < labels.num; i++) {
        size_t size = buf_pb_size_str(1, labels.ptr[i].name) +
                      buf_pb_size_str(2, labels.ptr[i].value);
        status |= buf_pb_enc_type(buf, field, PB_WIRE_TYPE_LENDELIM);
        status |= buf_pb_enc_varint(buf, size);
        status |= buf_pb_enc_str(buf, 1, labels.ptr[i].name);
        status |= buf_pb_enc_str(buf, 2, labels.ptr[i].value);
    }
    return status;
}

/*
 *  message Timestamp {
 *      int64 seconds = 1;  // Represents seconds of UTC time since Unix epoch
 *      int32 nanos   = 2;  // Non-negative fractions of a second at nanosecond resolution.
 *  }
 */
static size_t openmetrics_timestamp_size(int field, cdtime_t time)
{
    struct timespec ts = CDTIME_T_TO_TIMESPEC(time);
    size_t msg_size = buf_pb_size_int64(1, ts.tv_sec) + buf_pb_size_int32(2, ts.tv_nsec);
    return buf_pb_size_type(field, PB_WIRE_TYPE_LENDELIM) + buf_pb_size_varint(msg_size) + msg_size;
}

static int openmetrics_timestamp(buf_t *buf, int field, cdtime_t time)
{
    struct timespec ts = CDTIME_T_TO_TIMESPEC(time);

    size_t size = buf_pb_size_int64(1, ts.tv_sec) +
                  buf_pb_size_int32(2, ts.tv_nsec);

    int status = buf_pb_enc_type(buf, field, PB_WIRE_TYPE_LENDELIM);
    status |= buf_pb_enc_varint(buf, size);
    status |= buf_pb_enc_int64(buf, 1, ts.tv_sec);
    status |= buf_pb_enc_int32(buf, 2, ts.tv_nsec);
    return status;
}

/*
 *  message UnknownValue {
 *      oneof value {               // Required.
 *          double double_value = 1;
 *          int64 int_value     = 2;
 *      }
 *  }
 */
static size_t openmetrics_metricpoint_unknown_size(int field, metric_t const *m)
{
    size_t msg_size = 0;
    if (m->value.unknown.type == UNKNOWN_FLOAT64) {
        msg_size = buf_pb_size_double(1, m->value.unknown.float64);
    } else {
        msg_size = buf_pb_size_int64(2, m->value.unknown.int64);
    }
    return buf_pb_size_type(field, PB_WIRE_TYPE_LENDELIM) + buf_pb_size_varint(msg_size) + msg_size;
}
static int openmetrics_metricpoint_unknown(buf_t *buf, int field, metric_t const *m)
{
    size_t size = 0;

    if (m->value.unknown.type == UNKNOWN_FLOAT64) {
        size = buf_pb_size_double(1, m->value.unknown.float64);
    } else {
        size = buf_pb_size_int64(2, m->value.unknown.int64);
    }

    int status = buf_pb_enc_type(buf, field, PB_WIRE_TYPE_LENDELIM);
    status |= buf_pb_enc_varint(buf, size);

    if (m->value.unknown.type == UNKNOWN_FLOAT64) {
        status |= buf_pb_enc_double(buf, 1, m->value.unknown.float64);
    } else {
        status |= buf_pb_enc_int64(buf, 2, m->value.unknown.int64);
    }

    return status;
}
/*
 *  message GaugeValue {
 *      oneof value {              // Required.
 *          double double_value = 1;
 *          int64 int_value     = 2;
 *      }
 *  }
 */
static size_t openmetrics_metricpoint_gauge_size(int field, metric_t const *m)
{
    size_t msg_size = 0;
    if (m->value.gauge.type == GAUGE_FLOAT64) {
        msg_size = buf_pb_size_double(1, m->value.gauge.float64);
    } else {
        msg_size = buf_pb_size_int64(2, m->value.gauge.int64);
    }
    return buf_pb_size_type(field, PB_WIRE_TYPE_LENDELIM) + buf_pb_size_varint(msg_size) + msg_size;
}

static int openmetrics_metricpoint_gauge(buf_t *buf, int field, metric_t const *m)
{
    size_t size = 0;

    if (m->value.gauge.type == GAUGE_FLOAT64) {
        size = buf_pb_size_double(1, m->value.gauge.float64);
    } else {
        size = buf_pb_size_int64(2, m->value.gauge.int64);
    }

    int status = buf_pb_enc_type(buf, field, PB_WIRE_TYPE_LENDELIM);
    status |= buf_pb_enc_varint(buf, size);

    if (m->value.gauge.type == GAUGE_FLOAT64) {
        status |= buf_pb_enc_double(buf, 1, m->value.gauge.float64);
    } else {
        status |= buf_pb_enc_int64(buf, 2, m->value.gauge.int64);
    }

    return status;
}

/*
 *  message CounterValue {
 *      oneof total {              // Required.
 *          double double_value = 1;
 *          uint64 int_value    = 2;
 *      }
 *      Timestamp created     = 3; // Optional.
 *  }
 */
static size_t openmetrics_metricpoint_counter_size(int field, metric_t const *m)
{
    size_t msg_size = 0;
    if (m->value.counter.type == COUNTER_UINT64) {
        msg_size = buf_pb_size_uint64(2, m->value.counter.uint64);
    } else {
        msg_size = buf_pb_size_double(1, m->value.counter.float64);
    }
    return buf_pb_size_type(field, PB_WIRE_TYPE_LENDELIM) +
           buf_pb_size_varint(msg_size) + msg_size;
}

static int openmetrics_metricpoint_counter(buf_t *buf,  int field,metric_t const *m)
{
    size_t size = 0;

    if (m->value.counter.type == COUNTER_UINT64) {
        size = buf_pb_size_uint64(2, m->value.counter.uint64);
    } else {
        size = buf_pb_size_double(1, m->value.counter.float64);
    }

    int status = buf_pb_enc_type(buf, field, PB_WIRE_TYPE_LENDELIM);
    status |= buf_pb_enc_varint(buf, size);

    if (m->value.counter.type == COUNTER_UINT64) {
        status |= buf_pb_enc_uint64(buf, 2, m->value.counter.uint64);
    } else {
        status |= buf_pb_enc_double(buf, 1, m->value.counter.float64);
    }

    return status;
}

/*
 *  message HistogramValue {
 *      oneof sum {                    // Optional.
 *          double double_value   = 1;
 *          int64 int_value       = 2;
 *      }
 *      uint64 count              = 3; // Optional.
 *      Timestamp created         = 4; // Optional.
 *      repeated Bucket buckets   = 5; // Optional.
 *      message Bucket {
 *          uint64 count          = 1; // Required.
 *          double upper_bound    = 2; // Optional.
 *      }
 *  }
 */
static size_t openmetrics_metricpoint_historgram_size(__attribute__((unused)) int field,
                                                     __attribute__((unused)) metric_t const *m)
{
    return 0;
}

static int openmetrics_metricpoint_historgram(__attribute__((unused)) buf_t *buf,
                                              __attribute__((unused)) int field,
                                              __attribute__((unused)) metric_t const *m)
{
    return 0;
}

/*
 *  message StateSetValue {
 *      repeated State states   = 1; // Optional.
 *      message State {
 *          bool enabled        = 1; // Required.
 *          string name         = 2; // Required.
 *      }
 *  }
 */
static size_t openmetrics_metricpoint_state_set_size(__attribute__((unused)) int field,
                                                     __attribute__((unused)) metric_t const *m)
{
    return 0;
}

static int openmetrics_metricpoint_state_set(__attribute__((unused)) buf_t *buf,
                                             __attribute__((unused)) int field,
                                             __attribute__((unused)) metric_t const *m)
{
    return 0;
}

/*
 *  message InfoValue {
 *      repeated Label info = 1; // Optional.
 *  }
 */
static size_t openmetrics_metricpoint_info_size(int field, metric_t const *m)
{
    size_t msg_size = openmetrics_label_size(field, m->value.info);
    return buf_pb_size_type(field, PB_WIRE_TYPE_LENDELIM) +
           buf_pb_size_varint(msg_size) + msg_size;
}

static int openmetrics_metricpoint_info(buf_t *buf, int field, metric_t const *m)
{
    size_t size = openmetrics_label_size(field, m->value.info);

    int status = buf_pb_enc_type(buf, field, PB_WIRE_TYPE_LENDELIM);
    status |= buf_pb_enc_varint(buf, size);
    status |= openmetrics_label(buf, field, m->value.info);
    return status;
}

/*
 *  message SummaryValue {
 *      oneof sum {                     // Optional.
 *          double double_value    = 1;
 *          int64 int_value        = 2;
 *      }
 *      uint64 count = 3;               // Optional.
 *      Timestamp created          = 4; // Optional.
 *      repeated Quantile quantile = 5; // Optional.
 *      message Quantile {
 *          double quantile        = 1; // Required.
 *          double value           = 2; // Required.
 *      }
 *  }
 */
static size_t openmetrics_metricpoint_summary_size(__attribute__((unused)) int field,
                                                   __attribute__((unused)) metric_t const *m)
{
    return 0;
}

static int openmetrics_metricpoint_summary(__attribute__((unused)) buf_t *buf,
                                           __attribute__((unused)) int field,
                                           __attribute__((unused)) metric_t const *m)
{
    return 0;
}


/*
 *  message MetricPoint {
 *      oneof value {                         // Required.
 *          UnknownValue unknown_value     = 1;
 *          GaugeValue gauge_value         = 2;
 *          CounterValue counter_value     = 3;
 *          HistogramValue histogram_value = 4;
 *          StateSetValue state_set_value  = 5;
 *          InfoValue info_value           = 6;
 *          SummaryValue summary_value     = 7;
 *      }
 *      Timestamp timestamp              = 8; // Optional.
 *  }
 */
static size_t openmetrics_metricpoint_size(int field, metric_family_t const *fam, metric_t const *m)
{
    size_t msg_size = 0;

    switch(fam->type) {
    case METRIC_TYPE_UNKNOWN:
        msg_size = openmetrics_metricpoint_unknown_size(1, m);
        break;
    case METRIC_TYPE_GAUGE:
        msg_size = openmetrics_metricpoint_gauge_size(2, m);
        break;
    case METRIC_TYPE_COUNTER:
        msg_size = openmetrics_metricpoint_counter_size(3, m);
        break;
    case METRIC_TYPE_STATE_SET:
        msg_size = openmetrics_metricpoint_state_set_size(5, m);
        break;
    case METRIC_TYPE_INFO:
        msg_size = openmetrics_metricpoint_info_size(6, m);
        break;
    case METRIC_TYPE_SUMMARY:
        msg_size = openmetrics_metricpoint_summary_size(7, m);
        break;
    case METRIC_TYPE_HISTOGRAM:
        msg_size = openmetrics_metricpoint_historgram_size(4, m);
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        msg_size = openmetrics_metricpoint_historgram_size(4, m);
        break;
    }
    msg_size += openmetrics_timestamp_size(8, m->time);

    return buf_pb_size_type(field, PB_WIRE_TYPE_LENDELIM) +
           buf_pb_size_varint(msg_size) + msg_size;
}

static int openmetrics_metricpoint(buf_t *buf, int field, metric_family_t const *fam, metric_t const *m)
{
    size_t size = 0;

    switch(fam->type) {
    case METRIC_TYPE_UNKNOWN:
        size = openmetrics_metricpoint_unknown_size(1, m);
        break;
    case METRIC_TYPE_GAUGE:
        size = openmetrics_metricpoint_gauge_size(2, m);
        break;
    case METRIC_TYPE_COUNTER:
        size = openmetrics_metricpoint_counter_size(3, m);
        break;
    case METRIC_TYPE_STATE_SET:
        size = openmetrics_metricpoint_state_set_size(5, m);
        break;
    case METRIC_TYPE_INFO:
        size = openmetrics_metricpoint_info_size(6, m);
        break;
    case METRIC_TYPE_SUMMARY:
        size = openmetrics_metricpoint_summary_size(7, m);
        break;
    case METRIC_TYPE_HISTOGRAM:
        size = openmetrics_metricpoint_historgram_size(4, m);
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        size = openmetrics_metricpoint_historgram_size(4, m);
        break;
    }
    size += openmetrics_timestamp_size(8, m->time);

    int status = buf_pb_enc_type(buf, field, PB_WIRE_TYPE_LENDELIM);
    status |= buf_pb_enc_varint(buf, size);

    switch(fam->type) {
    case METRIC_TYPE_UNKNOWN:
        status |= openmetrics_metricpoint_unknown(buf, 1, m);
        break;
    case METRIC_TYPE_GAUGE:
        status |= openmetrics_metricpoint_gauge(buf, 2, m);
        break;
    case METRIC_TYPE_COUNTER:
        status |= openmetrics_metricpoint_counter(buf, 3, m);
        break;
    case METRIC_TYPE_STATE_SET:
        status |= openmetrics_metricpoint_state_set(buf, 5, m);
        break;
    case METRIC_TYPE_INFO:
        status |= openmetrics_metricpoint_info(buf, 6, m);
        break;
    case METRIC_TYPE_SUMMARY:
        status |= openmetrics_metricpoint_summary(buf, 7, m);
        break;
    case METRIC_TYPE_HISTOGRAM:
        status |= openmetrics_metricpoint_historgram(buf, 4, m);
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        status |= openmetrics_metricpoint_historgram(buf, 4, m);
        break;
    }
    status |= openmetrics_timestamp(buf, 8, m->time);

    return status;
}

/*
 *  message Metric {
 *      repeated Label labels              = 1; // Optional.
 *      repeated MetricPoint metric_points = 2; // Optional.
 *  }
 */
static int openmetrics_metric_size(int field, metric_family_t const *fam, metric_t const *m)
{
    size_t msg_size = openmetrics_label_size(1, m->label) +
                      openmetrics_metricpoint_size(2, fam, m);

    return buf_pb_size_type(field, PB_WIRE_TYPE_LENDELIM) +
           buf_pb_size_varint(msg_size) + msg_size;
}

static int openmetrics_metric(buf_t *buf, int field, metric_family_t const *fam, metric_t const *m)
{
    size_t size = openmetrics_label_size(1, m->label) +
                  openmetrics_metricpoint_size(2, fam,m);

    int status = buf_pb_enc_type(buf, field, PB_WIRE_TYPE_LENDELIM);
    status |= buf_pb_enc_varint(buf, size);
    status |= openmetrics_label(buf, 1, m->label);
    status |= openmetrics_metricpoint(buf, 2, fam, m);
    return status;
}

/*
 *  enum MetricType {
 *      UNKNOWN         = 0;
 *      GAUGE           = 1;
 *      COUNTER         = 2;
 *      STATE_SET       = 3;
 *      INFO            = 4;
 *      HISTOGRAM       = 5;
 *      GAUGE_HISTOGRAM = 6;
 *      SUMMARY         = 7;
 *  }
 *
 *
 *  message MetricFamily {
 *      string name             = 1; // Required.
 *      MetricType type         = 2; // Optional.
 *      string unit             = 3; // Optional.
 *      string help             = 4; // Optional.
 *      repeated Metric metrics = 5; // Optional.
 *  }
 */
int openmetrics_proto_metric_family(buf_t *buf, metric_family_t const *fam)
{
    uint32_t type = 0;
    switch(fam->type) {
    case METRIC_TYPE_UNKNOWN:
        type = 0;
        break;
    case METRIC_TYPE_GAUGE:
        type = 1;
        break;
    case METRIC_TYPE_COUNTER:
        type = 2;
        break;
    case METRIC_TYPE_STATE_SET:
        type = 3;
        break;
    case METRIC_TYPE_INFO:
        type = 4;
        break;
    case METRIC_TYPE_SUMMARY:
        type = 7;
        break;
    case METRIC_TYPE_HISTOGRAM:
        type = 5;
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        type = 6;
        break;
    }

    size_t size = buf_pb_size_str(1, fam->name);
    size += buf_pb_size_uint32(2, type);
    if (fam->unit != NULL)
        size += buf_pb_size_str(3, fam->unit);
    if (fam->help != NULL)
        size += buf_pb_size_str(4, fam->help);
    for (size_t i = 0; i < fam->metric.num; i++) {
        size += openmetrics_metric_size(5, fam, &fam->metric.ptr[i]);
    }

/*
 *  message MetricSet {
 *      repeated MetricFamily metric_families = 1;
 *  }
 */
    int field = 1;
    int status = buf_pb_enc_type(buf, field, PB_WIRE_TYPE_LENDELIM);
    status |= buf_pb_enc_varint(buf, size);
    status |= buf_pb_enc_str(buf, 1, fam->name);
    status |= buf_pb_enc_uint32(buf, 2, type);
    if (fam->unit != NULL)
        status |= buf_pb_enc_str(buf, 3, fam->unit);
    if (fam->help != NULL)
        status |= buf_pb_enc_str(buf, 4, fam->help);
    for (size_t i = 0; i < fam->metric.num; i++) {
        size += openmetrics_metric(buf, 5, fam, &fam->metric.ptr[i]);
    }

    return status;
}
