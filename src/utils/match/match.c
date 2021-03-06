/**
 * collectd - src/utils_match.c
 * Copyright (C) 2008-2014  Florian octo Forster
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
 **/

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

#include "utils/match/match.h"

#include <regex.h>

#define UTILS_MATCH_FLAGS_EXCLUDE_REGEX 0x02
#define UTILS_MATCH_FLAGS_REGEX 0x04

struct cu_match_s {
  regex_t regex;
  regex_t excluderegex;
  int flags;

  int (*callback)(const char *str, char *const *matches, size_t matches_num,
                  void *user_data);
  void *user_data;
  void (*free)(void *user_data);
};

/*
 * Private functions
 */
static char *match_substr(const char *str, int begin, int end) {
  char *ret;
  size_t ret_len;

  if ((begin < 0) || (end < 0) || (begin >= end))
    return NULL;
  if ((size_t)end > (strlen(str) + 1)) {
    ERROR("utils_match: match_substr: `end' points after end of string.");
    return NULL;
  }

  ret_len = end - begin;
  ret = malloc(ret_len + 1);
  if (ret == NULL) {
    ERROR("utils_match: match_substr: malloc failed.");
    return NULL;
  }

  sstrncpy(ret, str + begin, ret_len + 1);
  return ret;
} /* char *match_substr */

static int default_callback(const char __attribute__((unused)) * str,
                            char *const *matches, size_t matches_num,
                            void *user_data) {
  cu_match_value_t *data = (cu_match_value_t *)user_data;

  if (data->ds_type & UTILS_MATCH_DS_TYPE_GAUGE) {
    char *endptr = NULL;

    if (data->ds_type & UTILS_MATCH_CF_GAUGE_INC) {
      data->value.gauge.float64 = isnan(data->value.gauge.float64) ? 1 : data->value.gauge.float64 + 1;
      data->values_num++;
      return 0;
    }

    if (matches_num < 2)
      return -1;

    double value = (double)strtod(matches[1], &endptr);
    if (matches[1] == endptr)
      return -1;

    if (data->ds_type & UTILS_MATCH_CF_GAUGE_DIST) {
      latency_counter_add(data->latency, DOUBLE_TO_CDTIME_T(value));
      data->values_num++;
      return 0;
    }

    if ((data->values_num == 0) ||
        (data->ds_type & UTILS_MATCH_CF_GAUGE_LAST) ||
        (data->ds_type & UTILS_MATCH_CF_GAUGE_PERSIST)) {
      data->value.gauge.float64 = value;
    } else if (data->ds_type & UTILS_MATCH_CF_GAUGE_AVERAGE) {
      double f = ((double)data->values_num) / ((double)(data->values_num + 1));
      data->value.gauge.float64 = (data->value.gauge.float64 * f) + (value * (1.0 - f));
    } else if (data->ds_type & UTILS_MATCH_CF_GAUGE_MIN) {
      if (data->value.gauge.float64 > value)
        data->value.gauge.float64 = value;
    } else if (data->ds_type & UTILS_MATCH_CF_GAUGE_MAX) {
      if (data->value.gauge.float64 < value)
        data->value.gauge.float64 = value;
    } else if (data->ds_type & UTILS_MATCH_CF_GAUGE_ADD) {
      data->value.gauge.float64 += value;
    } else {
      ERROR("utils_match: default_callback: obj->ds_type is invalid!");
      return -1;
    }

    data->values_num++;
  } else if (data->ds_type & UTILS_MATCH_DS_TYPE_COUNTER) {
    char *endptr = NULL;

    if (data->ds_type & UTILS_MATCH_CF_COUNTER_INC) {
      data->value.counter.uint64++;
      data->values_num++;
      return 0;
    }

    if (matches_num < 2)
      return -1;

    int64_t value = (int64_t)strtoull(matches[1], &endptr, 0);
    if (matches[1] == endptr)
      return -1;

    if (data->ds_type & UTILS_MATCH_CF_COUNTER_SET)
      data->value.counter.uint64 = value;
    else if (data->ds_type & UTILS_MATCH_CF_COUNTER_ADD)
      data->value.counter.uint64 += value;
    else {
      ERROR("utils_match: default_callback: obj->ds_type is invalid!");
      return -1;
    }

    data->values_num++;
  } else {
    ERROR("utils_match: default_callback: obj->ds_type is invalid!");
    return -1;
  }

  return 0;
} /* int default_callback */

static void match_simple_free(void *data) {
  cu_match_value_t *user_data = (cu_match_value_t *)data;
  if (user_data->latency)
    latency_counter_destroy(user_data->latency);

  free(data);
} /* void match_simple_free */

/*
 * Public functions
 */
