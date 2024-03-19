// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2016 Aurelien beorn Rougemont
// SPDX-FileCopyrightText: Copyright (C) 2020 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Aurelien beorn Rougemont <beorn at gandi dot net>
// SPDX-FileContributor: Manoj Srivastava <srivasta at google.com>
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "libutils/common.h"
#include "libutils/dtoa.h"
#include "libutils/strbuf.h"
#include "libformat/opentsdb_telnet.h"
/*
putm <metric name> <time stamp> <value> <tag> <tag>... \n

Metric name must be one word and is limited to utf8 characters.
Time stamp milliseconds since Jan 1, 1970 (unix epoch)
Value can either be a long or double value.
Tag is in the form of key=value.
*/

//putm <metric name> <time stamp> <value> <tag> <tag>... \n
//Metric names, tag names and values are case sensitive and can contain any character except spaces and in the case of tags anything except ‘=’.

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

static int opentsdb_render_metric(strbuf_t *buf,
                                  char *metric, char *metric_suffix,
                                  const label_set_t *labels1, const label_set_t *labels2,
                                  fmt_opentsdb_t resolution, cdtime_t time, int ttl,
                                  data_type_t type, data_t value)
{
    int status = 0;

    if (resolution == FMT_OPENTSDB_SEC)
        status = strbuf_putstrn(buf, "put ", strlen("put "));
    else
        status = strbuf_putstrn(buf, "putm ", strlen("putm "));

    status |= strbuf_putstr(buf, metric);
    if (metric_suffix != NULL)
        status |= strbuf_putstr(buf, metric_suffix);

    status |= strbuf_putchar(buf, ' ');

    if (resolution == FMT_OPENTSDB_SEC)
        status |= strbuf_putuint(buf, CDTIME_T_TO_TIME_T(time));
    else
        status |= strbuf_putuint(buf, CDTIME_T_TO_MS(time));

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


    size_t size1 = labels1 == NULL ? 0 : labels1->num;
    size_t n1 = 0;
    size_t size2 = labels2 == NULL ? 0 : labels2->num;
    size_t n2 = 0;

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
            status |= strbuf_putchar(buf, ' ');
            status |= strbuf_putstr(buf, pair->name);
            status |= strbuf_putchar(buf, '=');
            status |= strbuf_print_escaped(buf, pair->value, "\\\"\n\r\t", '\\');
        }
    }

    if (ttl > 0) {
        status |= strbuf_putstrn(buf, " kairos_opt.ttl=", strlen(" kairos_opt.ttl="));
        status |= strbuf_putint(buf, ttl);
    }

    status |= strbuf_putchar(buf, '\n');
    return status;
}

int opentsdb_telnet_metric(strbuf_t *buf, metric_family_t const *fam, metric_t const *m, int ttl, fmt_opentsdb_t resolution)
{
    int status = 0;

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
        status |= opentsdb_render_metric(buf, fam->name, NULL, &m->label, NULL,
                                              resolution, m->time, ttl, type, value);
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
        status |= opentsdb_render_metric(buf, fam->name, NULL, &m->label, NULL,
                                              resolution, m->time, ttl, type, value);
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

        status |= opentsdb_render_metric(buf, fam->name, "_total", &m->label, NULL,
                                              resolution, m->time, ttl, type, value);
    }   break;
    case METRIC_TYPE_STATE_SET:
        for (size_t j = 0; j < m->value.state_set.num; j++) {
            label_pair_t label_pair = {.name = fam->name,
                                       .value = m->value.state_set.ptr[j].name};
            label_set_t label_set = {.num = 1, .ptr = &label_pair};
            data_t value = { .uint64 = m->value.state_set.ptr[j].enabled ? 1 : 0 };

            status |= opentsdb_render_metric(buf, fam->name, NULL, &m->label, &label_set,
                                                  resolution, m->time, ttl, DATA_UINT64, value);
        }
        break;
    case METRIC_TYPE_INFO:
        status |= opentsdb_render_metric(buf, fam->name, "_info", &m->label, &m->value.info,
                                              resolution, m->time, ttl, DATA_UINT64, (data_t){.uint64 = 1});
        break;
    case METRIC_TYPE_SUMMARY: {
        summary_t *s = m->value.summary;

        for (int j = s->num - 1; j >= 0; j--) {
            char quantile[DTOA_MAX];

            dtoa(s->quantiles[j].quantile, quantile, sizeof(quantile));

            label_pair_t label_pair = {.name = "quantile", .value = quantile};
            label_set_t label_set = {.num = 1, .ptr = &label_pair};
            status |= opentsdb_render_metric(buf, fam->name, NULL, &m->label, &label_set,
                                             resolution, m->time, ttl,
                                             DATA_FLOAT64, (data_t){.float64 = s->quantiles[j].value});
        }

        status |= opentsdb_render_metric(buf, fam->name, "_count", &m->label, NULL,
                                              resolution, m->time, ttl,
                                              DATA_UINT64, (data_t){.uint64 = s->count});
        status |= opentsdb_render_metric(buf, fam->name, "_sum", &m->label, NULL,
                                              resolution, m->time, ttl,
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
            status |= opentsdb_render_metric(buf, fam->name, "_bucket", &m->label, &label_set,
                                             resolution, m->time, ttl,
                                             DATA_UINT64, (data_t){.uint64 = h->buckets[j].counter});
        }

        status |= opentsdb_render_metric(buf, fam->name,
                                         fam->type == METRIC_TYPE_HISTOGRAM ? "_count" : "_gcount",
                                         &m->label, NULL, resolution, m->time, ttl,
                                         DATA_UINT64, (data_t){.uint64 = histogram_counter(h)});
        status |= opentsdb_render_metric(buf, fam->name,
                                         fam->type == METRIC_TYPE_HISTOGRAM ? "_sum" : "_gsum",
                                         &m->label, NULL, resolution, m->time, ttl,
                                         DATA_FLOAT64, (data_t){.float64 = histogram_sum(h)});
    }   break;
    }

    return status;
}

int opentsdb_telnet_metric_family(strbuf_t *buf, metric_family_t const *fam, int ttl, fmt_opentsdb_t resolution)
{
    int status = 0;

    for (size_t i=0; i < fam->metric.num ; i++) {
       status |= opentsdb_telnet_metric(buf, fam, &fam->metric.ptr[i], ttl, resolution);
    }

    return status;
}
