// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "libmetric/metric.h"
#include "libutils/common.h"
#include "libutils/dtoa.h"
#include "libxson/render.h"

/* This is the KAIROSDB format for write_http output
 *
 * Target format
 * [
 *   {
 *       "name":"cpu_usage"
 *       "timestamp": 1453897164060,
 *       "value": 97.1,
 *       "ttl": 300,
 *       "tags": {
 *                  "instance": "example.com",
 *                  "cpu":          "0",
 *                  "state":        "idle"
 *       }
 *   }
 * ]
*/

static inline int opentsdb_metric(xson_render_t *r, char *metric, char *metric_suffix,
                                  const label_set_t *labels1, const label_set_t *labels2,
                                  cdtime_t time, int ttl)
{
    int status = 0;

    // status |= xson_render_key_string(r, "name");
    status |= xson_render_key_string(r, "metric");
    if (metric_suffix != NULL) {
        struct iovec iov[] = {
            { .iov_base = metric, .iov_len = strlen(metric) },
            { .iov_base = metric_suffix, .iov_len = strlen(metric_suffix) }
        };
        status |= xson_render_iov(r, iov, 2);
    } else {
        status |= xson_render_string(r, metric);
    }

    if (((labels1 != NULL) && (labels1->num > 0)) || ((labels2 != NULL) && (labels2->num > 0))) {
        status |= xson_render_key_string(r, "tags");
        status |= xson_render_map_open(r);
        if (labels1 != NULL) {
            for (size_t i = 0; i < labels1->num; i++) {
                status |= xson_render_key_string(r, labels1->ptr[i].name);
                status |= xson_render_string(r, labels1->ptr[i].value);
            }
        }
        if (labels2 != NULL) {
            for (size_t i = 0; i < labels2->num; i++) {
                status |= xson_render_key_string(r, labels2->ptr[i].name);
                status |= xson_render_string(r, labels2->ptr[i].value);
            }
        }
        status |= xson_render_map_close(r);
    }

    status |= xson_render_key_string(r, "timestamp");
    status |= xson_render_integer(r, CDTIME_T_TO_MS(time));

    if (ttl != 0) {
        status |= xson_render_key_string(r, "ttl");
        status |= xson_render_integer(r, ttl);
    }

    return status;
}

static int opentsdb_unknown(xson_render_t *r, metric_family_t const *fam, metric_t const *m, int ttl)
{
    int status = xson_render_map_open(r);
    status |= opentsdb_metric(r, fam->name, NULL, &m->label, NULL, m->time, ttl);
    status |= xson_render_key_string(r, "value");
    if (m->value.unknown.type == UNKNOWN_FLOAT64)
        status |= xson_render_double(r, m->value.unknown.float64);
    else
        status |= xson_render_integer(r, m->value.unknown.int64);
    return status | xson_render_map_close(r);
}

static int opentsdb_gauge(xson_render_t *r, metric_family_t const *fam, metric_t const *m, int ttl)
{
    int status = xson_render_map_open(r);
    status |= opentsdb_metric(r, fam->name, NULL, &m->label, NULL, m->time, ttl);
    status |= xson_render_key_string(r, "value");
    if (m->value.gauge.type == GAUGE_FLOAT64)
        status |= xson_render_double(r, m->value.gauge.float64);
    else
        status |= xson_render_integer(r, m->value.gauge.int64);
    return status | xson_render_map_close(r);
}

static int opentsdb_counter(xson_render_t *r, metric_family_t const *fam, metric_t const *m, int ttl)
{
    int status = xson_render_map_open(r);
    status |= opentsdb_metric(r, fam->name, "_total", &m->label, NULL, m->time, ttl);
    status |= xson_render_key_string(r, "value");
    if (m->value.counter.type == COUNTER_UINT64)
        status |= xson_render_integer(r, m->value.counter.uint64);
    else
        status |= xson_render_double(r, m->value.counter.float64);
    return status | xson_render_map_close(r);
}

static int opentsdb_state_set(xson_render_t *r, metric_family_t const *fam, metric_t const *m, int ttl)
{
    int status = 0;

    for (size_t i = 0; i < m->value.state_set.num; i++) {
        status |= xson_render_map_open(r);

        label_pair_t label_pair = {.name = fam->name,
                                   .value = m->value.state_set.ptr[i].name};
        label_set_t label_set = {.num = 1, .ptr = &label_pair};

        status |= opentsdb_metric(r, fam->name, NULL, &m->label, &label_set, m->time, ttl);

        status |= xson_render_key_string(r, "value");
        status |= xson_render_integer(r, m->value.state_set.ptr[i].enabled ? 1 : 0);

        status |=xson_render_map_close(r);
    }
    return status;
}

static int opentsdb_info(xson_render_t *r, metric_family_t const *fam, metric_t const *m, int ttl)
{
    int status = xson_render_map_open(r);
    status |= opentsdb_metric(r, fam->name, "_info", &m->label, &m->value.info, m->time, ttl);
    status |= xson_render_key_string(r, "value");
    status |= xson_render_integer(r, 1);
    return status | xson_render_map_close(r);
}

