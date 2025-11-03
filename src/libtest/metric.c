// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"

#include "libutils/common.h"
#include "libutils/itoa.h"
#include "libutils/dtoa.h"
#include "libmetric/metric.h"
#include "libmetric/label_set.h"
#include "libmetric/parser.h"

static metric_family_t *fam_dispatch;
static size_t fam_dispatch_size;

static metric_family_t *fam_expect;
static size_t fam_expect_size;

extern char *hostname_g;

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

static int dump_metric(FILE *fp, char *metric, char *metric_suffix,
                       const label_set_t *labels1, const label_set_t *labels2,
                       cdtime_t time, data_type_t type, data_t value)
{
    (void)time;

    fprintf(fp, "%s%s", metric, metric_suffix != NULL ? metric_suffix : "");

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
                fprintf(fp, "{");
            if (n > 0)
                fprintf(fp, ",");
            fprintf(fp, "%s=\"", pair->name);
            char *c = pair->value;
            while (*c != '\0') {
                switch (*c) {
                case '"':
                    fputs("\\\"", fp);
                    break;
                case '\\':
                    fputs("\\\\", fp);
                    break;
                case '\n':
                    fputs("\\n", fp);
                    break;
                case '\r':
                    fputs("\\r", fp);
                    break;
                case '\t':
                    fputs("\\t", fp);
                    break;
                default:
                    fputc(*c, fp);
                }
                c++;
            }
            fprintf(fp, "\"");
            n++;
        }
    }

    if (n > 0)
        fprintf(fp, "}");

    fprintf(fp, " ");

    switch(type) {
    case DATA_FLOAT64: {
        char buf[DTOA_MAX];
        dtoa(value.float64, buf, sizeof(buf));
        fprintf(fp, "%s\n", buf);
    }   break;
    case DATA_UINT64: {
        char buf[ITOA_MAX];
        itoa(value.uint64, buf);
        fprintf(fp, "%s\n", buf);
    }   break;
    case DATA_INT64: {
        char buf[ITOA_MAX];
        itoa(value.int64, buf);
        fprintf(fp, "%s\n", buf);
    }   break;
    }

    return 0;
}

static int dump_metric_family(FILE *fp, metric_family_t const *fam)
{
    int status = 0;

    if (fam == NULL)
        return EINVAL;

    if (fam->metric.num == 0)
        return 0;

    fprintf(fp, "# TYPE %s ",fam->name);
    switch(fam->type) {
    case METRIC_TYPE_UNKNOWN:
        fprintf(fp, "unknown\n");
        break;
    case METRIC_TYPE_GAUGE:
        fprintf(fp, "gauge\n");
        break;
    case METRIC_TYPE_COUNTER:
        fprintf(fp, "counter\n");
        break;
    case METRIC_TYPE_STATE_SET:
        fprintf(fp, "stateset\n");
        break;
    case METRIC_TYPE_INFO:
        fprintf(fp, "info\n");
        break;
    case METRIC_TYPE_SUMMARY:
        fprintf(fp, "summary\n");
        break;
    case METRIC_TYPE_HISTOGRAM:
        fprintf(fp, "histogram\n");
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        fprintf(fp, "gaugehistogram\n");
        break;
    }

#if 0
    if (fam->help != NULL)
        fprintf(fp, "# HELP %s %s\n", fam->name, fam->help);

    if (fam->unit != NULL)
        fprintf(fp, "# UNIT %s %s\n", fam->name, fam->unit);
#endif
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
            dump_metric(fp, fam->name, NULL, &m->label, NULL, m->time, type, value);
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
            dump_metric(fp, fam->name, NULL, &m->label, NULL, m->time, type, value);
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

            dump_metric(fp, fam->name, "_total", &m->label, NULL, m->time, type, value);
        }   break;
        case METRIC_TYPE_STATE_SET:
            for (size_t j = 0; j < m->value.state_set.num; j++) {
                label_pair_t label_pair = {.name = fam->name,
                                           .value = m->value.state_set.ptr[j].name};
                label_set_t label_set = {.num = 1, .ptr = &label_pair};
                data_t value = { .uint64 = m->value.state_set.ptr[j].enabled ? 1 : 0 };

                dump_metric(fp, fam->name, NULL, &m->label, &label_set, m->time,
                            DATA_UINT64, value);
            }
            break;
        case METRIC_TYPE_INFO:
            dump_metric(fp, fam->name, "_info", &m->label, &m->value.info, m->time,
                         DATA_UINT64, (data_t){.uint64 = 1});
            break;
        case METRIC_TYPE_SUMMARY: {
            summary_t *s = m->value.summary;

            for (int j = s->num - 1; j >= 0; j--) {
                char quantile[DTOA_MAX];

                dtoa(s->quantiles[j].quantile, quantile, sizeof(quantile));

                label_pair_t label_pair = {.name = "quantile", .value = quantile};
                label_set_t label_set = {.num = 1, .ptr = &label_pair};
                dump_metric(fp, fam->name, NULL, &m->label, &label_set, m->time,
                            DATA_FLOAT64, (data_t){.float64 = s->quantiles[j].value});
            }

            dump_metric(fp, fam->name, "_count", &m->label, NULL, m->time,
                        DATA_UINT64, (data_t){.uint64 = s->count});
            dump_metric(fp, fam->name, "_sum", &m->label, NULL, m->time,
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
                dump_metric(fp, fam->name, "_bucket", &m->label, &label_set, m->time,
                            DATA_UINT64, (data_t){.uint64 = h->buckets[j].counter});
            }

            dump_metric(fp, fam->name, fam->type == METRIC_TYPE_HISTOGRAM ? "_count" : "_gcount",
                        &m->label, NULL, m->time,
                        DATA_UINT64, (data_t){.uint64 = histogram_counter(h)});
            dump_metric(fp, fam->name, fam->type == METRIC_TYPE_HISTOGRAM ?  "_sum" : "_gsum",
                        &m->label, NULL, m->time,
                        DATA_FLOAT64, (data_t){.float64 = histogram_sum(h)});
        }   break;
        }
    }

    return 0;
}

