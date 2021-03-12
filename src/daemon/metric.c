/**
 * collectd - src/daemon/metric.c
 * Copyright (C) 2019-2020  Google LLC
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
 *   Manoj Srivastava <srivasta at google.com>
 **/

#include "collectd.h"
#include "distribution.h"
#include "metric.h"
#include "plugin.h"

typed_value_t typed_value_clone(typed_value_t val) {
  typed_value_t copy = {
      .value = val.value,
      .type = val.type,
  };
  if (val.type == METRIC_TYPE_DISTRIBUTION) {
    copy.value.distribution = distribution_clone(val.value.distribution);
  }
  return copy;
}

typed_value_t typed_value_create(value_t val, metric_type_t type) {
  typed_value_t tval = {
      .value = val,
      .type = type,
  };
  if (type == METRIC_TYPE_DISTRIBUTION) {
    tval.value.distribution = distribution_clone(val.distribution);
  }
  return tval;
}

void typed_value_destroy(typed_value_t val) {
  if (val.type == METRIC_TYPE_DISTRIBUTION) {
    distribution_destroy(val.value.distribution);
  }
}

int value_marshal_text(strbuf_t *buf, value_t v, metric_type_t type) {
  switch (type) {
  case METRIC_TYPE_GAUGE:
  case METRIC_TYPE_UNTYPED:
    return strbuf_printf(buf, GAUGE_FORMAT, v.gauge);
  case METRIC_TYPE_COUNTER:
    return strbuf_printf(buf, "%" PRIu64, v.counter);
  case METRIC_TYPE_DISTRIBUTION:
    ERROR("Distribution metrics are not to be represented as text.");
    return EINVAL;
  default:
    ERROR("Unknown metric value type: %d", (int)type);
    return EINVAL;
  }
}

int metric_reset(metric_t *m) {
  if (m == NULL) {
    return EINVAL;
  }

  label_set_reset(&m->label);
  meta_data_destroy(m->meta);

  if (m->family != NULL && m->family->type == METRIC_TYPE_DISTRIBUTION) {
    distribution_destroy(m->value.distribution);
  }

  memset(m, 0, sizeof(*m));

  return 0;
}

int metric_identity(strbuf_t *buf, metric_t const *m) {
  if ((buf == NULL) || (m == NULL) || (m->family == NULL)) {
    return EINVAL;
  }

  int status = strbuf_print(buf, m->family->name);
  if (m->label.num == 0) {
    return status;
  }
 
  status = status || label_set_marshal(buf, m->label);

  return status;
}

int metric_label_set(metric_t *m, char const *name, char const *value) {
  if ((m == NULL) || (name == NULL)) {
    return EINVAL;
  }

  return label_set_add(&m->label, name, value);
}

char const *metric_label_get(metric_t const *m, char const *name) {
  if ((m == NULL) || (name == NULL)) {
    errno = EINVAL;
    return NULL;
  }

  label_pair_t *set = label_set_read(m->label, name);
  if (set == NULL) {
    return NULL;
  }

  return set->value;
}

static int metric_list_add(metric_list_t *metrics, metric_t m) {
  if (metrics == NULL) {
    return EINVAL;
  }

  metric_t *tmp =
      realloc(metrics->ptr, sizeof(*metrics->ptr) * (metrics->num + 1));
  if (tmp == NULL) {
    return errno;
  }
  metrics->ptr = tmp;

  metric_t copy = {
      .family = m.family,
      .time = m.time,
      .interval = m.interval,
      .meta = meta_data_clone(m.meta),
  };
  copy.value = (m.family->type == METRIC_TYPE_DISTRIBUTION)
          ? (value_t){.distribution = distribution_clone(m.value.distribution)}
          : m.value;
  int status = label_set_clone(&copy.label, m.label);
  if (((m.meta != NULL) && (copy.meta == NULL)) || (status != 0)) {
    label_set_reset(&copy.label);
    meta_data_destroy(copy.meta);
    return status;
  }

  metrics->ptr[metrics->num] = copy;
  metrics->num++;

  return 0;
}

static void metric_list_reset(metric_list_t *metrics) {
  if (metrics == NULL) {
    return;
  }

  for (size_t i = 0; i < metrics->num; i++) {
    metric_reset(metrics->ptr + i);
  }
  free(metrics->ptr);

  metrics->ptr = NULL;
  metrics->num = 0;
}

