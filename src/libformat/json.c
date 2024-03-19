// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "libmetric/metric.h"
#include "libutils/common.h"
#include "libxson/render.h"

/*
  {
    "metric": "name",
    "help": "help text",
    "unit": "unit",
    "type": "gauge",
    "metrics": [
        {
            "labels": {
                "key1": "value1",
                "key2": "value2",
            }
            "timestamp": 124
            "interval": 10
            "value": 123
        },
     ]
   }
*/

static inline int json_labels(xson_render_t *r, label_set_t const *labels, cdtime_t time,
                                                cdtime_t interval)
{
    int status = 0;

    status |= xson_render_key_string(r, "labels");
    status |= xson_render_map_open(r);
    for (size_t i = 0; i < labels->num; i++) {
        status |= xson_render_key_string(r, labels->ptr[i].name);
        status |= xson_render_string(r, labels->ptr[i].value);
    }
    status |= xson_render_map_close(r);

    status |= xson_render_key_string(r, "timestamp");
    status |= xson_render_integer(r, CDTIME_T_TO_MS(time));

    status |= xson_render_key_string(r, "interval");
    status |= xson_render_integer(r, CDTIME_T_TO_MS(interval));

    return status;
}

static int json_unknown(xson_render_t *r, metric_t const *m)
{
    int status = json_labels(r, &m->label, m->time, m->interval);
    status |= xson_render_key_string(r, "value");
    if (m->value.unknown.type == UNKNOWN_FLOAT64)
        return status | xson_render_double(r, m->value.unknown.float64);
    return status | xson_render_integer(r, m->value.unknown.int64);
}

static int json_gauge(xson_render_t *r, metric_t const *m)
{
    int status = json_labels(r, &m->label, m->time, m->interval);
    status |= xson_render_key_string(r, "value");
    if (m->value.gauge.type == GAUGE_FLOAT64)
        return status | xson_render_double(r, m->value.gauge.float64);
    return status | xson_render_integer(r, m->value.gauge.int64);
}

static int json_counter(xson_render_t *r, metric_t const *m)
{
    int status = json_labels(r, &m->label, m->time, m->interval);
    status |= xson_render_key_string(r, "value");
    if (m->value.counter.type == COUNTER_UINT64)
        return status | xson_render_integer(r, m->value.counter.uint64);
    return status | xson_render_double(r, m->value.counter.float64);
}

static int json_info(xson_render_t *r, metric_t const *m)
{
    int status = json_labels(r, &m->label, m->time, m->interval);
    status |= xson_render_key_string(r, "info");
    status |= xson_render_map_open(r);
    for (size_t i = 0; i < m->value.info.num; i++) {
        status |= xson_render_key_string(r, m->value.info.ptr[i].name);
        status |= xson_render_string(r, m->value.info.ptr[i].value);
    }
    return status | xson_render_map_close(r);
}

static int json_state_set(xson_render_t *r, metric_t const *m)
{
    int status = json_labels(r, &m->label, m->time, m->interval);
    status |= xson_render_key_string(r, "stateset");
    status |= xson_render_map_open(r);
    for (size_t i = 0; i < m->value.state_set.num; i++) {
        status |= xson_render_key_string(r, m->value.state_set.ptr[i].name);
        status |= xson_render_bool(r, m->value.state_set.ptr[i].enabled);
    }
    return status | xson_render_map_close(r);
}

static int json_summary(xson_render_t *r, metric_t const *m)
{
    int status = json_labels(r, &m->label, m->time, m->interval);

    status |= xson_render_key_string(r, "quantiles");
    xson_render_array_open(r);
    for (int i = m->value.summary->num - 1; i >= 0; i--) {
        xson_render_array_open(r);
        status |= xson_render_double(r, m->value.summary->quantiles[i].quantile);
        status |= xson_render_double(r, m->value.summary->quantiles[i].value);
        xson_render_array_close(r);
    }
    xson_render_array_close(r);

    status |= xson_render_key_string(r, "count");
    status |= xson_render_integer(r, m->value.summary->count);

    status |= xson_render_key_string(r, "sum");
    status |= xson_render_integer(r, m->value.summary->sum);
    return status;
}

