// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "libmetric/metric.h"
#include "libutils/common.h"
#include "libutils/dtoa.h"
#include "libxson/render.h"

static inline int opentelemetry_label_pair(xson_render_t *r, label_pair_t label) 
{
    int status = 0;
 
    status |= xson_render_map_open(r); 
    status |= xson_render_key_string(r, "key");
    status |= xson_render_string(r, label.name);
    status |= xson_render_key_string(r, "value");
    status |= xson_render_map_open(r); 
    status |= xson_render_key_string(r, "stringValue");
    status |= xson_render_string(r, label.value);
    status |= xson_render_map_close(r); 
    status |= xson_render_map_close(r); 

    return status;
}

static inline int opentelemetry_attributes(xson_render_t *r, const label_set_t *labels1,
                                                             const label_set_t *labels2)
{
    int status = 0;
 
    if (((labels1 != NULL) && (labels1->num > 0)) || ((labels2 != NULL) && (labels2->num > 0))) {
        status |= xson_render_key_string(r, "attributes");
        status |= xson_render_array_open(r);
        if (labels1 != NULL) {
            for (size_t i = 0; i < labels1->num; i++) {
                status |= opentelemetry_label_pair(r, labels1->ptr[i]);
            }
        }
        if (labels2 != NULL) {
            for (size_t i = 0; i < labels2->num; i++) {
                status |= opentelemetry_label_pair(r, labels2->ptr[i]);
            }
        }
        status |= xson_render_array_close(r);
    }

    return status;
}

static int opentelemetry_unknown(xson_render_t *r, metric_t const *m)
{
    int status = xson_render_map_open(r);
    if (m->value.unknown.type == UNKNOWN_FLOAT64) {
        status |= xson_render_key_string(r, "asDouble");
        status |= xson_render_double(r, m->value.unknown.float64);
    } else {
        status |= xson_render_key_string(r, "asInt");
        status |= xson_render_integer(r, m->value.unknown.int64);
    }
    status |= xson_render_key_string(r, "timeUnixNano");
    status |= xson_render_integer(r, CDTIME_T_TO_NS(m->time));
    status |= opentelemetry_attributes(r, &m->label, NULL);
    return status | xson_render_map_close(r);
}

static int opentelemetry_gauge(xson_render_t *r, metric_t const *m)
{
    int status = xson_render_map_open(r);
    if (m->value.gauge.type == GAUGE_FLOAT64) {
        status |= xson_render_key_string(r, "asDouble");
        status |= xson_render_double(r, m->value.gauge.float64);
    } else {
        status |= xson_render_key_string(r, "asInt");
        status |= xson_render_integer(r, m->value.gauge.int64);
    }
    status |= xson_render_key_string(r, "timeUnixNano");
    status |= xson_render_integer(r, CDTIME_T_TO_NS(m->time));
    status |= opentelemetry_attributes(r, &m->label, NULL);
    return status | xson_render_map_close(r);
}

static int opentelemetry_counter(xson_render_t *r, metric_t const *m)
{
    int status = xson_render_map_open(r);
    status |= xson_render_key_string(r, "isMonotonic");
    status |= xson_render_bool(r, true);
    if (m->value.counter.type == COUNTER_UINT64) {
        status |= xson_render_key_string(r, "asInt");
        status |= xson_render_integer(r, m->value.counter.uint64);
    } else {
        status |= xson_render_key_string(r, "asDouble");
        status |= xson_render_double(r, m->value.counter.float64);
    }
    status |= xson_render_key_string(r, "timeUnixNano");
    status |= xson_render_integer(r, CDTIME_T_TO_NS(m->time));
    status |= opentelemetry_attributes(r, &m->label, NULL);
    return status | xson_render_map_close(r);
}

static int opentelemetry_state_set(xson_render_t *r, char *name, metric_t const *m)
{
    int status = 0;

    for (size_t i = 0; i < m->value.state_set.num; i++) {
        status |= xson_render_map_open(r);
        status |= xson_render_key_string(r, "isMonotonic");
        status |= xson_render_bool(r, false);
        status |= xson_render_key_string(r, "asInt");
        status |= xson_render_integer(r, m->value.state_set.ptr[i].enabled ? 1 : 0);
        status |= xson_render_key_string(r, "timeUnixNano");
        status |= xson_render_integer(r, CDTIME_T_TO_NS(m->time));
        label_pair_t label_pair = {.name = name,
                                   .value = m->value.state_set.ptr[i].name};
        label_set_t label_set = {.num = 1, .ptr = &label_pair};
        status |= opentelemetry_attributes(r, &m->label, &label_set);
        status |=xson_render_map_close(r);
    }
    return status;
}

