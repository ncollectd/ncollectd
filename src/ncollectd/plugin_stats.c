// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "plugin_internal.h"
#include "plugin_stats.h"

void plugin_stats_init(plugin_stats_t *stats)
{
    stats->entries = NULL;
    pthread_mutex_init(&stats->lock, NULL);
}

void plugin_stats_remove(plugin_stats_t *stats, plugin_stats_entry_t *entry)
{
    if (entry == NULL)
        return;

    pthread_mutex_lock(&stats->lock);

    plugin_stats_entry_t *se_prev = NULL;
    plugin_stats_entry_t *se = stats->entries;
    while (se != NULL) {
        if (se == entry) {
            plugin_stats_entry_t *se_next = se->next;
            free(entry);
            if (se_prev == NULL)
                stats->entries = se_next;
            else
                se_prev->next = se_next;
            break;
        }
        se_prev = se;
        se = se->next;
    }

    pthread_mutex_unlock(&stats->lock);
}

plugin_stats_entry_t *plugin_stats_register(plugin_stats_t *stats, const char *plugin)
{
    plugin_stats_entry_t *se = calloc(1, sizeof(*se));
    if (se == NULL) {
        ERROR("calloc failed.");
        return NULL;
    }

    se->plugin = plugin;

    pthread_mutex_lock(&stats->lock);
    se->next = stats->entries;
    stats->entries = se;
    pthread_mutex_unlock(&stats->lock);

    return se;
}

void plugin_stats_add(plugin_stats_entry_t *entry, cdtime_t cpu_user, cdtime_t cpu_sys,
                                                   cdtime_t elapsed, int status)
{
    atomic_fetch_add(&entry->cpu_user, cpu_user);
    atomic_fetch_add(&entry->cpu_sys, cpu_sys);
    atomic_fetch_add(&entry->time, elapsed);
    atomic_fetch_add(&entry->calls, 1);
    if (status != 0)
        atomic_fetch_add(&entry->calls_failures, 1);
}

void plugin_stats_read(plugin_stats_t *stats,
                       metric_family_t *fam_cpu_user, metric_family_t *fam_cpu_system,
                       metric_family_t *fam_time, metric_family_t *fam_calls,
                       metric_family_t *fam_failures)
{
    pthread_mutex_lock(&stats->lock);

    plugin_stats_entry_t *se = stats->entries;
    while(se != NULL) {
        unsigned long long se_time = atomic_load(&se->time);
        unsigned long long se_calls = atomic_load(&se->calls);
        unsigned long long se_calls_failures = atomic_load(&se->calls_failures);
        unsigned long long se_cpu_user = atomic_load(&se->cpu_user);
        unsigned long long se_cpu_sys = atomic_load(&se->cpu_sys);

        metric_family_append(fam_time,
                             VALUE_COUNTER_FLOAT64(CDTIME_T_TO_DOUBLE((cdtime_t)se_time)), NULL,
                             &LABEL_PAIR_CONST("plugin", se->plugin), NULL);
        metric_family_append(fam_calls,
                             VALUE_COUNTER(se_calls), NULL,
                             &LABEL_PAIR_CONST("plugin", se->plugin), NULL);
        metric_family_append(fam_failures,
                             VALUE_COUNTER(se_calls_failures), NULL,
                             &LABEL_PAIR_CONST("plugin", se->plugin), NULL);
        metric_family_append(fam_cpu_user,
                             VALUE_COUNTER_FLOAT64(CDTIME_T_TO_DOUBLE(se_cpu_user)), NULL,
                             &LABEL_PAIR_CONST("plugin", se->plugin), NULL);
        metric_family_append(fam_cpu_system,
                             VALUE_COUNTER_FLOAT64(CDTIME_T_TO_DOUBLE(se_cpu_sys)), NULL,
                             &LABEL_PAIR_CONST("plugin", se->plugin), NULL);

        se = se->next;
    }

    pthread_mutex_unlock(&stats->lock);
}
