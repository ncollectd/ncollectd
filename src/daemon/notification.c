/**
 * collectd - src/notification.c
 * Copyright (C) 2005-2014  Florian octo Forster
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Florian octo Forster <octo at collectd.org>
 *   Sebastian Harl <sh at tokkee.org>
 *   Manoj Srivastava <srivasta at google.com>
 **/

#include "collectd.h"
#include "utils/common/common.h"

#include "metric.h"
#include "notification.h"
#include "plugin.h"

int notification_identity(strbuf_t *buf, notification_t const *n) {
  if ((buf == NULL) || (n == NULL)) {
    return EINVAL;
  }

  int status = strbuf_print(buf, n->name);
  if (n->label.num == 0) {
    return status;
  }

  status = status || label_set_marshal(buf, n->label);
  return status;
}

int notification_marshal(strbuf_t *buf, notification_t const *n) {
  if ((buf == NULL) || (n == NULL)) {
    return EINVAL;
  }

  int status = strbuf_print(buf, n->name);

  status = status || label_set_marshal(buf, n->label);
  status = status || label_set_marshal(buf, n->annotation);

  const char *severity = " FAILURE ";
  if (n->severity == NOTIF_WARNING)
    severity = " WARNING ";
  else if (n->severity == NOTIF_OKAY)
    severity = " OKAY ";

  status = status || strbuf_print(buf, severity);
  status = status || strbuf_printf(buf, "%.3f\n", CDTIME_T_TO_DOUBLE(n->time));

  return status;
}

int notification_init_metric(notification_t *n, int severity, metric_t const *m) {
  if ((n == NULL) || (m == NULL)) {
    return EINVAL;
  }

  n->name = m->family->name;
  n->severity = severity;
  n->time = m->time;

  int status = label_set_clone(&n->label, m->label);
  if (status != 0) {
    return status;
  }

  return 0;
}

char const *notification_label_get(notification_t const *n, char const *name)
{
  if ((n == NULL) || (name == NULL)) {
    errno = EINVAL;
    return NULL;
  }

  label_pair_t *set = label_set_read(n->label, name);
  if (set == NULL) {
    return NULL;
  }

  return set->value;
}

int notification_label_set(notification_t *n, char const *name, char const *value)
{
  if ((n == NULL) || (name == NULL)) {
    return EINVAL;
  }

  return label_set_add(&n->label, name, value);
}

char const *notification_annotation_get(notification_t const *n, char const *name)
{
  if ((n == NULL) || (name == NULL)) {
    errno = EINVAL;
    return NULL;
  }

  label_pair_t *set = label_set_read(n->annotation, name);
  if (set == NULL) {
    return NULL;
  }

  return set->value;
}

int notification_annotation_set(notification_t *n, char const *name, char const *value)
{
  if ((n == NULL) || (name == NULL)) {
    return EINVAL;
  }

  return label_set_add(&n->annotation, name, value);
}

int notification_reset(notification_t *n)
{
  if (n == NULL) {
    return EINVAL;
  }

  label_set_reset(&n->label);
  label_set_reset(&n->annotation);
  meta_data_destroy(n->meta);

  memset(n, 0, sizeof(*n));

  return 0;
}

void notification_free(notification_t *n)
{
  if (n == NULL) {
    return;
  }

  label_set_reset(&n->label);
  label_set_reset(&n->annotation);
  meta_data_destroy(n->meta);
  sfree(n->name);
  sfree(n);
}

notification_t *notification_clone(notification_t const *src) {
  if (src == NULL) {
    //return EINVAL;
    return NULL;
  }

  notification_t *dest = calloc(1, sizeof(*dest));
  if (dest == NULL) {
    //return ENOMEM;
    return NULL;
  }
 
  int status = label_set_clone(&dest->label, src->label);
  if (status != 0) {
    return NULL;
  }

  status = label_set_clone(&dest->annotation, src->annotation);
  if (status != 0) {
    notification_free(dest);
    return NULL;
  }
  if (src->meta != NULL) {
    dest->meta = meta_data_clone(src->meta);
    if (dest->meta != NULL) {
      notification_free(dest);
      return NULL;
    }
  }

  dest->time = src->time;
  dest->severity = src->severity;
  dest->name = sstrdup(src->name);
  if (dest->name == NULL) {
    ERROR("notification_clone: strdup failed.");
    notification_free(dest);
    return NULL;
  }

  return dest;
}

int notitication_unmarshal(notification_t *n, char const *buf)
{
  if ((n == NULL) || (buf == NULL)) {
    return EINVAL;
  }

  char const *ptr = buf;
  size_t name_len = strspn(ptr, VALID_NAME_CHARS);
  if (name_len == 0) {
    return EINVAL;
  }

  char name[name_len + 1];
  strncpy(name, ptr, name_len);
  name[name_len] = 0;
  ptr += name_len;

  n->name = strdup(name);
  if (n->name == NULL) {
    return ENOMEM;
  }

  char const *end = ptr;
  int ret = label_set_unmarshal(&n->label, &end);
  if (ret != 0) {
    return ret;
  }
  ptr = end;

  end = ptr;
  ret = label_set_unmarshal(&n->annotation, &end);
  if (ret != 0) {
    return ret;
  }
  ptr = end;

  if ((ptr[0] != '}') || ((ptr[1] != 0) && (ptr[1] != ' '))) {
    return EINVAL;
  }

  return 0;
}
