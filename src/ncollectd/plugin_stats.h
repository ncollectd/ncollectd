/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "ncollectd.h"

#include <stdatomic.h>
#include <pthread.h>

struct plugin_stats_entry_s;
typedef struct plugin_stats_entry_s plugin_stats_entry_t;
struct plugin_stats_entry_s {
    const char *plugin;
    atomic_ullong time;
    atomic_ullong calls;
    atomic_ullong calls_failures;
    atomic_ullong cpu_user;
    atomic_ullong cpu_sys;
    plugin_stats_entry_t *next;
};

typedef struct {
    plugin_stats_entry_t *entries;
    pthread_mutex_t lock;
} plugin_stats_t;

void plugin_stats_init(plugin_stats_t *stats);

void plugin_stats_remove(plugin_stats_t *stats, plugin_stats_entry_t *entry);

plugin_stats_entry_t *plugin_stats_register(plugin_stats_t *stats, const char *plugin);

void plugin_stats_add(plugin_stats_entry_t *entry, cdtime_t cpu_user, cdtime_t cpu_sys,
                                                   cdtime_t elapsed, int status);
void plugin_stats_read(plugin_stats_t *stats,
                       metric_family_t *fam_cpu_user, metric_family_t *fam_cpu_system,
                       metric_family_t *fam_time, metric_family_t *fam_calls,
                       metric_family_t *fam_failures);
