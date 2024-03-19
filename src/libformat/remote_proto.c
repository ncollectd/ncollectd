// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "libutils/common.h"
#include "libutils/dtoa.h"
#include "libutils/buf_pb.h"
#include "libutils/time.h"
#include "libmetric/metric.h"

/*
 *  message Label {
 *      string name  = 1;
 *      string value = 2;
 *  }
 */

static size_t remote_labels_size(int field, char *metric, char *metric_suffix,
                                 const label_set_t *labels1, const label_set_t *labels2)
{
    size_t len = 0;

    size_t msg_size = buf_pb_size_str(1, "__name__") +
                      buf_pb_size_str_str(2, metric, metric_suffix);
    len += buf_pb_size_type(field, PB_WIRE_TYPE_LENDELIM);
    len += buf_pb_size_varint(msg_size);
    len += msg_size;

    if (labels1 != NULL) {
        for (size_t i = 0; i < labels1->num; i++) {
            msg_size = buf_pb_size_str(1, labels1->ptr[i].name) +
                       buf_pb_size_str(2, labels1->ptr[i].value);
            len += buf_pb_size_type(field, PB_WIRE_TYPE_LENDELIM);
            len += buf_pb_size_varint(msg_size);
            len += msg_size;
        }
    }

    if (labels2 != NULL) {
        for (size_t i = 0; i < labels2->num; i++) {
            msg_size = buf_pb_size_str(1, labels2->ptr[i].name) +
                       buf_pb_size_str(2, labels2->ptr[i].value);
            len += buf_pb_size_type(field, PB_WIRE_TYPE_LENDELIM);
            len += buf_pb_size_varint(msg_size);
            len += msg_size;
        }
    }

    return len;
}

static int remote_labels(buf_t *buf, int field, char *metric, char *metric_suffix,
                         const label_set_t *labels1, const label_set_t *labels2)
{
    int status = 0;

    size_t size = buf_pb_size_str(1, "__name__") +
                  buf_pb_size_str_str(2, metric, metric_suffix);
    status |= buf_pb_enc_type(buf, field, PB_WIRE_TYPE_LENDELIM);
    status |= buf_pb_enc_varint(buf, size);
    status |= buf_pb_enc_str(buf, 1, "__name__");
    status |= buf_pb_enc_str_str(buf, 2, metric, metric_suffix);

    if (labels1 != NULL) {
        for (size_t i = 0; i < labels1->num; i++) {
            size = buf_pb_size_str(1, labels1->ptr[i].name) +
                   buf_pb_size_str(2, labels1->ptr[i].value);
            status |= buf_pb_enc_type(buf, field, PB_WIRE_TYPE_LENDELIM);
            status |= buf_pb_enc_varint(buf, size);
            status |= buf_pb_enc_str(buf, 1, labels1->ptr[i].name);
            status |= buf_pb_enc_str(buf, 2, labels1->ptr[i].value);
        }
    }

    if (labels2 != NULL) {
        for (size_t i = 0; i < labels2->num; i++) {
            size = buf_pb_size_str(1, labels2->ptr[i].name) +
                   buf_pb_size_str(2, labels2->ptr[i].value);
            status |= buf_pb_enc_type(buf, field, PB_WIRE_TYPE_LENDELIM);
            status |= buf_pb_enc_varint(buf, size);
            status |= buf_pb_enc_str(buf, 1, labels2->ptr[i].name);
            status |= buf_pb_enc_str(buf, 2, labels2->ptr[i].value);
        }
    }

    return status;
}

/*
 *  message Sample {
 *      double value    = 1;
 *      int64 timestamp = 2;
 *  }
 */
static int remote_sample_size(int field, double value, cdtime_t time)
{
    int64_t tm = CDTIME_T_TO_MS(time);

    size_t msg_size = buf_pb_size_double(1, value) +
                      buf_pb_size_int64(2, tm);
    return buf_pb_size_type(field, PB_WIRE_TYPE_LENDELIM) +
           buf_pb_size_varint(msg_size) + msg_size;
}