static int dump_metric_family_list(FILE *fp, metric_family_t *fams, size_t size)
{
    for (size_t i=0; i < size; i++) {
        metric_family_t *fam = &fams[i];
        dump_metric_family(fp, fam);
    }

    return 0;
}

static void test_metric_family_array_free(metric_family_t *fams, size_t size)
{
    for (size_t i=0; i < size; i++) {
        metric_family_t *fam = &fams[i];
        free(fam->name);
        free(fam->help);
        free(fam->unit);
        metric_list_reset(&fam->metric, fam->type);

    }
    free(fams);
}

static int test_metric_expect_add(metric_family_t *fam,
                                  __attribute__((unused)) plugin_filter_t *filter,
                                  __attribute__((unused)) cdtime_t time)
{
    if (fam == NULL)
        return 0;

    metric_family_t *ffam = NULL;

    for (size_t i=0; i < fam_expect_size; i++) {
        if (strcmp(fam->name, fam_expect[i].name) == 0) {
            ffam = &fam_expect[i];
        }
    }

    if (ffam == NULL) {
        metric_family_t *tmp = realloc(fam_expect, sizeof(*fam) * (fam_expect_size + 1));
        if (tmp == NULL) {
            fprintf(stderr, "realloc failed.");
            return -1;
        }

        fam_expect = tmp;
        ffam = &fam_expect[fam_expect_size];
        fam_expect_size++;

        memcpy(ffam, fam, sizeof(*fam));

        fam->name = NULL;
        fam->help = NULL;
        fam->unit = NULL;
        fam->metric.num = 0;
        fam->metric.ptr = NULL;

        return 0;
    }

    for(size_t i = 0; i < fam->metric.num; i++) {
        metric_list_append(&ffam->metric, fam->metric.ptr[i]);
    }

    free(fam->name);
    fam->name = NULL;
    free(fam->help);
    fam->help = NULL;
    free(fam->unit);
    fam->unit = NULL;
    free(fam->metric.ptr);
    fam->metric.num = 0;
    fam->metric.ptr = NULL;

    return 0;
}

