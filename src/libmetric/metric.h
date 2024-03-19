/* SPDX-License-Identifier: GPL-2.0-only OR MIT                      */
/* SPDX-FileCopyrightText: Copyright (C) 2019-2020  Google LLC       */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Florian octo Forster <octo at collectd.org> */
/* SPDX-FileContributor: Manoj Srivastava <srivasta at google.com>   */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "libutils/time.h"
#include "libmetric/label_set.h"
#include "libmetric/state_set.h"
#include "libmetric/histogram.h"
#include "libmetric/summary.h"

typedef enum {
    METRIC_TYPE_UNKNOWN         = 0,
    METRIC_TYPE_GAUGE           = 1,
    METRIC_TYPE_COUNTER         = 2,
    METRIC_TYPE_STATE_SET       = 3,
    METRIC_TYPE_INFO            = 4,
    METRIC_TYPE_SUMMARY         = 5,
    METRIC_TYPE_HISTOGRAM       = 6,
    METRIC_TYPE_GAUGE_HISTOGRAM = 7,
} metric_type_t;

typedef enum {
    UNKNOWN_FLOAT64 = 0,
    UNKNOWN_INT64   = 1,
} unknown_type_t;

typedef struct {
    unknown_type_t type;
    union {
        double float64;
        int64_t int64;
    };
} unknown_t;

typedef enum {
    GAUGE_FLOAT64 = 0,
    GAUGE_INT64   = 1,
} gauge_type_t;

typedef struct {
    gauge_type_t type;
    union {
        double float64;
        int64_t int64;
    };
} gauge_t;

typedef enum {
    COUNTER_UINT64  = 0,
    COUNTER_FLOAT64 = 1,
} counter_type_t;

typedef struct {
    counter_type_t type;
    union {
        double float64;
        uint64_t uint64;
    };
} counter_t;

typedef union {
    unknown_t unknown;
    gauge_t gauge;
    counter_t counter;
    state_set_t state_set;
    label_set_t info;
    histogram_t *histogram;
    summary_t *summary;
} value_t;

#define VALUE_UNKNOWN(d) (value_t){.unknown.type = UNKNOWN_FLOAT64, .unknown.float64 = (d)}
#define VALUE_UNKNOWN_FLOAT64(d) (value_t){.unknown.type = UNKNOWN_FLOAT64, .unknown.float64 = (d)}
#define VALUE_UNKNOWN_INT64(d) (value_t){.unknown.type = UNKNOWN_INT64, .unknown.int64 = (d)}
#define VALUE_GAUGE(d) (value_t){.gauge.type = GAUGE_FLOAT64, .gauge.float64 = (d)}
#define VALUE_GAUGE_FLOAT64(d) (value_t){.gauge.type = GAUGE_FLOAT64, .gauge.float64 = (d)}
#define VALUE_GAUGE_INT64(d) (value_t){.gauge.type = GAUGE_INT64, .gauge.int64 = (d)}
#define VALUE_COUNTER(d) (value_t){.counter.type = COUNTER_UINT64, .counter.uint64 = (d)}
#define VALUE_COUNTER_UINT64(d) (value_t){.counter.type = COUNTER_UINT64, .counter.uint64 = (d)}
#define VALUE_COUNTER_FLOAT64(d) (value_t){.counter.type = COUNTER_FLOAT64, .counter.float64 = (d)}
#define VALUE_STATE_SET(d) (value_t){.state_set = (d)}
#define VALUE_INFO(d) (value_t){.info = (d)}
#define VALUE_HISTOGRAM(d) (value_t){.histogram = (d)}
#define VALUE_SUMMARY(d) (value_t){.summary = (d)}

const char *metric_type_str (metric_type_t type);

/* metric_t is a metric inside a metric family. */
typedef struct {
    label_set_t label;
    value_t value;
    cdtime_t time;
    cdtime_t interval;
} metric_t;

