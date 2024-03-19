/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "ncollectd.h"
#include "libmetric/metric.h"

#include <regex.h>

typedef enum {
    METRIC_MATCH_OP_NONE,
    METRIC_MATCH_OP_EQL,
    METRIC_MATCH_OP_NEQ,
    METRIC_MATCH_OP_EQL_REGEX,
    METRIC_MATCH_OP_NEQ_REGEX,
    METRIC_MATCH_OP_EXISTS,
    METRIC_MATCH_OP_NEXISTS
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
    metric_match_pair_t **ptr;
    size_t num;
} metric_match_set_t;

typedef struct {
    metric_match_set_t *name;
    metric_match_set_t *labels;
} metric_match_t;

void metric_match_pair_free(metric_match_pair_t *pair);

metric_match_pair_t *metric_match_pair_alloc(char const *name, metric_match_op_t op, char *value);

void metric_match_set_free(metric_match_set_t *match);

metric_match_set_t *metric_match_set_alloc(void);

int metric_match_set_append(metric_match_set_t *match, metric_match_pair_t *pair);

int metric_match_set_add(metric_match_set_t *match, const char *name,
                                                    metric_match_op_t op, char *value);

int metric_match_add(metric_match_t *match, const char *name, metric_match_op_t op, char *value);

int metric_match_unmarshal(metric_match_t *match, const char *str);

void metric_match_reset(metric_match_t *match);

bool metric_match_cmp(metric_match_t *match, const char *name, const label_set_t *labels);