static int metric_list_clone(metric_list_t *dest, metric_list_t src,
                             metric_family_t *fam) {
  if (src.num == 0) {
    return 0;
  }

  metric_list_t ret = {
      .ptr = calloc(src.num, sizeof(*ret.ptr)),
      .num = src.num,
  };
  if (ret.ptr == NULL) {
    return ENOMEM;
  }

  for (size_t i = 0; i < src.num; i++) {

    ret.ptr[i] = (metric_t){
        .family = fam,
        .time = src.ptr[i].time,
        .interval = src.ptr[i].interval,
    };

    if (src.ptr[i].family->type == METRIC_TYPE_DISTRIBUTION) {
      ret.ptr[i].value.distribution =
          distribution_clone(src.ptr[i].value.distribution);
    } else {
      ret.ptr[i].value = src.ptr[i].value;
    }

    int status = label_set_clone(&ret.ptr[i].label, src.ptr[i].label);
    if (status != 0) {
      metric_list_reset(&ret);
      return status;
    }
  }

  *dest = ret;
  return 0;
}

int metric_family_metric_append(metric_family_t *fam, metric_t m) {
  if (fam == NULL) {
    return EINVAL;
  }

  m.family = fam;
  return metric_list_add(&fam->metric, m);
}

int metric_family_append(metric_family_t *fam, char const *lname,
                         char const *lvalue, value_t v, metric_t const *templ) {
  if ((fam == NULL) || ((lname == NULL) != (lvalue == NULL))) {
    return EINVAL;
  }

  metric_t m = {
      .family = fam,
      .value = v,
  };
  if (templ != NULL) {
    int status = label_set_clone(&m.label, templ->label);
    if (status != 0) {
      return status;
    }

    m.time = templ->time;
    m.interval = templ->interval;
    m.meta = meta_data_clone(templ->meta);
  }

  if (lname != NULL) {
    int status = metric_label_set(&m, lname, lvalue);
    if (status != 0) {
      return status;
    }
  }

  int status = metric_family_metric_append(fam, m);
  metric_reset(&m);
  return status;
}

int metric_family_metric_reset(metric_family_t *fam) {
  if (fam == NULL) {
    return EINVAL;
  }

  metric_list_reset(&fam->metric);
  return 0;
}

void metric_family_free(metric_family_t *fam) {
  if (fam == NULL) {
    return;
  }

  free(fam->name);
  free(fam->help);
  metric_list_reset(&fam->metric);
  free(fam);
}

metric_family_t *metric_family_clone(metric_family_t const *fam) {
  if (fam == NULL) {
    errno = EINVAL;
    return NULL;
  }

  metric_family_t *ret = calloc(1, sizeof(*ret));
  if (ret == NULL) {
    return NULL;
  }

  ret->name = strdup(fam->name);
  if (fam->help != NULL) {
    ret->help = strdup(fam->help);
  }
  ret->type = fam->type;

  int status = metric_list_clone(&ret->metric, fam->metric, ret);
  if (status != 0) {
    metric_family_free(ret);
    errno = status;
    return NULL;
  }

  return ret;
}

/* metric_family_unmarshal_identity parses the metric identity and updates
 * "inout" to point to the first character following the identity. With valid
 * input, this means that "inout" will then point either to a '\0' (null byte)
 * or a ' ' (space). */
static int metric_family_unmarshal_identity(metric_family_t *fam,
                                            char const **inout) {
  if ((fam == NULL) || (inout == NULL) || (*inout == NULL)) {
    return EINVAL;
  }

  char const *ptr = *inout;
  size_t name_len = strspn(ptr, VALID_NAME_CHARS);
  if (name_len == 0) {
    return EINVAL;
  }

  char name[name_len + 1];
  strncpy(name, ptr, name_len);
  name[name_len] = 0;
  ptr += name_len;

  fam->name = strdup(name);
  if (fam->name == NULL) {
    return ENOMEM;
  }

  /* metric name without labels */
  if ((ptr[0] == 0) || (ptr[0] == ' ')) {
    *inout = ptr;
    return 0;
  }

  metric_t *m = fam->metric.ptr;

  char const *end = ptr;
  int ret = label_set_unmarshal(&m->label, &end);
  if (ret != 0) {
    return ret;
  }
  ptr = end;

  if ((ptr[0] != '}') || ((ptr[1] != 0) && (ptr[1] != ' '))) {
    return EINVAL;
  }

  *inout = &ptr[1];
  return 0;
}

metric_t *metric_parse_identity(char const *buf) {
  if (buf == NULL) {
    errno = EINVAL;
    return NULL;
  }

  metric_family_t *fam = calloc(1, sizeof(*fam));
  if (fam == NULL) {
    return NULL;
  }
  fam->type = METRIC_TYPE_UNTYPED;

  int status = metric_list_add(&fam->metric, (metric_t){.family = fam});
  if (status != 0) {
    metric_family_free(fam);
    errno = status;
    return NULL;
  }

  status = metric_family_unmarshal_identity(fam, &buf);
  if (status != 0) {
    metric_family_free(fam);
    errno = status;
    return NULL;
  }

  if (buf[0] != 0) {
    metric_family_free(fam);
    errno = EINVAL;
    return NULL;
  }

  return fam->metric.ptr;
}
