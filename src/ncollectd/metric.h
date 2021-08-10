/* SPDX-License-Identifier: GPL-2.0-only OR MIT  */
/* Copyright (C) 2019-2020  Google LLC           */
/* Authors:                                      */
/*   Florian octo Forster <octo at collectd.org> */
/*   Manoj Srivastava <srivasta at google.com>   */

#ifndef METRIC_H
#define METRIC_H 1

#include "distribution.h"
#include "utils/strbuf/strbuf.h"
#include "utils_time.h"
#include "label_set.h"
#include "state_set.h"
#include "meta_data.h"

typedef enum {
  METRIC_TYPE_UNKNOWN      = 1,
  METRIC_TYPE_GAUGE        = 2,
  METRIC_TYPE_COUNTER      = 3,
  METRIC_TYPE_STATE_SET    = 4,
  METRIC_TYPE_INFO         = 5,
  METRIC_TYPE_DISTRIBUTION = 6,
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
  cdtime_t created;
} counter_t;

typedef union {
  unknown_t unknown;
  gauge_t gauge;
  counter_t counter;
  state_set_t state_set;
  label_set_t info;
  distribution_t *distribution;
} value_t;

typedef struct {
  value_t value;
  metric_type_t type;
} typed_value_t;

/* return a copy of typed_value_t val
 * if type is METRIC_TYPE_DISTRIBUTION then distribution will be cloned
 * that means that it should be freed */
typed_value_t typed_value_clone(typed_value_t val);

/* create a typed_value_t object from val and type
 * if type is METRIC_TYPE_DISTRIBUTION then distribution will be cloned
 * that means that it should be freed */
typed_value_t typed_value_create(value_t val, metric_type_t type);

/* if type is distribution then it will be freed */
void typed_value_destroy(typed_value_t val);

/* value_marshal_text prints a text representation of v to buf. */
int value_marshal_text(strbuf_t *buf, value_t v, metric_type_t type);

/*
 * Metric
 */
/* forward declaration since metric_family_t and metric_t refer to each other.
 */
struct metric_family_s;
typedef struct metric_family_s metric_family_t;

/* metric_t is a metric inside a metric family. */
typedef struct {
  metric_family_t *family; /* backreference for family->name and family->type */

  label_set_t label;
  value_t value;
  cdtime_t time; /* TODO(octo): use ms or µs instead? */
  cdtime_t interval;
  meta_data_t meta;
} metric_t;

/* metric_identity writes the identity of the metric "m" to "buf", using the
 * OpenMetrics / Prometheus plain text exposition format.
 *
 * Example:
 *   "http_requests_total{method=\"post\",code=\"200\"}"
 */
int metric_identity(strbuf_t *buf, metric_t const *m);

/* metric_parse_identity parses "s" and returns a metric with only its identity
 * set. On error, errno is set and NULL is returned. The returned memory must
 * be freed by passing m->family to metric_family_free(). */
metric_t *metric_parse_identity(char const *s);

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
int metric_reset(metric_t *m);

/* metric_list_t is an unordered list of metrics. */
typedef struct {
  metric_t *ptr;
  size_t num;
} metric_list_t;

/*
 * Metric Family
 */
/* metric_family_t is a group of metrics of the same type. */
struct metric_family_s {
  char *name;
  char *help;
  char *unit;
  metric_type_t type;

  metric_list_t metric;
};

/* metric_family_metric_append appends a new metric to the metric family. This
 * allocates memory which must be freed using metric_family_metric_reset. */
int metric_family_metric_append(metric_family_t *fam, metric_t m);

/* metric_family_append constructs a new metric_t and appends it to fam. It is
 * a convenience function that is funcitonally approximately equivalent to the
 * following code, but without modifying templ:
 *
 *   metric_t m = *templ;
 *   m.value = v;
 *   metric_label_set(&m, lname, lvalue);
 *   metric_family_metric_append(fam, m);
 */
int metric_family_append(metric_family_t *fam, char const *lname,
                         char const *lvalue, value_t v, metric_t const *templ);

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

#endif
