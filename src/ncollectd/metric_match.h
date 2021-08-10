/* SPDX-License-Identifier: GPL-2.0-only OR MIT */

#ifndef METRIC_MATCH
#define METRIC_MATCH 1

#include <regex.h>

typedef enum {
  METRIC_MATCH_OP_NONE      = 0,
  METRIC_MATCH_OP_EQL       = 1,
  METRIC_MATCH_OP_NEQ       = 2,
  METRIC_MATCH_OP_EQL_REGEX = 3, 
  METRIC_MATCH_OP_NEQ_REGEX = 4,
  METRIC_MATCH_OP_EXISTS    = 5,
  METRIC_MATCH_OP_NEXISTS   = 6,
} metric_match_op_t;

typedef union {
  char *string;
  regex_t *regex;
} metric_match_value_t;

typedef struct {
  char *name;
  metric_match_op_t op;
  metric_match_value_t value;
} metric_match_pair_t;

typedef struct {
  metric_match_pair_t *ptr;
  size_t num;
} metric_match_set_t;

typedef struct {
  metric_match_set_t family;
  metric_match_set_t labels;
} metric_match_t;


int metric_match_set_add(metric_match_set_t *match, char const *name, metric_match_op_t op, char *value);
int metric_match_add(metric_match_t *match, char const *name, metric_match_op_t op, char *value);

int metric_match_unmarshal(metric_match_t *match, char const *str);

void metric_match_set_reset(metric_match_set_t *match_set);
void metric_match_reset(metric_match_t *match);

#endif