static int remote_sample(buf_t *buf, int field, double value, cdtime_t time)
{
    int64_t tm = CDTIME_T_TO_MS(time);

    size_t size = buf_pb_size_double(1, value) +
                  buf_pb_size_int64(2, tm);

    int status = buf_pb_enc_type(buf, field, PB_WIRE_TYPE_LENDELIM);
    status |= buf_pb_enc_varint(buf, size);
    status |= buf_pb_enc_double(buf, 1, value);
    status |= buf_pb_enc_int64(buf, 2, tm);
    return status;
}

/*
 *  message TimeSeries {
 *      repeated Label labels   = 1 [(gogoproto.nullable) = false];
 *      repeated Sample samples = 2 [(gogoproto.nullable) = false];
 *  }
 */

static int remote_timeseries_enc (buf_t *buf, int field, char *metric, char *metric_suffix,
                                  const label_set_t *labels1, const label_set_t *labels2,
                                  double value, cdtime_t time)
{
    size_t size = remote_sample_size(2, value, time) +
                  remote_labels_size(1, metric, metric_suffix, labels1, labels2);

    int status = buf_pb_enc_type(buf, field, PB_WIRE_TYPE_LENDELIM);
    status |= buf_pb_enc_varint(buf, size);
    status |= remote_labels(buf, 1, metric, metric_suffix, labels1, labels2);
    status |= remote_sample(buf, 2, value, time);
    return status;
}

static int remote_timeseries(buf_t *buf, metric_family_t const *fam)
{
    int field = 1;
    int status = 0;
    for (size_t i = 0; i < fam->metric.num; i++) {
        metric_t const *m = &fam->metric.ptr[i];
        switch(fam->type) {
        case METRIC_TYPE_UNKNOWN: {
            double value = m->value.unknown.type == UNKNOWN_FLOAT64 ? m->value.unknown.float64
                                                                    : m->value.unknown.int64;
            status |= remote_timeseries_enc(buf, field, fam->name, NULL, &m->label, NULL,
                                                 value, m->time);
        }   break;
        case METRIC_TYPE_GAUGE: {
            double value = m->value.gauge.type == GAUGE_FLOAT64 ? m->value.gauge.float64
                                                                : m->value.gauge.int64;

            status |= remote_timeseries_enc(buf, field, fam->name, NULL, &m->label, NULL,
                                                 value, m->time);
        }   break;
        case METRIC_TYPE_COUNTER: {
            double value = m->value.counter.type == COUNTER_UINT64 ? m->value.counter.uint64
                                                                   : m->value.counter.float64;

            status |= remote_timeseries_enc(buf, field, fam->name, "_total", &m->label, NULL,
                                                 value, m->time);
        }   break;
        case METRIC_TYPE_STATE_SET:
            for (size_t j = 0; j < m->value.state_set.num; j++) {
                label_pair_t label_pair = {.name = fam->name,
                                           .value = m->value.state_set.ptr[j].name};
                label_set_t label_set = {.num = 1, .ptr = &label_pair};
                double value = m->value.state_set.ptr[j].enabled ? 1.0 : 0.0;
                status |= remote_timeseries_enc(buf, field, fam->name, NULL, &m->label, &label_set,
                                                     value, m->time);
            }
            break;
        case METRIC_TYPE_INFO:
            status |= remote_timeseries_enc(buf, field, fam->name, "_info",
                                                 &m->label, &m->value.info, 1, m->time);
            break;
        case METRIC_TYPE_SUMMARY:
            for (int j = m->value.summary->num - 1; j >= 0; j--) {
                char quantile[DTOA_MAX];
                dtoa(m->value.summary->quantiles[j].quantile, quantile, sizeof(quantile));
                label_pair_t label_pair = {.name = "quantile", .value = quantile};
                label_set_t label_set = {.num = 1, .ptr = &label_pair};
                status |= remote_timeseries_enc(buf, field, fam->name, NULL, &m->label, &label_set,
                                                     m->value.summary->quantiles[j].value, m->time);
            }
            status |= remote_timeseries_enc(buf, field, fam->name, "_count", &m->label, NULL,
                                                 m->value.summary->count, m->time);
            status |= remote_timeseries_enc(buf, field, fam->name, "_sum", &m->label, NULL,
                                                 m->value.summary->sum, m->time);
            break;
        case METRIC_TYPE_HISTOGRAM:
        case METRIC_TYPE_GAUGE_HISTOGRAM:
            for (int j = m->value.histogram->num - 1; j >= 0; j--) {
                char le[DTOA_MAX];
                dtoa(m->value.histogram->buckets[j].maximum, le, sizeof(le));
                label_pair_t label_pair = {.name = "le", .value = le};
                label_set_t label_set = {.num = 1, .ptr = &label_pair};
                status |= remote_timeseries_enc(buf, field, fam->name, "_bucket",
                                                     &m->label, &label_set,
                                                     m->value.histogram->buckets[j].counter,
                                                     m->time);
            }
            status |= remote_timeseries_enc(buf, field, fam->name,
                                                 fam->type == METRIC_TYPE_HISTOGRAM ? "_count"
                                                                                    : "_gcount",
                                                 &m->label, NULL,
                                                 histogram_counter(m->value.histogram), m->time);
            status |= remote_timeseries_enc(buf, field, fam->name,
                                                 fam->type == METRIC_TYPE_HISTOGRAM ? "_sum"
                                                                                    : "_gsum",
                                                 &m->label, NULL,
                                                 histogram_sum(m->value.histogram), m->time);
            break;
        }
    }
    return status;
}

