// SPDX-License-Identifier: GPL-2.0-only OR MIT
// Copyright (C) 2014       Pierre-Yves Ritschard
// Authors:
//   Pierre-Yves Ritschard <pyr at spootnik.org>

#include "collectd.h"

#include "utils/avltree/avltree.h"
#include "utils/common/common.h"
#include "utils_threshold.h"

#include <pthread.h>

/*
 * Exported symbols
 * {{{ */
c_avl_tree_t *threshold_tree = NULL;
/* }}} */

/*
 * threshold_t *threshold_get
 *
 * Retrieve one specific threshold configuration. For looking up a threshold
 * matching a metric_t, see "threshold_search" below. Returns NULL if the
 * specified threshold doesn't exist.
 */
threshold_t *threshold_get(const char *metric_name) { /* {{{ */
  threshold_t *th = NULL;

  if (c_avl_get(threshold_tree, metric_name, (void *)&th) == 0)
    return th;
  else
    return NULL;
} /* }}} threshold_t *threshold_get */