static int json_histogram(xson_render_t *r, metric_t const *m)
{
    int status = json_labels(r, &m->label, m->time, m->interval);

    status |= xson_render_key_string(r, "buckets");
    xson_render_array_open(r);
    for (int i = m->value.histogram->num - 1; i >= 0; i--) {
        xson_render_array_open(r);
        status |= xson_render_double(r, m->value.histogram->buckets[i].maximum);
        status |= xson_render_integer(r, m->value.histogram->buckets[i].counter);
        xson_render_array_close(r);
    }
    xson_render_array_close(r);

    status |= xson_render_key_string(r, "count");
    status |= xson_render_integer(r, histogram_counter(m->value.histogram));

    status |= xson_render_key_string(r, "sum");
    status |= xson_render_double(r, histogram_counter(m->value.histogram));
    return status;
}

static int json_gauge_histogram(xson_render_t *r, metric_t const *m)
{
    int status = json_labels(r, &m->label, m->time, m->interval);

    status |= xson_render_key_string(r, "buckets");
    xson_render_array_open(r);
    for (int i = m->value.histogram->num - 1; i >= 0; i--) {
        xson_render_array_open(r);
        status |= xson_render_double(r, m->value.histogram->buckets[i].counter);
        status |= xson_render_integer(r, m->value.histogram->buckets[i].counter);
        xson_render_array_close(r);
    }
    xson_render_array_close(r);

    status |= xson_render_key_string(r, "gcount");
    status |= xson_render_integer(r, histogram_counter(m->value.histogram));

    status |= xson_render_key_string(r, "gsum");
    status |= xson_render_double(r, histogram_counter(m->value.histogram));

    return status;
}

int json_metric_family(strbuf_t *buf, metric_family_t const *fam)
{
    if ((buf == NULL) || (fam == NULL))
        return EINVAL;

    if (fam->metric.num == 0)
        return 0;

    xson_render_t r = {0};
    xson_render_init(&r, buf, XSON_RENDER_TYPE_JSON, 0);

    int status = xson_render_map_open(&r);

    status |= xson_render_key_string(&r, "metric");
    status |= xson_render_string(&r, fam->name);

    status |= xson_render_key_string(&r, "type");
    switch(fam->type) {
    case METRIC_TYPE_UNKNOWN:
        status |= xson_render_string(&r, "unknown");
        break;
    case METRIC_TYPE_GAUGE:
        status |= xson_render_string(&r, "gauge");
        break;
    case METRIC_TYPE_COUNTER:
        status |= xson_render_string(&r, "counter");
        break;
    case METRIC_TYPE_STATE_SET:
        status |= xson_render_string(&r, "stateset");
        break;
    case METRIC_TYPE_INFO:
        status |= xson_render_string(&r, "info");
        break;
    case METRIC_TYPE_SUMMARY:
        status |= xson_render_string(&r, "summary");
        break;
    case METRIC_TYPE_HISTOGRAM:
        status |= xson_render_string(&r, "histogram");
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        status |= xson_render_string(&r, "gaugehistogram");
        break;
    }

    if (fam->help != NULL) {
        status |= xson_render_key_string(&r, "help");
        status |= xson_render_string(&r, fam->help);
    }

    if (fam->unit != NULL) {
        status |= xson_render_key_string(&r, "unit");
        status |= xson_render_string(&r, fam->unit);
    }

    if (status != 0)
        return status;

    status |= xson_render_key_string(&r, "metrics");
    status |= xson_render_array_open(&r);
    for (size_t i = 0; i < fam->metric.num; i++) {
        metric_t *m = &fam->metric.ptr[i];
        status |= xson_render_map_open(&r);
        switch(fam->type) {
        case METRIC_TYPE_UNKNOWN:
            status |= json_unknown(&r, m);
            break;
        case METRIC_TYPE_GAUGE:
            status |= json_gauge(&r, m);
            break;
        case METRIC_TYPE_COUNTER:
            status |= json_counter(&r, m);
            break;
        case METRIC_TYPE_STATE_SET:
            status |= json_state_set(&r, m);
            break;
        case METRIC_TYPE_INFO:
            status |= json_info(&r, m);
            break;
        case METRIC_TYPE_SUMMARY:
            status |= json_summary(&r, m);
            break;
        case METRIC_TYPE_HISTOGRAM:
            status |= json_histogram(&r, m);
            break;
        case METRIC_TYPE_GAUGE_HISTOGRAM:
            status |= json_gauge_histogram(&r, m);
            break;
        }
        status |= xson_render_map_close(&r);
        if (status != 0)
            return status;
    }
    status |= xson_render_array_close(&r);

    status |= xson_render_map_close(&r);

    return status;
}
