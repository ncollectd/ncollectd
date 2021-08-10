/* SPDX-License-Identifier: GPL-2.0-only OR MIT   */
/* Copyright (C) 2014       Pierre-Yves Ritschard */
/* Authors:                                       */
/*   Pierre-Yves Ritschard <pyr at spootnik.org>  */

#ifndef UTILS_THRESHOLD_H
#define UTILS_THRESHOLD_H 1

#define UT_FLAG_INVERT 0x01
#define UT_FLAG_PERSIST 0x02
#define UT_FLAG_INTERESTING 0x04
#define UT_FLAG_PERSIST_OK 0x08

typedef struct threshold_s {
  char *name;
  label_set_t labels;
  double warning_min;
  double warning_max;
  double failure_min;
  double failure_max;
  double hysteresis;
  unsigned int flags;
  int hits;
  struct threshold_s *next;
} threshold_t;

extern c_avl_tree_t *threshold_tree;

threshold_t *threshold_get(const char *metric_name);

#endif /* UTILS_THRESHOLD_H */
