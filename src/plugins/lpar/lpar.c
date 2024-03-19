// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2010 Aurélien Reynaud
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Aurélien Reynaud <collectd at wattapower.net>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <libperfstat.h>
#include <sys/protosw.h>
#include <sys/utsname.h>

/* XINTFRAC was defined in libperfstat.h somewhere between AIX 5.3 and 6.1 */
#ifndef XINTFRAC
#include <sys/systemcfg.h>
#define XINTFRAC  ((double)(_system_configuration.Xint) / (double)(_system_configuration.Xfrac))
#endif

#define CLOCKTICKS_TO_TICKS(cticks) ((cticks) / XINTFRAC)

static bool pool_stats;
#ifdef PERFSTAT_SUPPORTS_DONATION
static bool donate_flag;
#endif

static perfstat_partition_total_t lparstats_old;

enum {
    FAM_LPAR_ENTITLED,
    FAM_LPAR_USER,
    FAM_LPAR_SYSTEM,
    FAM_LPAR_WAIT,
    FAM_LPAR_IDLE,
    FAM_LPAR_CONSUMED,
    FAM_LPAR_IDLE_DONATED,
    FAM_LPAR_BUSY_DONATED,
    FAM_LPAR_IDLE_STOLEN,
    FAM_LPAR_BUSY_STOLEN,
    FAM_LPAR_POOL_IDLE,
    FAM_LPAR_POOL_BUSY,
    FAM_LPAR_MAX,
};