static int opentelemetry_info(xson_render_t *r, metric_t const *m)
{
    int status = xson_render_map_open(r);
    status |= xson_render_key_string(r, "asInt");
    status |= xson_render_integer(r, 1);
    status |= xson_render_key_string(r, "timeUnixNano");
    status |= xson_render_integer(r, CDTIME_T_TO_NS(m->time));
    status |= opentelemetry_attributes(r, &m->label, &m->value.info);
    return status | xson_render_map_close(r);
}

static int opentelemetry_summary(xson_render_t *r, metric_t const *m)
{
    int status = xson_render_map_open(r);

    status |= xson_render_key_string(r, "ValueAtQuantile");
    status |= xson_render_map_open(r);
    for (int i = m->value.summary->num - 1; i >= 0; i--) {
        status |= xson_render_key_string(r, "quantile");
        status |= xson_render_double(r, m->value.summary->quantiles[i].quantile);
        status |= xson_render_key_string(r, "value");
        status |= xson_render_double(r, m->value.summary->quantiles[i].value);
    }
    status |= xson_render_map_close(r);

    status |= xson_render_key_string(r, "count");
    status |= xson_render_integer(r,  m->value.summary->count);
    status |= xson_render_key_string(r, "sum");
    status |= xson_render_integer(r, m->value.summary->sum);
    status |= xson_render_key_string(r, "timeUnixNano");
    status |= xson_render_integer(r, CDTIME_T_TO_NS(m->time));
    status |= opentelemetry_attributes(r, &m->label, &m->value.info);

    return status | xson_render_map_close(r);
}

static int opentelemetry_histogram(xson_render_t *r, metric_t const *m)
{
    int status = xson_render_map_open(r);

    status |= xson_render_key_string(r, "bucketCounts");   
    status |= xson_render_array_open(r);
    for (int i = m->value.histogram->num - 1; i >= 0; i--) {
        status |= xson_render_integer(r, m->value.histogram->buckets[i].counter);
    }    
    status |= xson_render_array_close(r);
    
    status |= xson_render_key_string(r, "explicitBounds");
    status |= xson_render_array_open(r);
    for (int i = m->value.histogram->num - 1; i > 0; i--) {
        status |= xson_render_double(r, m->value.histogram->buckets[i].maximum);
    }
    status |= xson_render_array_close(r);

    status |= xson_render_key_string(r, "count");
    status |= xson_render_integer(r, histogram_counter(m->value.histogram));
    status |= xson_render_key_string(r, "sum");
    status |= xson_render_double(r, histogram_sum(m->value.histogram));
    status |= xson_render_key_string(r, "timeUnixNano");
    status |= xson_render_integer(r, CDTIME_T_TO_NS(m->time));
    status |= opentelemetry_attributes(r, &m->label, &m->value.info);

    return status | xson_render_map_close(r);
}