static int test_metric_cmp(metric_t *a, metric_t *b, metric_type_t type)
{
    if ((a == NULL) || (b == NULL))
        return -1;

    if (type != METRIC_TYPE_INFO) {
        if (label_set_cmp(&a->label, &b->label) != 0)
            return -1;
    }

    switch (type) {
    case METRIC_TYPE_UNKNOWN:
        if (a->value.unknown.type != b->value.unknown.type) {
            if (a->value.unknown.type == UNKNOWN_FLOAT64) {
                double val = b->value.unknown.int64;
                if(a->value.unknown.float64 != val) {
                    return -1;
                }
            } else if (a->value.unknown.type == UNKNOWN_INT64) {
                int64_t val = b->value.unknown.int64;
                if(a->value.unknown.int64 != val) {
                    return -1;
                }
            }
            return -1;
        } else {
            if (a->value.unknown.type == UNKNOWN_FLOAT64) {
                if (a->value.unknown.float64 != b->value.unknown.float64)
                    return -1;
            } else {
                if (a->value.unknown.int64 != b->value.unknown.int64)
                    return -1;
            }
        }
        break;
    case METRIC_TYPE_GAUGE:
        if (a->value.gauge.type != b->value.gauge.type) {
            if (a->value.gauge.type == GAUGE_FLOAT64) {
                double val = b->value.gauge.int64;
                if(a->value.gauge.float64 != val) {
                    return -1;
                }
            } else if (a->value.gauge.type == GAUGE_INT64) {
                int64_t val = b->value.gauge.int64;
                if(a->value.gauge.int64 != val) {
                    return -1;
                }
            }
            return -1;
        } else {
            if (a->value.gauge.type == GAUGE_FLOAT64) {
                if (a->value.gauge.float64 != b->value.gauge.float64) {
                    return -1;
                }
            } else {
                if (a->value.gauge.int64 != b->value.gauge.int64) {
                    return -1;
                }
            }
        }
        break;
    case METRIC_TYPE_COUNTER:
        if (a->value.counter.type != b->value.counter.type) {
            if (a->value.counter.type == COUNTER_FLOAT64) {
                double val = b->value.counter.uint64;
                if(a->value.counter.float64 != val) {
                    return -1;
                }
            } else if (a->value.counter.type == COUNTER_UINT64) {
                uint64_t val = b->value.counter.uint64;
                if(a->value.counter.uint64 != val) {
                    return -1;
                }
            }
        } else {
            if (a->value.counter.type == COUNTER_UINT64) {
                if (a->value.counter.uint64 != b->value.counter.uint64)  {
                    return -1;
                }
            } else {
                if (a->value.counter.float64 != b->value.counter.float64)  {
                    return -1;
                }
            }
        }
        break;
    case METRIC_TYPE_STATE_SET:
        // TODO
        break;
    case METRIC_TYPE_INFO:
        label_set_add_set(&a->label, true, a->value.info);
        label_set_add_set(&b->label, true, b->value.info);
        if (label_set_cmp(&a->label, &b->label) != 0)
            return -1;
        break;
    case METRIC_TYPE_SUMMARY:
        // TODO
        break;
    case METRIC_TYPE_HISTOGRAM:
        // TODO
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        // TODO
        break;
    }

    return 0;
}

static int test_metric_list_cmp(metric_list_t *a, metric_list_t *b, metric_type_t type)
{
    if ((a == NULL) || (b == NULL))
        return -1;

    if (a->num != b->num)
        return -1;

    for (size_t i = 0; i < a->num; i++) {
        bool found = false;
        for (size_t j = 0; j < b->num; j++) {
            if (test_metric_cmp(&a->ptr[i], &b->ptr[j], type) == 0) {
                found = true;
                break;
            }
        }
        if (!found)
            return -1;
    }

    return 0;
}

static int test_metric_family_cmp(metric_family_t *a, metric_family_t *b)
{
    if ((a == NULL) || (b == NULL))
        return -1;

    if (strcmp(a->name, b->name) != 0)
        return -1;

    if (a->type != b->type)
        return -1;

#if 0
    if (a->help != NULL) {
       if (b->help == NULL)
            return -1;
       if (strcmp(a->help, b->help) != 0)
            return -1;
    } else if (b->help != NULL) {
        return -1;
    }
#endif
    if (a->unit != NULL) {
       if (b->unit == NULL)
            return -1;
       if (strcmp(a->unit, b->unit) != 0)
            return -1;
    } else if (b->unit!= NULL) {
        return -1;
    }
    return test_metric_list_cmp(&a->metric, &b->metric, a->type);
}

