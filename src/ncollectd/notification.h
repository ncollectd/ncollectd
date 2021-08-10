/* SPDX-License-Identifier: GPL-2.0-only OR MIT  */
/* Copyright (C) 2005-2014  Florian octo Forster */
/* Authors:                                      */
/*   Florian octo Forster <octo at collectd.org> */
/*   Sebastian Harl <sh at tokkee.org>           */
/*   Manoj Srivastava <srivasta at google.com>   */

#ifndef NOTIFICATION_H
#define NOTIFICATION_H 1

#include "utils_time.h"

#include "label_set.h"
#include "metric.h"

#define NOTIF_FAILURE 1
#define NOTIF_WARNING 2
#define NOTIF_OKAY 4

/* TODO remove when migration to v6 finish */
typedef struct notification_meta_s notification_meta_t;

typedef struct notification_s {
  int severity;
  cdtime_t time;
  char *name;
  label_set_t label;
  label_set_t annotation;
  meta_data_t meta;
} notification_t;

/* notification_init_metric sets the notification severity and the time,
 * name and labes from the values of metric "m".
 */
int notification_init_metric(notification_t *n, int severity, metric_t const *m);

int notification_identity(strbuf_t *buf, notification_t const *n);
/* notification_marshal writes the notifiaciton "n" to the "buf" using
 * the following format:
 *   name{labels}{annotations} serverity timestamp
 *
 * Example:
 *   "http_requests_total{method=\"post\",code=\"500\"}{summary=\"Too manny errors\"}
 */
int notification_marshal(strbuf_t *buf, notification_t const *n);

/* notification_label_get/notification_annotation_get efficiently looks up
 * and returns the value of the "name" label/annotation. If a label/annotation
 * does not exist, NULL is returned and errno is set to * ENOENT.
 * The returned pointer may not be valid after a subsequent call to
 * "notification_label_set"/"notification_annotation_get". */
char const *notification_label_get(notification_t const *n, char const *name);
char const *notification_annotation_get(notification_t const *n, char const *name);

/* notification_label_set/notification_annotation_set  adds or updates a 
 * label/annotation to/in the label set
 * If "value" is NULL or the empty string, the label/annotation is removed.
 * Removing a label/annotation  that does not exist is *not* an error. */
int notification_label_set(notification_t *n, char const *name, char const *value);
int notification_annotation_set(notification_t *n, char const *name, char const *value);

/* notification_free fress a "notification_t" that was allocated with
 * notification_clone(). */
void notification_free(notification_t *n);

/* notification_clone returns a copy of the provided notification. On error,
 * errno is set and NULL is returned. The returned pointer must be freed with
 * notification_free(). */
notification_t *notification_clone(notification_t const *src);

/* notification_reset frees labels, annotations and meta data in
 * the notification */
int notification_reset(notification_t *n);

#endif