int opentelemetry_json_metric(xson_render_t *r, metric_family_t const *fam)
{
    int status = 0;

    status |= xson_render_map_open(r);

    status |= xson_render_key_string(r, "name");
    status |= xson_render_string(r, fam->name);
    if (fam->unit != NULL) {
        status |= xson_render_key_string(r, "unit");
        status |= xson_render_string(r, fam->unit);
    }
    if (fam->help != NULL) {
        status |= xson_render_key_string(r, "description");
        status |= xson_render_string(r, fam->help);
    }

    switch(fam->type) {
    case METRIC_TYPE_UNKNOWN:
        status |= xson_render_key_string(r, "gauge");
        status |= xson_render_map_open(r);
        status |= xson_render_key_string(r, "dataPoints");
        status |= xson_render_array_open(r);
        for (size_t i=0; i < fam->metric.num ; i++) {
            status |= opentelemetry_unknown(r, &fam->metric.ptr[i]);
        }
        status |= xson_render_array_close(r);
        status |= xson_render_map_close(r);
        break;
    case METRIC_TYPE_GAUGE:
        status |= xson_render_key_string(r, "gauge");
        status |= xson_render_map_open(r);
        status |= xson_render_key_string(r, "dataPoints");
        status |= xson_render_array_open(r);
        for (size_t i=0; i < fam->metric.num ; i++) {
            status |= opentelemetry_gauge(r, &fam->metric.ptr[i]);
        }
        status |= xson_render_array_close(r);
        status |= xson_render_map_close(r);
        break;
    case METRIC_TYPE_COUNTER:
        status |= xson_render_key_string(r, "sum");
        status |= xson_render_map_open(r);
        status |= xson_render_key_string(r, "aggregationTemporality"); 
        status |= xson_render_integer(r, 2); /* AGGREGATION_TEMPORALITY_CUMULATIVE */
        status |= xson_render_key_string(r, "dataPoints");
        status |= xson_render_array_open(r);
        for (size_t i=0; i < fam->metric.num ; i++) {
            status |= opentelemetry_counter(r, &fam->metric.ptr[i]);
        }
        status |= xson_render_array_close(r);
        status |= xson_render_map_close(r);
        break;
    case METRIC_TYPE_STATE_SET:
        status |= xson_render_key_string(r, "sum");
        status |= xson_render_map_open(r);
        status |= xson_render_key_string(r, "dataPoints");
        status |= xson_render_array_open(r);
        for (size_t i=0; i < fam->metric.num ; i++) {
            status |= opentelemetry_state_set(r, fam->name, &fam->metric.ptr[i]);
        }
        status |= xson_render_array_close(r);
        status |= xson_render_map_close(r);
        break;
    case METRIC_TYPE_INFO:
        status |= xson_render_key_string(r, "gauge");
        status |= xson_render_map_open(r);
        status |= xson_render_key_string(r, "dataPoints");
        status |= xson_render_array_open(r);
        for (size_t i=0; i < fam->metric.num ; i++) {
            status |= opentelemetry_info(r, &fam->metric.ptr[i]);
        }
        status |= xson_render_array_close(r);
        status |= xson_render_map_close(r);
        break;
    case METRIC_TYPE_SUMMARY:
        status |= xson_render_key_string(r, "summary");
        status |= xson_render_map_open(r);
        status |= xson_render_key_string(r, "dataPoints");
        status |= xson_render_array_open(r);
        for (size_t i=0; i < fam->metric.num ; i++) {
            status |= opentelemetry_summary(r, &fam->metric.ptr[i]);
        }
        status |= xson_render_array_close(r);
        status |= xson_render_map_close(r);
        break;
    case METRIC_TYPE_HISTOGRAM:
        status |= xson_render_key_string(r, "histogram");
        status |= xson_render_map_open(r);
        status |= xson_render_key_string(r, "aggregationTemporality"); 
        status |= xson_render_integer(r, 2); /* AGGREGATION_TEMPORALITY_CUMULATIVE */
        status |= xson_render_key_string(r, "dataPoints");
        status |= xson_render_array_open(r);
        for (size_t i=0; i < fam->metric.num ; i++) {
            status |= opentelemetry_histogram(r, &fam->metric.ptr[i]);
        }
        status |= xson_render_array_close(r);
        status |= xson_render_map_close(r);
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        break;
    }

    status |= xson_render_map_close(r);

    return status;
}

int opentelemetry_json_metric_family(strbuf_t *buf, metric_family_t const *fam)
{
    if (fam->type == METRIC_TYPE_GAUGE_HISTOGRAM) /* unsuported */
        return 0;

    int status = 0;

    xson_render_t r = {0};
    xson_render_init(&r, buf, XSON_RENDER_TYPE_JSON, 0);

    status |= xson_render_map_open(&r);
    status |= xson_render_key_string(&r, "resourceMetrics");
    status |= xson_render_array_open(&r);
   
    status |= xson_render_map_open(&r);
#if 0
    status |= xson_render_key_string(&r, "resource");
#endif
    status |= xson_render_key_string(&r, "scopeMetrics");
    status |= xson_render_map_open(&r);

    status |= xson_render_key_string(&r, "scope");
    status |= xson_render_map_open(&r);
    status |= xson_render_key_string(&r, "name");
    status |= xson_render_string(&r, PACKAGE_NAME);
    status |= xson_render_key_string(&r, "version");
    status |= xson_render_string(&r, PACKAGE_VERSION);
    status |= xson_render_map_close(&r);

    status |= xson_render_key_string(&r, "metrics");
    status |= xson_render_array_open(&r);
    status |= opentelemetry_json_metric(&r, fam);
    status |= xson_render_array_close(&r);

    status |= xson_render_map_close(&r);

    status |= xson_render_map_close(&r);

    status |= xson_render_array_close(&r);
    status |= xson_render_map_close(&r);

    return status;
}
