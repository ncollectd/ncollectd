// SPDX-License-Identifier: GPL-2.0-only OR MIT
// Copyright (C) 2013       Florian octo Forster
// Authors:
//   Florian octo Forster <octo at collectd.org>

#include "utils_cache.h"
#include <errno.h>

#include <errno.h>

gauge_t *uc_get_rate_vl(__attribute__((unused)) data_set_t const *ds,
                        __attribute__((unused)) value_list_t const *vl) {
  errno = ENOTSUP;
  return NULL;
}

int uc_get_rate(__attribute__((unused)) metric_t const *m,
                __attribute__((unused)) gauge_t *ret_value) {
  return ENOTSUP;
}

int uc_get_rate_by_name(const char *name, gauge_t *ret_value) {
  return ENOTSUP;
}

int uc_get_names(char ***ret_names, cdtime_t **ret_times, size_t *ret_number) {
  return ENOTSUP;
}

int uc_get_value_by_name_vl(const char *name, value_t **ret_values,
                            size_t *ret_values_num) {
  return ENOTSUP;
}

int uc_meta_data_get_signed_int(metric_t const *m, const char *key,
                                int64_t *value) {
  return -ENOENT;
}

int uc_meta_data_get_unsigned_int(metric_t const *m, const char *key,
                                  uint64_t *value) {
  return -ENOENT;
}

int uc_meta_data_add_signed_int(metric_t const *m, const char *key,
                                int64_t value) {
  return 0;
}

int uc_meta_data_add_unsigned_int(metric_t const *m, const char *key,
                                  uint64_t value) {
  return 0;
}