cu_match_t *
match_create_callback(const char *regex, const char *excluderegex,
                      int (*callback)(const char *str, char *const *matches,
                                      size_t matches_num, void *user_data),
                      void *user_data,
                      void (*free_user_data)(void *user_data)) {
  cu_match_t *obj;
  int status;

  DEBUG("utils_match: match_create_callback: regex = %s, excluderegex = %s",
        regex, excluderegex);

  obj = calloc(1, sizeof(*obj));
  if (obj == NULL)
    return NULL;

  status = regcomp(&obj->regex, regex, REG_EXTENDED | REG_NEWLINE);
  if (status != 0) {
    ERROR("Compiling the regular expression \"%s\" failed.", regex);
    sfree(obj);
    return NULL;
  }
  obj->flags |= UTILS_MATCH_FLAGS_REGEX;

  if (excluderegex && strcmp(excluderegex, "") != 0) {
    status = regcomp(&obj->excluderegex, excluderegex, REG_EXTENDED);
    if (status != 0) {
      ERROR("Compiling the excluding regular expression \"%s\" failed.",
            excluderegex);
      sfree(obj);
      return NULL;
    }
    obj->flags |= UTILS_MATCH_FLAGS_EXCLUDE_REGEX;
  }

  obj->callback = callback;
  obj->user_data = user_data;
  obj->free = free_user_data;

  return obj;
} /* cu_match_t *match_create_callback */

cu_match_t *match_create_simple(const char *regex, const char *excluderegex,
                                int match_ds_type) {
  cu_match_value_t *user_data;
  cu_match_t *obj;

  user_data = calloc(1, sizeof(*user_data));
  if (user_data == NULL)
    return NULL;
  user_data->ds_type = match_ds_type;

  if ((match_ds_type & UTILS_MATCH_DS_TYPE_GAUGE) &&
      (match_ds_type & UTILS_MATCH_CF_GAUGE_DIST)) {
    user_data->latency = latency_counter_create();
    if (user_data->latency == NULL) {
      ERROR("match_create_simple(): latency_counter_create() failed.");
      free(user_data);
      return NULL;
    }
  }

  obj = match_create_callback(regex, excluderegex, default_callback, user_data,
                              match_simple_free);
  if (obj == NULL) {
    if (user_data->latency)
      latency_counter_destroy(user_data->latency);

    sfree(user_data);
    return NULL;
  }
  return obj;
} /* cu_match_t *match_create_simple */

void match_value_reset(cu_match_value_t *mv) {
  if (mv == NULL)
    return;

  /* Reset GAUGE metrics only and except GAUGE_PERSIST. */
  if ((mv->ds_type & UTILS_MATCH_DS_TYPE_GAUGE) &&
      !(mv->ds_type & UTILS_MATCH_CF_GAUGE_PERSIST)) {
    mv->value.gauge.float64 = (mv->ds_type & UTILS_MATCH_CF_GAUGE_INC) ? 0 : NAN;
    mv->values_num = 0;
  }
} /* }}} void match_value_reset */

void match_destroy(cu_match_t *obj) {
  if (obj == NULL)
    return;

  if (obj->flags & UTILS_MATCH_FLAGS_REGEX)
    regfree(&obj->regex);
  if (obj->flags & UTILS_MATCH_FLAGS_EXCLUDE_REGEX)
    regfree(&obj->excluderegex);
  if ((obj->user_data != NULL) && (obj->free != NULL))
    (*obj->free)(obj->user_data);

  sfree(obj);
} /* void match_destroy */

int match_apply(cu_match_t *obj, const char *str) {
  int status;
  regmatch_t re_match[32];
  char *matches[32] = {0};
  size_t matches_num;

  if ((obj == NULL) || (str == NULL))
    return -1;

  if (obj->flags & UTILS_MATCH_FLAGS_EXCLUDE_REGEX) {
    status =
        regexec(&obj->excluderegex, str, STATIC_ARRAY_SIZE(re_match), re_match,
                /* eflags = */ 0);
    /* Regex did match, so exclude this line */
    if (status == 0) {
      DEBUG("ExludeRegex matched, don't count that line\n");
      return 0;
    }
  }

  status = regexec(&obj->regex, str, STATIC_ARRAY_SIZE(re_match), re_match,
                   /* eflags = */ 0);

  /* Regex did not match */
  if (status != 0)
    return 0;

  for (matches_num = 0; matches_num < STATIC_ARRAY_SIZE(matches);
       matches_num++) {
    if ((re_match[matches_num].rm_so < 0) || (re_match[matches_num].rm_eo < 0))
      break;

    matches[matches_num] = match_substr(str, re_match[matches_num].rm_so,
                                        re_match[matches_num].rm_eo);
    if (matches[matches_num] == NULL) {
      status = -1;
      break;
    }
  }

  if (status != 0) {
    ERROR("utils_match: match_apply: match_substr failed.");
  } else {
    status = obj->callback(str, matches, matches_num, obj->user_data);
    if (status != 0) {
      ERROR("utils_match: match_apply: callback failed.");
    }
  }

  for (size_t i = 0; i < matches_num; i++) {
    sfree(matches[i]);
  }

  return status;
} /* int match_apply */

void *match_get_user_data(cu_match_t *obj) {
  if (obj == NULL)
    return NULL;
  return obj->user_data;
} /* void *match_get_user_data */
