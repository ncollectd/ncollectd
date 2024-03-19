// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "libmetric/metric.h"
#include "libutils/dtoa.h"
#include "libutils/strbuf.h"

typedef union {
    double   float64;
    uint64_t uint64;
    int64_t  int64;
} data_t;

typedef enum {
    DATA_FLOAT64,
    DATA_UINT64,
    DATA_INT64
} data_type_t;

static int openmetrics_metric(strbuf_t *buf, char *metric, char *metric_suffix,
                              const label_set_t *labels1, const label_set_t *labels2,
                              cdtime_t time, data_type_t type, data_t value)
{
    int status = strbuf_putstr(buf, metric);
    if (metric_suffix != NULL)
        status |= strbuf_putstr(buf, metric_suffix);

    size_t size1 = labels1 == NULL ? 0 : labels1->num;
    size_t n1 = 0;
    size_t size2 = labels2 == NULL ? 0 : labels2->num;
    size_t n2 = 0;

    size_t n = 0;
    while ((n1 < size1) || (n2 < size2)) {
        label_pair_t *pair = NULL;
        if ((n1 < size1) && (n2 < size2)) {
            if (strcmp(labels1->ptr[n1].name, labels2->ptr[n2].name) <= 0) {
                pair = &labels1->ptr[n1++];
            } else {
                pair = &labels2->ptr[n2++];
            }
        } else if (n1 < size1) {
            pair = &labels1->ptr[n1++];
        } else if (n2 < size2) {
            pair = &labels2->ptr[n2++];
        }

        if (pair != NULL) {
            if (n == 0)
                status |= strbuf_putchar(buf, '{');
            if (n > 0)
                status |= strbuf_putchar(buf, ',');
            status |= strbuf_putstr(buf, pair->name);
            status |= strbuf_putstrn(buf, "=\"", strlen("=\""));
            status |= strbuf_putescape_label(buf, pair->value);
            status |= strbuf_putchar(buf, '"');
            n++;
        }
    }

    if (n > 0)
        status |= strbuf_putchar(buf, '}');

    status |= strbuf_putchar(buf, ' ');

    switch(type) {
    case DATA_FLOAT64:
        status |= strbuf_putdouble(buf, value.float64);
        break;
    case DATA_UINT64:
        status |= strbuf_putuint(buf, value.uint64);
        break;
    case DATA_INT64:
        status |= strbuf_putint(buf, value.int64);
        break;
    }

    status |= strbuf_putchar(buf, ' ');
    status |= strbuf_putuint(buf, CDTIME_T_TO_MS(time));
    status |= strbuf_putchar(buf, '\n');

    return status;
}

