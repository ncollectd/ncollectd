/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef METRIC_SET_H
#define METRIC_SET_H 1

typedef struct {

}

typedef struct {
  char *name;
  label_set_t label;
};

typedef struct {
  size_t alloc;
  size_t size;
  metric_cache_id_t *ptr;
} metric_set_t;


int metric_set_add(metric_set_t *set, char *name, label_set_t *label);
int metric_set_reset(metric_set_t *set);

#endif
