/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "libmetric/metric.h"
#include "libmetric/metric_match.h"

typedef enum {
    FILTER_RESULT_CONTINUE,
    FILTER_RESULT_STOP,
    FILTER_RESULT_RETURN,
    FILTER_RESULT_DROP
} filter_result_t;

typedef enum {
    FILTER_SCOPE_LOCAL      = (1 <<  0),
    FILTER_SCOPE_PRE_CACHE  = (1 <<  1),
    FILTER_SCOPE_POST_CACHE = (1 <<  2),
} filter_scope_t;

struct plugin_filter_s;
typedef struct plugin_filter_s plugin_filter_t;

struct filter_stmt_s;
typedef struct filter_stmt_s filter_stmt_t;

plugin_filter_t *filter_global_get_by_name(const char *name);
int filter_global_configure(const config_item_t *ci);
plugin_filter_t *filter_configure(const config_item_t *ci);

int plugin_filter_configure(const config_item_t *ci, plugin_filter_t **filter);
void plugin_filter_free(plugin_filter_t *filter);

int filter_process(plugin_filter_t *filter, metric_family_list_t *fam_list);

void filter_free(plugin_filter_t *filter);
void filter_reset(plugin_filter_t *filter);
void filter_global_free(void);
