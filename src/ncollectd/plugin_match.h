/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#define MATCH_METRIC_TYPE_MASK  0xf000

typedef enum {
    MATCH_METRIC_TYPE_GAUGE         = 0x1000,
    MATCH_METRIC_TYPE_GAUGE_AVERAGE = 0x1001,
    MATCH_METRIC_TYPE_GAUGE_MIN     = 0x1002,
    MATCH_METRIC_TYPE_GAUGE_MAX     = 0x1004,
    MATCH_METRIC_TYPE_GAUGE_LAST    = 0x1008,
    MATCH_METRIC_TYPE_GAUGE_INC     = 0x1010,
    MATCH_METRIC_TYPE_GAUGE_ADD     = 0x1020,
    MATCH_METRIC_TYPE_GAUGE_PERSIST = 0x1040,
    MATCH_METRIC_TYPE_COUNTER       = 0x2000,
    MATCH_METRIC_TYPE_COUNTER_SET   = 0x2001,
    MATCH_METRIC_TYPE_COUNTER_ADD   = 0x2002,
    MATCH_METRIC_TYPE_COUNTER_INC   = 0x2004,
} match_metric_type_t;

struct match_metric_family_set_s;
typedef struct match_metric_family_set_s match_metric_family_set_t;

typedef struct {
    int (*config)(const config_item_t *ci, void **user_data);
    void (*destroy)(void *user_data);
    int (*match)(match_metric_family_set_t *set, char *buffer, void *user_data);
} plugin_match_proc_t;

struct plugin_match_s;
typedef struct plugin_match_s plugin_match_t;

int plugin_register_match(const char *name, plugin_match_proc_t proc);

void plugin_free_register_match(void);

int plugin_match_config(const config_item_t *ci, plugin_match_t **plugin_match_list);

int plugin_match(plugin_match_t *plugin_match_list, char *str);

void plugin_match_shutdown(plugin_match_t *plugin_match_list);

int plugin_match_dispatch(plugin_match_t *plugin_match_list, plugin_filter_t *filter,
                          label_set_t *labels, bool reset);

int plugin_match_metric_family_set_add(match_metric_family_set_t *set,
                                       char *name, char *help, char *unit,
                                       match_metric_type_t type, label_set_t *labels,
                                       char const *svalue, cdtime_t t);

int cf_util_get_match_metric_type(const config_item_t *ci, match_metric_type_t *type);