static int opentsdb_summary(xson_render_t *r, metric_family_t const *fam, metric_t const *m, int ttl)
{
    int status = 0;

    for (int i = m->value.summary->num - 1; i >= 0; i--) {
        char quantile[DTOA_MAX];

        dtoa(m->value.summary->quantiles[i].quantile, quantile, sizeof(quantile));

        label_pair_t label_pair = {.name = "quantile", .value = quantile};
        label_set_t label_set = {.num = 1, .ptr = &label_pair};

        status |= xson_render_map_open(r);
        status |= opentsdb_metric(r, fam->name, NULL, &m->label, &label_set, m->time, ttl);
        status |= xson_render_key_string(r, "value");
        status |= xson_render_double(r, m->value.summary->quantiles[i].value);
        status |= xson_render_map_close(r);
    }

    status |= xson_render_map_open(r);
    status |= opentsdb_metric(r, fam->name, "_count", &m->label, NULL, m->time, ttl);
    status |= xson_render_key_string(r, "value");
    status |= xson_render_integer(r, m->value.summary->count);
    status |= xson_render_map_close(r);

    status |= xson_render_map_open(r);
    status |= opentsdb_metric(r, fam->name, "_sum", &m->label, NULL, m->time, ttl);
    status |= xson_render_key_string(r, "value");
    status |= xson_render_integer(r, m->value.summary->sum);
    status |= xson_render_map_close(r);

    return status;
}

static int opentsdb_histogram(xson_render_t *r, metric_family_t const *fam, metric_t const *m, int ttl)
{
    int status = 0;

    for (int i = m->value.histogram->num - 1; i >= 0; i--) {
        char le[DTOA_MAX];

        dtoa(m->value.histogram->buckets[i].maximum, le, sizeof(le));

        label_pair_t label_pair = {.name = "le", .value = le};
        label_set_t label_set = {.num = 1, .ptr = &label_pair};

        status |= xson_render_map_open(r);
        status |= opentsdb_metric(r, fam->name, NULL, &m->label, &label_set, m->time, ttl);
        status |= xson_render_key_string(r, "value");
        status |= xson_render_integer(r, m->value.histogram->buckets[i].counter);
        status |= xson_render_map_close(r);
    }

    status |= xson_render_map_open(r);
    status |= opentsdb_metric(r, fam->name, "_count", &m->label, NULL, m->time, ttl);
    status |= xson_render_key_string(r, "value");
    status |= xson_render_integer(r, histogram_counter(m->value.histogram));
    status |= xson_render_map_close(r);

    status |= xson_render_map_open(r);
    status |= opentsdb_metric(r, fam->name, "_sum", &m->label, NULL, m->time, ttl);
    status |= xson_render_key_string(r, "value");
    status |= xson_render_double(r,histogram_sum(m->value.histogram));
    status |= xson_render_map_close(r);

    return status;
}

static int opentsdb_gauge_histogram(xson_render_t *r, metric_family_t const *fam, metric_t const *m, int ttl)
{
    int status = 0;

    for (int i = m->value.histogram->num - 1; i >= 0; i--) {
        char le[DTOA_MAX];

        dtoa(m->value.histogram->buckets[i].maximum, le, sizeof(le));

        label_pair_t label_pair = {.name = "le", .value = le};
        label_set_t label_set = {.num = 1, .ptr = &label_pair};

        status |= xson_render_map_open(r);
        status |= opentsdb_metric(r, fam->name, NULL, &m->label, &label_set, m->time, ttl);
        status |= xson_render_key_string(r, "value");
        status |= xson_render_integer(r, m->value.histogram->buckets[i].counter);
        status |= xson_render_map_close(r);
    }

    status |= xson_render_map_open(r);
    status |= opentsdb_metric(r, fam->name, "_gcount", &m->label, NULL, m->time, ttl);
    status |= xson_render_key_string(r, "value");
    status |= xson_render_integer(r, histogram_counter(m->value.histogram));
    status |= xson_render_map_close(r);

    status |= xson_render_map_open(r);
    status |= opentsdb_metric(r, fam->name, "_gsum", &m->label, NULL, m->time, ttl);
    status |= xson_render_key_string(r, "value");
    status |= xson_render_double(r, histogram_sum(m->value.histogram));
    status |= xson_render_map_close(r);

    return status;
}

int opentsdb_json_metric(xson_render_t *r, metric_family_t const *fam, metric_t const *m, int ttl)
{
    int status = 0;

    switch(fam->type) {
    case METRIC_TYPE_UNKNOWN:
        status |= opentsdb_unknown(r, fam, m, ttl);
        break;
    case METRIC_TYPE_GAUGE:
        status |= opentsdb_gauge(r, fam, m, ttl);
        break;
    case METRIC_TYPE_COUNTER:
        status |= opentsdb_counter(r, fam, m, ttl);
        break;
    case METRIC_TYPE_STATE_SET:
        status |= opentsdb_state_set(r, fam, m, ttl);
        break;
    case METRIC_TYPE_INFO:
        status |= opentsdb_info(r, fam, m, ttl);
        break;
    case METRIC_TYPE_SUMMARY:
        status |= opentsdb_summary(r, fam, m, ttl);
        break;
    case METRIC_TYPE_HISTOGRAM:
        status |= opentsdb_histogram(r, fam, m, ttl);
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        status |= opentsdb_gauge_histogram(r, fam, m, ttl);
        break;
    }

    return status;
}

int opentsdb_json_metric_family(strbuf_t *buf, metric_family_t const *fam, int ttl)
{
    int status = 0;

    xson_render_t r = {0};
    xson_render_init(&r, buf, XSON_RENDER_TYPE_JSON, 0);

    xson_render_array_open(&r);
    for (size_t i=0; i < fam->metric.num ; i++) {
       status |= opentsdb_json_metric(&r, fam, &fam->metric.ptr[i], ttl);
    }
    xson_render_array_close(&r);

    return status;
}