/* metric_label_set adds or updates a label to/in the label set.
 * If "value" is NULL or the empty string, the label is removed. Removing a
 * label that does not exist is *not* an error. */
int metric_label_set(metric_t *m, char const *name, char const *value);

/* metric_label_get efficiently looks up and returns the value of the "name"
 * label. If a label does not exist, NULL is returned and errno is set to
 * ENOENT. The returned pointer may not be valid after a subsequent call to
 * "metric_label_set". */
char const *metric_label_get(metric_t const *m, char const *name);

/* metric_reset frees all labels and meta data stored in the metric and resets
 * the metric to zero. If the metric is a distribution metric, the function
 * frees the according distribution.*/
int metric_reset(metric_t *m, metric_type_t type);

/* metric_list_t is an unordered list of metrics. */
typedef struct {
    metric_t *ptr;
    size_t num;
} metric_list_t;

/* metric_family_t is a group of metrics of the same type. */
typedef struct {
    char *name;
    char *help;
    char *unit;
    metric_type_t type;
    metric_list_t metric;
} metric_family_t;

/* metric_family_metric_append appends a new metric to the metric family. This
 * allocates memory which must be freed using metric_family_metric_reset. */
int metric_family_metric_append(metric_family_t *fam, metric_t m);

__attribute__ ((sentinel(0)))
int metric_family_append(metric_family_t *fam, value_t v, label_set_t *labels, ...);

/* metric_family_metric_reset frees all metrics in the metric family and
 * resets the count to zero. */
int metric_family_metric_reset(metric_family_t *fam);

/* metric_family_free frees a "metric_family_t" that was allocated with
 * metric_family_clone(). */
void metric_family_free(metric_family_t *fam);

/* metric_family_clone returns a copy of the provided metric family. On error,
 * errno is set and NULL is returned. The returned pointer must be freed with
 * metric_family_free(). */
metric_family_t *metric_family_clone(metric_family_t const *fam);

/*The static function metric_list_clone creates a clone of the argument
 *metric_list_t src. For each metric_t element in the src list it checks if its
 *value is a distribution metric and if yes, calls the distribution_clone
 *function for the value and saves the pointer to the returned distribution_t
 *clone into the metric_list_t dest. If the value is not a distribution_t, it
 *simply sets the value of the element in the destination list to the value of
 *the element in the source list. */

int metric_list_add(metric_list_t *metrics, metric_t m, metric_type_t type);

int metric_list_append(metric_list_t *metrics, metric_t m);

void metric_list_reset(metric_list_t *metrics, metric_type_t type);

int metric_list_clone(metric_list_t *dest, metric_list_t src, metric_family_t *fam);

int metric_value_clone(value_t *dst, value_t src, metric_type_t type);

typedef int (*dispatch_metric_family_t)(metric_family_t *fam, cdtime_t time);

int metric_parse_line(metric_family_t *fam,  dispatch_metric_family_t dispatch,
                      const char *prefix, size_t prefix_size,
                      label_set_t *labels_extra, cdtime_t interval, cdtime_t ts,
                      const char *line);

int metric_parser_dispatch(metric_family_t *fam, dispatch_metric_family_t dispatch);

typedef struct {
    bool fixed;
    size_t pos;
    size_t size;
    metric_family_t **ptr;
} metric_family_list_t;

#define METRIC_FAMILY_LIST_CREATE() (metric_family_list_t){.ptr = NULL}
#define METRIC_FAMILY_LIST_CREATE_STATIC(b) \
    (metric_family_list_t){.ptr = b, .size = sizeof(b), .fixed = true}

int metric_family_list_alloc(metric_family_list_t *faml, size_t num);

void metric_family_list_reset(metric_family_list_t *faml);

static inline int metric_family_list_append(metric_family_list_t *faml, metric_family_t *fam)
{
    if (faml->size == 0)
        return -1;
    if (faml->pos >= faml->size)
        return -1;

    faml->ptr[faml->pos] = fam;
    faml->pos++;
    return 0;
}