static metric_family_t fams[FAM_LPAR_MAX] = {
    [FAM_LPAR_ENTITLED] = {
        .name = "system_lpar_entitled",
        .type = METRIC_TYPE_GAUGE,
        .help = "The entitled processing capacity in processor units."
    },
    [FAM_LPAR_USER] = {
        .name = "system_lpar_user",
        .type = METRIC_TYPE_GAUGE,
        .help = "The percentage of the entitled processing capacity used "
                "while executing at the user level."
    },
    [FAM_LPAR_SYSTEM] = {
        .name = "system_lpar_system",
        .type = METRIC_TYPE_GAUGE,
        .help = "The percentage of the entitled processing capacity used "
                "while executing at the system level."
    },
    [FAM_LPAR_WAIT] = {
        .name = "system_lpar_wait",
        .type = METRIC_TYPE_GAUGE,
        .help = "The percentage of the entitled processing capacity unused "
                "while the partition was idle and had outstanding disk I/O request(s)."
    },
    [FAM_LPAR_IDLE] = {
        .name = "system_lpar_idle",
        .type = METRIC_TYPE_GAUGE,
        .help = "The percentage of the entitled processing capacity unused "
                "while the partition was idle and did not have any outstanding disk I/O request."
    },
    [FAM_LPAR_CONSUMED] = {
        .name = "system_lpar_consumed",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of physical processors consumed."
    },
    [FAM_LPAR_IDLE_DONATED] = {
        .name = "system_lpar_idle_donated",
        .type = METRIC_TYPE_GAUGE,
        .help = "The percentage of physical processor that is used by "
                "explicitly donated idle cycles, for dedicated partitions only."
    },
    [FAM_LPAR_BUSY_DONATED] = {
        .name = "system_lpar_busy_donated",
        .type = METRIC_TYPE_GAUGE,
        .help = "The percentage of physical processor that is used by "
                "donating busy cycles, for dedicated partitions only."
    },
    [FAM_LPAR_IDLE_STOLEN] = {
        .name = "system_lpar_idle_stolen",
        .type = METRIC_TYPE_GAUGE,
        .help = "The percentage of physical processor idle cycles stolen "
                "by the hypervisor from a dedicated partition."
    },
    [FAM_LPAR_BUSY_STOLEN] = {
        .name = "system_lpar_busy_stolen",
        .type = METRIC_TYPE_GAUGE,
        .help = "The percentage of physical processor busy cycles stolen "
                "by the hypervisor from a dedicated partition."
    },
    [FAM_LPAR_POOL_IDLE] = {
        .name = "system_lpar_pool_busy",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_LPAR_POOL_BUSY] = {
        .name = "system_lpar_pool_idle",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
};

static int lpar_read(void)
{
    perfstat_partition_total_t lparstats;
    struct utsname name;

    /* An LPAR has the same serial number as the physical system it is currently
         running on. It is a convenient way of tracking LPARs as they are moved
         from chassis to chassis through Live Partition Mobility (LPM). */
    if (uname(&name) != 0) {
        PLUGIN_ERROR("uname failed.");
        return -1;
    }

    /* Retrieve the current metrics. Returns the number of structures filled. */
    int status = perfstat_partition_total(NULL, &lparstats, sizeof(perfstat_partition_total_t), 1);
    if (status != 1) {
        PLUGIN_ERROR("perfstat_partition_total failed: %s (%i)", STRERRNO, status);
        return -1;
    }

    /* Number of ticks since we last run. */
    u_longlong_t ticks = lparstats.timebase_last - lparstats_old.timebase_last;
    if (ticks == 0) {
        /* The stats have not been updated. Return now to avoid dividing by zero */
        return 0;
    }

    /*
     * On a shared partition, we're "entitled" to a certain amount of
     * processing power, for example 250/100 of a physical CPU. Processing
     * capacity not used by the partition may be assigned to a different
     * partition by the hypervisor, so "idle" is hopefully a very small
     * number.
     *
     * A dedicated partition may donate its CPUs to another partition and
     * may steal ticks from somewhere else (another partition or maybe the
     * shared pool, I don't know --octo).
     */

    /* entitled_proc_capacity is in 1/100th of a CPU */
    double entitled_proc_capacity = 0.01 * ((double)lparstats.entitled_proc_capacity);
    metric_family_append(&fams[FAM_LPAR_ENTITLED], VALUE_GAUGE(entitled_proc_capacity), NULL,
                         &(label_pair_const_t){.name="serial", .value=name.machine}, NULL);

    /* The number of ticks actually spent in the various states */
    u_longlong_t user_ticks = lparstats.puser - lparstats_old.puser;
    u_longlong_t syst_ticks = lparstats.psys - lparstats_old.psys;
    u_longlong_t wait_ticks = lparstats.pwait - lparstats_old.pwait;
    u_longlong_t idle_ticks = lparstats.pidle - lparstats_old.pidle;
    u_longlong_t consumed_ticks = user_ticks + syst_ticks + wait_ticks + idle_ticks;

    metric_family_append(&fams[FAM_LPAR_USER],
                         VALUE_GAUGE((double)user_ticks / (double)ticks), NULL,
                         &(label_pair_const_t){.name="serial", .value=name.machine}, NULL);
    metric_family_append(&fams[FAM_LPAR_SYSTEM],
                         VALUE_GAUGE((double)syst_ticks / (double)ticks), NULL,
                         &(label_pair_const_t){.name="serial", .value=name.machine}, NULL);
    metric_family_append(&fams[FAM_LPAR_WAIT],
                         VALUE_GAUGE((double)wait_ticks / (double)ticks), NULL,
                         &(label_pair_const_t){.name="serial", .value=name.machine}, NULL);
    metric_family_append(&fams[FAM_LPAR_IDLE],
                         VALUE_GAUGE((double)idle_ticks / (double)ticks), NULL,
                         &(label_pair_const_t){.name="serial", .value=name.machine}, NULL);

#ifdef PERFSTAT_SUPPORTS_DONATION
    if (donate_flag) {
        /* donated => ticks given to another partition
         * stolen  => ticks received from another partition */
        u_longlong_t idle_donated_ticks, busy_donated_ticks;
        u_longlong_t idle_stolen_ticks, busy_stolen_ticks;

        /* FYI:  PURR == Processor Utilization of Resources Register
         *          SPURR == Scaled PURR */
        idle_donated_ticks = lparstats.idle_donated_purr - lparstats_old.idle_donated_purr;
        busy_donated_ticks = lparstats.busy_donated_purr - lparstats_old.busy_donated_purr;
        idle_stolen_ticks = lparstats.idle_stolen_purr - lparstats_old.idle_stolen_purr;
        busy_stolen_ticks = lparstats.busy_stolen_purr - lparstats_old.busy_stolen_purr;

        metric_family_append(&fams[FAM_LPAR_IDLE_DONATED],
                             VALUE_GAUGE((double)idle_donated_ticks / (double)ticks), NULL,
                             &(label_pair_const_t){.name="serial", .value=name.machine}, NULL);
        metric_family_append(&fams[FAM_LPAR_BUSY_DONATED],
                             VALUE_GAUGE((double)busy_donated_ticks / (double)ticks), NULL,
                             &(label_pair_const_t){.name="serial", .value=name.machine}, NULL);
        metric_family_append(&fams[FAM_LPAR_IDLE_STOLEN],
                             VALUE_GAUGE((double)idle_stolen_ticks / (double)ticks), NULL,
                             &(label_pair_const_t){.name="serial", .value=name.machine}, NULL);
        metric_family_append(&fams[FAM_LPAR_BUSY_STOLEN],
                             VALUE_GAUGE((double)busy_stolen_ticks / (double)ticks), NULL,
                             &(label_pair_const_t){.name="serial", .value=name.machine}, NULL);

        /* Donated ticks will be accounted for as stolen ticks in other LPARs */
        consumed_ticks += idle_stolen_ticks + busy_stolen_ticks;
    }
#endif

    metric_family_append(&fams[FAM_LPAR_CONSUMED],
                         VALUE_GAUGE((double)consumed_ticks / (double)ticks), NULL,
                         &(label_pair_const_t){.name="serial", .value=name.machine}, NULL);

    if (pool_stats) {
        /* We're calculating "busy" from "idle" and the total number of
         * CPUs, because the "busy" member didn't exist in early versions
         * of libperfstat. It was added somewhere between AIX 5.3 ML5 and ML9. */
        u_longlong_t pool_idle_cticks = lparstats.pool_idle_time - lparstats_old.pool_idle_time;
        double pool_idle_cpus = CLOCKTICKS_TO_TICKS((double)pool_idle_cticks) / (double)ticks;
        double pool_busy_cpus = ((double)lparstats.phys_cpus_pool) - pool_idle_cpus;
        if (pool_busy_cpus < 0.0)
            pool_busy_cpus = 0.0;

        char pool_id[24];
        ssnprintf(pool_id, sizeof(pool_id), "%X", (unsigned int)lparstats.pool_id);

        metric_family_append(&fams[FAM_LPAR_POOL_BUSY],
                             VALUE_GAUGE(pool_busy_cpus), NULL,
                             &(label_pair_const_t){.name="serial", .value=name.machine},
                             &(label_pair_const_t){.name="pool_id", .value=pool_id}, NULL);
        metric_family_append(&fams[FAM_LPAR_POOL_IDLE],
                             VALUE_GAUGE(pool_idle_cpus), NULL,
                             &(label_pair_const_t){.name="serial", .value=name.machine},
                             &(label_pair_const_t){.name="pool_id", .value=pool_id}, NULL);
    }

    memcpy(&lparstats_old, &lparstats, sizeof(lparstats_old));

    plugin_dispatch_metric_family_array(fams, FAM_LPAR_MAX, 0);
    return 0;
}

static int lpar_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "cpu-pool-stats") == 0) {
            status = cf_util_get_boolean(child, &pool_stats);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int lpar_init(void)
{
    /* Retrieve the initial metrics. Returns the number of structures filled. */
    int status = perfstat_partition_total(NULL, &lparstats_old, sizeof(perfstat_partition_total_t),
                                          1);
    if (status != 1) {
        PLUGIN_ERROR("perfstat_partition_total failed: %s (%i)", STRERRNO, status);
        return -1;
    }

#ifdef PERFSTAT_SUPPORTS_DONATION
    if (!lparstats_old.type.b.shared_enabled && lparstats_old.type.b.donate_enabled)
        donate_flag = true;
#endif

    if (pool_stats && !lparstats_old.type.b.pool_util_authority) {
        PLUGIN_WARNING("This partition does not have pool authority. "
                        "Disabling CPU pool statistics collection.");
        pool_stats = false;
    }

    return 0;
}


void module_register(void)
{
    plugin_register_config("lpar", lpar_config);
    plugin_register_init("lpar", lpar_init);
    plugin_register_read("lpar", lpar_read);
}
