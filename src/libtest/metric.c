// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"

#include "libutils/common.h"
#include "libmetric/metric.h"
#include "libmetric/label_set.h"

static metric_family_t *fam_dispatch;
static size_t fam_dispatch_size;

static metric_family_t *fam_expect;
static size_t fam_expect_size;

extern char *hostname_g;

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

static int test_metric_expect_add(metric_family_t *fam, __attribute__((unused)) cdtime_t time)
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

static int test_metric_family_list_cmp(metric_family_t *a, size_t size_a,
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

    metric_family_t fam = {0};

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        strnrtrim(buffer, strlen(buffer));
        int status = metric_parse_line(&fam, test_metric_expect_add, NULL, 0, NULL, 0, 0, buffer);
        if (status != 0) {
            fprintf(stderr, "Cannot parse '%s'.", buffer);
            fclose(fp);
            return -1;
        }
    }

    metric_parser_dispatch(&fam, test_metric_expect_add);

    fclose(fp);

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

int plugin_dispatch_metric_family_array(metric_family_t *fams, size_t size,
                                        __attribute__((unused)) cdtime_t time)
{
    for (size_t i = 0; i < size; i++) {
        int status = plugin_test_add_metric_family(&fams[i]);
        if (status != 0)
            return -1;
    }

    return 0;
}