/*
 *  message MetricMetadata {
 *      enum MetricType {
 *          UNKNOWN        = 0;
 *          COUNTER        = 1;
 *          GAUGE          = 2;
 *          HISTOGRAM      = 3;
 *          GAUGEHISTOGRAM = 4;
 *          SUMMARY        = 5;
 *          INFO           = 6;
 *          STATESET       = 7;
 *      }
 *      MetricType type           = 1;
 *      string metric_family_name = 2;
 *      string help               = 4;
 *      string unit               = 5;
 *  }
 */

static int remote_metricmetadata(buf_t *buf, metric_family_t const *fam)
{
    uint32_t type = 0;

    switch(fam->type) {
    case METRIC_TYPE_UNKNOWN:
        type = 0;
        break;
    case METRIC_TYPE_GAUGE:
        type = 2;
        break;
    case METRIC_TYPE_COUNTER:
        type = 1;
        break;
    case METRIC_TYPE_STATE_SET:
        type = 7;
        break;
    case METRIC_TYPE_INFO:
        type = 6;
        break;
    case METRIC_TYPE_SUMMARY:
        type = 5;
        break;
    case METRIC_TYPE_HISTOGRAM:
        type = 3;
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        type = 4;
        break;
    }

    size_t size = buf_pb_size_uint32(1, type);
    size +=  buf_pb_size_str(2, fam->name);
    if (fam->help != NULL)
        size +=  buf_pb_size_str(4, fam->help);
    if (fam->unit != NULL)
        size += buf_pb_size_str(5, fam->unit);

    int field = 3;
    int status = buf_pb_enc_type(buf, field, PB_WIRE_TYPE_LENDELIM);
    status |= buf_pb_enc_varint(buf, size);
    status |= buf_pb_enc_uint32(buf, 1, type);
    status |= buf_pb_enc_str(buf, 2, fam->name);
    if (fam->help != NULL)
        status |= buf_pb_enc_str(buf, 4, fam->help);
    if (fam->unit != NULL)
        status |= buf_pb_enc_str(buf, 5, fam->unit);

    return status;
}

/*
 *  message WriteRequest {
 *      repeated prometheus.TimeSeries timeseries = 1 [(gogoproto.nullable) = false];
 *      repeated prometheus.MetricMetadata metadata = 3 [(gogoproto.nullable) = false];
 *  }
 */
int remote_proto_metric_family(buf_t *buf, metric_family_t const *fam, bool metadata)
{
    int status = 0;

    if (metadata)
        status = remote_metricmetadata(buf, fam);

    status |= remote_timeseries(buf, fam);

    return status;
}