int test_metric_family_list_cmp(metric_family_t *a, size_t size_a,
                                metric_family_t *b, size_t size_b)
{
    size_t fa_size = 0;
    for (size_t i = 0; i < size_a; i++) {
        if(a[i].metric.num > 0)
            fa_size++;
    }

    size_t fb_size = 0;
    for (size_t i = 0; i < size_b; i++) {
        if(b[i].metric.num > 0)
            fb_size++;
    }

    if (fa_size != fb_size) {
        fprintf(stderr, "Different number of metrics. Expect %zu got %zu.", fa_size, fb_size);
        return -1;
    }

    for (size_t i = 0; i < size_a; i++) {
        if (a[i].metric.num == 0)
            continue;

        bool found = false;
        for (size_t j = 0; j < size_b; j++) {
            if (test_metric_family_cmp(&a[i], &b[j]) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            fprintf(stderr, "Metric %s not found.", a[i].name);
            return -1;
        }
    }

    return 0;
}

void plugin_test_metrics_reset(void)
{
    test_metric_family_array_free(fam_dispatch, fam_dispatch_size);
    fam_dispatch = NULL;
    fam_dispatch_size = 0;

    test_metric_family_array_free(fam_expect, fam_expect_size);
    fam_expect = NULL;
    fam_expect_size = 0;
}

int plugin_test_metrics_cmp(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Cannot open file: '%s'.\n", filename);
        return -1;
    }

    metric_parser_t *mp = metric_parser_alloc(NULL, NULL);
    if (mp == NULL) {
        fprintf(stderr, "Cannot alloc metric parser.\n");
        return -1;
    }

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        strnrtrim(buffer, strlen(buffer));
        int status = metric_parse_line(mp, buffer);
        if (status != 0) {
            fprintf(stderr, "Cannot parse '%s'.", buffer);
            fclose(fp);
            metric_parser_free(mp);
            return -1;
        }
    }

    metric_parser_dispatch(mp, test_metric_expect_add, NULL, 0);

    fclose(fp);
    metric_parser_free(mp);

    char *env_dump = getenv("TEST_DUMP_METRICS");
    if (env_dump != NULL)
        dump_metric_family_list(stdout, fam_dispatch, fam_dispatch_size);

    return test_metric_family_list_cmp(fam_dispatch, fam_dispatch_size,
                                       fam_expect, fam_expect_size);
}

static int plugin_test_add_metric_family(metric_family_t *fam)
{
     metric_family_t *ffam = NULL;

    if(fam->metric.num == 0)
        return 0;

    if (fam->name == NULL) {
        metric_family_metric_reset(fam);
        return 0;
    }

    for (size_t i=0; i < fam_dispatch_size; i++) {
        if (strcmp(fam->name, fam_dispatch[i].name) == 0) {
            ffam = &fam_dispatch[i];
        }
    }

    if (ffam == NULL) {
        metric_family_t *tmp = realloc(fam_dispatch, sizeof(*fam) * (fam_dispatch_size + 1));
        if (tmp == NULL) {
            fprintf(stderr, "realloc failed.");
            return -1;
        }

        fam_dispatch = tmp;
        ffam = &fam_dispatch[fam_dispatch_size];

        if (fam->name == NULL)
            return -1;

        fam_dispatch_size++;

        ffam->name = strdup(fam->name);

        if (fam->help != NULL)
            ffam->help = strdup(fam->help);
        else
            ffam->help = NULL;

        if (fam->unit != NULL)
            ffam->unit = strdup(fam->unit);
        else
            ffam->unit = NULL;

        ffam->type = fam->type;
        ffam->metric.num = fam->metric.num;
        ffam->metric.ptr = fam->metric.ptr;

        for (size_t i = 0; i < ffam->metric.num; i++) {
            label_set_add(&ffam->metric.ptr[i].label, false, "hostname", hostname_g);
        }

        fam->metric.num = 0;
        fam->metric.ptr = NULL;

        return 0;
    }

    for(size_t i = 0; i < fam->metric.num; i++) {
        label_set_add(&fam->metric.ptr[i].label, false, "hostname", hostname_g);
        metric_list_append(&ffam->metric, fam->metric.ptr[i]);
    }

    free(fam->metric.ptr);
    fam->metric.num = 0;
    fam->metric.ptr = NULL;

    return 0;
}

int plugin_dispatch_metric_family_array_filtered(metric_family_t *fams, size_t size,
                                                 __attribute__((unused)) plugin_filter_t *filter,
                                                 __attribute__((unused)) cdtime_t time)
{
    for (size_t i = 0; i < size; i++) {
        int status = plugin_test_add_metric_family(&fams[i]);
        if (status != 0)
            return -1;
    }

    return 0;
}