int openmetrics_text_metric_family(strbuf_t *buf, metric_family_t const *fam)
{
    int status = 0;

    if ((buf == NULL) || (fam == NULL))
        return EINVAL;

    if (fam->metric.num == 0)
        return 0;

    status |= strbuf_putstrn(buf, "# TYPE ", strlen("# TYPE "));
    status |= strbuf_putstr(buf, fam->name);
    status |= strbuf_putchar(buf, ' ');
    switch(fam->type) {
    case METRIC_TYPE_UNKNOWN:
        status |= strbuf_putstrn(buf, "unknown", strlen("unknown"));
        break;
    case METRIC_TYPE_GAUGE:
        status |= strbuf_putstrn(buf, "gauge", strlen("gauge"));
        break;
    case METRIC_TYPE_COUNTER:
        status |= strbuf_putstrn(buf, "counter", strlen("counter"));
        break;
    case METRIC_TYPE_STATE_SET:
        status |= strbuf_putstrn(buf, "stateset", strlen("stateset"));
        break;
    case METRIC_TYPE_INFO:
        status |= strbuf_putstrn(buf, "info", strlen("info"));
        break;
    case METRIC_TYPE_SUMMARY:
        status |= strbuf_putstrn(buf, "summary", strlen("summary"));
        break;
    case METRIC_TYPE_HISTOGRAM:
        status |= strbuf_putstrn(buf, "histogram", strlen("histogram"));
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        status |= strbuf_putstrn(buf, "gaugehistogram", strlen("gaugehistogram"));
        break;
    }
    status |= strbuf_putchar(buf, '\n');

    if (fam->help != NULL) {
        status |= strbuf_putstrn(buf, "# HELP ", strlen("# HELP "));
        status |= strbuf_putstr(buf, fam->name);
        status |= strbuf_putchar(buf, ' ');
        status |= strbuf_putstr(buf, fam->help);
        status |= strbuf_putchar(buf, '\n');
    }

    if (fam->unit != NULL) {
        status |= strbuf_putstrn(buf, "# UNIT ", strlen("# UNIT "));
        status |= strbuf_putstr(buf, fam->name);
        status |= strbuf_putchar(buf, ' ');
        status |= strbuf_putstr(buf, fam->unit);
        status |= strbuf_putchar(buf, '\n');
    }

    if (status != 0)
        return status;

    for (size_t i = 0; i < fam->metric.num; i++) {
        metric_t *m = &fam->metric.ptr[i];
        switch(fam->type) {
        case METRIC_TYPE_UNKNOWN: {
            data_t value = {0};
            data_type_t type = 0;
            if (m->value.unknown.type == UNKNOWN_FLOAT64) {
                value.float64 = m->value.unknown.float64;
                type = DATA_FLOAT64;
            } else {
                value.int64 = m->value.unknown.int64;
                type = DATA_INT64;
            }
            status |= openmetrics_metric(buf, fam->name, NULL, &m->label, NULL, m->time,
                                              type, value);
        }   break;
        case METRIC_TYPE_GAUGE: {
            data_t value = {0};
            data_type_t type = 0;
            if (m->value.gauge.type == GAUGE_FLOAT64) {
                value.float64 = m->value.gauge.float64;
                type = DATA_FLOAT64;
            } else {
                value.int64 = m->value.gauge.int64;
                type = DATA_INT64;
            }
            status |= openmetrics_metric(buf, fam->name, NULL, &m->label, NULL, m->time,
                                              type, value);
        }   break;
        case METRIC_TYPE_COUNTER: {
            data_t value = {0};
            data_type_t type = 0;
            if (m->value.counter.type == COUNTER_UINT64) {
                value.uint64 = m->value.counter.uint64;
                type = DATA_UINT64;
            } else {
                value.float64 = m->value.counter.float64;
                type = DATA_FLOAT64;
            }

            status |= openmetrics_metric(buf, fam->name, "_total", &m->label, NULL, m->time,
                                              type, value);
        }   break;
        case METRIC_TYPE_STATE_SET:
            for (size_t j = 0; j < m->value.state_set.num; j++) {
                label_pair_t label_pair = {.name = fam->name,
                                           .value = m->value.state_set.ptr[j].name};
                label_set_t label_set = {.num = 1, .ptr = &label_pair};
                data_t value = { .uint64 = m->value.state_set.ptr[j].enabled ? 1 : 0 };

                status |= openmetrics_metric(buf, fam->name, NULL, &m->label, &label_set, m->time,
                                                  DATA_UINT64, value);
            }
            break;
        case METRIC_TYPE_INFO:
            status |= openmetrics_metric(buf, fam->name, "_info", &m->label, &m->value.info, m->time,
                                              DATA_UINT64, (data_t){.uint64 = 1});
            break;
        case METRIC_TYPE_SUMMARY: {
            summary_t *s = m->value.summary;

            for (int j = s->num - 1; j >= 0; j--) {
                char quantile[DTOA_MAX];

                dtoa(s->quantiles[j].quantile, quantile, sizeof(quantile));

                label_pair_t label_pair = {.name = "quantile", .value = quantile};
                label_set_t label_set = {.num = 1, .ptr = &label_pair};
                status |= openmetrics_metric(buf, fam->name, NULL, &m->label, &label_set, m->time,
                                             DATA_FLOAT64, (data_t){.float64 = s->quantiles[j].value});
            }

            status |= openmetrics_metric(buf, fam->name, "_count", &m->label, NULL, m->time,
                                              DATA_UINT64, (data_t){.uint64 = s->count});
            status |= openmetrics_metric(buf, fam->name, "_sum", &m->label, NULL, m->time,
                                              DATA_UINT64, (data_t){.uint64 = s->sum});
        }   break;
        case METRIC_TYPE_HISTOGRAM:
        case METRIC_TYPE_GAUGE_HISTOGRAM: {
            histogram_t *h = m->value.histogram;

            for (int j = h->num - 1; j >= 0; j--) {
                char le[DTOA_MAX];
                dtoa(h->buckets[j].maximum, le, sizeof(le));

                label_pair_t label_pair = {.name = "le", .value = le};
                label_set_t label_set = {.num = 1, .ptr = &label_pair};
                status |= openmetrics_metric(buf, fam->name, "_bucket",
                                             &m->label, &label_set, m->time,
                                             DATA_UINT64, (data_t){.uint64 = h->buckets[j].counter});
            }

            status |= openmetrics_metric(buf, fam->name,
                                         fam->type == METRIC_TYPE_HISTOGRAM ? "_count" : "_gcount",
                                         &m->label, NULL, m->time,
                                         DATA_UINT64, (data_t){.uint64 = histogram_counter(h)});
            status |= openmetrics_metric(buf, fam->name,
                                         fam->type == METRIC_TYPE_HISTOGRAM ?  "_sum" : "_gsum",
                                         &m->label, NULL, m->time,
                                         DATA_FLOAT64, (data_t){.float64 = histogram_sum(h)});
        }   break;
        }

        if (status != 0)
            return status;
    }

    return status;
}
