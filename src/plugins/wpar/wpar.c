// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2010-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <sys/proc.h>
#include <sys/protosw.h>
#include <libperfstat.h>

enum {
    FAM_WPAR_CPU_USER,
    FAM_WPAR_CPU_SYSTEM,
    FAM_WPAR_LOAD_1M,
    FAM_WPAR_LOAD_5M,
    FAM_WPAR_LOAD_15M,
    FAM_WPAR_MEMORY_USER_BYTES,
    FAM_WPAR_MEMORY_FREE_BYTES,
    FAM_WPAR_MEMORY_CACHED_BYTES,
    FAM_WPAR_MEMORY_TOTAL_BYTES,
    FAM_WPAR_MAX
};

static metric_family_t fams[FAM_WPAR_MAX] = {
    [FAM_WPAR_CPU_USER] = {
        .name = "system_wpar_cpu_user",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_WPAR_CPU_SYSTEM] = {
        .name = "system_wpar_cpu_system",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_WPAR_LOAD_1M] = {
        .name = "system_wpar_load_1m",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_WPAR_LOAD_5M] = {
        .name = "system_wpar_load_5m",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_WPAR_LOAD_15M] = {
        .name = "system_wpar_load_15m",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_WPAR_MEMORY_USER_BYTES] = {
        .name = "system_wpar_memory_user_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_WPAR_MEMORY_FREE_BYTES] = {
        .name = "system_wpar_memory_free_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_WPAR_MEMORY_CACHED_BYTES] = {
        .name = "system_wpar_memory_cached_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_WPAR_MEMORY_TOTAL_BYTES] = {
        .name = "system_wpar_memory_total_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
};

static int pagesize;
static int wpar_total_num;
static perfstat_wpar_total_t *wpar_total = NULL;
static unsigned long long timebase_saved;
static time_t time_saved;

typedef struct wpar_cpu {
    u_longlong_t user;
    u_longlong_t sys;
} wpar_cpu_t;

static wpar_cpu_t *prev_wcpu;
static wpar_cpu_t *cnt_wcpu;

static int wpar_read (void)
{
    /* Read the partition information to get the cpu time base */
    perfstat_partition_total_t part;
    int status = perfstat_partition_total(NULL, &part, sizeof(perfstat_partition_total_t), 1);
    if (status < 0) {
        PLUGIN_WARNING("perfstat_partition_total failed: %s", STRERRNO);
        return -1;
    }

    unsigned long long hardware_ticks = 0;
    if (timebase_saved > 0)
        hardware_ticks = part.timebase_last - timebase_saved;

    timebase_saved = part.timebase_last;

    time_t time_now = time(NULL);
    time_t time_diff = time_now - time_saved;
    time_saved = time_now;

    /* Read the number of partitions */
    int nwpar = perfstat_wpar_total (NULL, NULL, sizeof (perfstat_wpar_total_t), 0);
    if (nwpar < 0) {
        PLUGIN_WARNING("perfstat_wpar_total failed: %s", STRERRNO);
        return -1;
    }

    if (nwpar == 0)
        return 0;

    /* If necessary, reallocate the "wpar_total" memory. */
    if ((wpar_total_num != nwpar) || (wpar_total == NULL)) {
        perfstat_wpar_total_t *tmp = realloc (wpar_total, nwpar * sizeof (*wpar_total));
        if (tmp == NULL) {
            PLUGIN_ERROR("realloc failed.");
            return -1;
        }
        wpar_total = tmp;

        free(prev_wcpu);
        prev_wcpu = calloc (nwpar, sizeof (wpar_cpu_t));
        if (prev_wcpu == NULL) {
            PLUGIN_ERROR("calloc failed.");
            return -1;
        }

        free(cnt_wcpu);
        cnt_wcpu = calloc (nwpar, sizeof (wpar_cpu_t));
        if (cnt_wcpu == NULL) {
            PLUGIN_ERROR("calloc failed.");
            return -1;
        }
        /* Skip one loop in cpu collection to fill prev_wcpu */
        hardware_ticks = 0;
    }
    wpar_total_num = nwpar;

    /* Now actually query the data */
    perfstat_id_wpar_t id_wpar = {.spec = WPARNAME };
    nwpar = perfstat_wpar_total (&id_wpar, wpar_total, sizeof(perfstat_wpar_total_t), wpar_total_num);
    if (nwpar < 0) {
        PLUGIN_WARNING("perfstat_wpar_total failed: %s", STRERRNO);
        return -1;
    }

    if (nwpar > wpar_total_num) {
        PLUGIN_INFO("Number of WPARs increased during allocation. "
                    "Will ignore %i WPAR(s).", nwpar - wpar_total_num);
        nwpar = wpar_total_num;
    }

    /* Iterate over all WPARs and dispatch information */
    for (int i = 0; i < nwpar; i++) {
        const char *wname = wpar_total[i].name;

        /* Update the ID structure */
        memset (&id_wpar, 0, sizeof (id_wpar));
        id_wpar.spec = WPARID;
        id_wpar.u.wpar_id = wpar_total[i].wpar_id;

        /* Memory */
        perfstat_memory_total_wpar_t wmemory;
        status = perfstat_memory_total_wpar(&id_wpar, &wmemory, sizeof(wmemory), 1);
        if (status < 0) {
            PLUGIN_WARNING("perfstat_memory_total_wpar(%s) failed: %s", wname, STRERRNO);
            continue;
        }

        metric_family_append(&fams[FAM_WPAR_MEMORY_USER_BYTES],
                             VALUE_GAUGE(wmemory.real_inuse * pagesize), NULL,
                             &(label_pair_const_t){.name="wpar_name", .value=wname}, NULL);
        metric_family_append(&fams[FAM_WPAR_MEMORY_FREE_BYTES],
                             VALUE_GAUGE(wmemory.real_free * pagesize), NULL,
                             &(label_pair_const_t){.name="wpar_name", .value=wname}, NULL);
        metric_family_append(&fams[FAM_WPAR_MEMORY_CACHED_BYTES],
                             VALUE_GAUGE(wmemory.numperm * pagesize), NULL,
                             &(label_pair_const_t){.name="wpar_name", .value=wname}, NULL);
        metric_family_append(&fams[FAM_WPAR_MEMORY_TOTAL_BYTES],
                             VALUE_GAUGE(wmemory.real_total * pagesize), NULL,
                             &(label_pair_const_t){.name="wpar_name", .value=wname}, NULL);

        /* CPU and load */
        perfstat_cpu_total_wpar_t wcpu;
        status = perfstat_cpu_total_wpar(&id_wpar, &wcpu, sizeof(wcpu), 1);
        if (status < 0) {
            PLUGIN_WARNING("perfstat_cpu_total_wpar(%s) failed: %s", wname, STRERRNO);
            continue;
        }

        double factor = 1.0 / ((double) (1 << SBITS));
        float snum = ((double) wcpu.loadavg[0]) * factor;
        metric_family_append(&fams[FAM_WPAR_LOAD_1M], VALUE_GAUGE(snum), NULL,
                             &(label_pair_const_t){.name="wpar_name", .value=wname}, NULL);
        double mnum = ((double) wcpu.loadavg[1]) * factor;
        metric_family_append(&fams[FAM_WPAR_LOAD_5M], VALUE_GAUGE(mnum), NULL,
                             &(label_pair_const_t){.name="wpar_name", .value=wname}, NULL);
        double lnum = ((double) wcpu.loadavg[2]) * factor;
        metric_family_append(&fams[FAM_WPAR_LOAD_15M], VALUE_GAUGE(lnum), NULL,
                             &(label_pair_const_t){.name="wpar_name", .value=wname}, NULL);

        if (hardware_ticks > 0) {
            /* Number of physical processors */
           int pncpus = wcpu.ncpus;
            if (part.smt_thrds > 0 )
                pncpus = wcpu.ncpus / part.smt_thrds;

            u_longlong_t diff_sys = (wcpu.psys - prev_wcpu[i].sys) * 100 * time_diff;
            cnt_wcpu[i].sys += (diff_sys / hardware_ticks) / pncpus;

            u_longlong_t diff_user = (wcpu.puser - prev_wcpu[i].user) * 100 * time_diff;
            cnt_wcpu[i].user += (diff_user / hardware_ticks) / pncpus;
        }

        metric_family_append(&fams[FAM_WPAR_CPU_USER], VALUE_COUNTER(cnt_wcpu[i].user), NULL,
                             &(label_pair_const_t){.name="wpar_name", .value=wname}, NULL);
        metric_family_append(&fams[FAM_WPAR_CPU_SYSTEM], VALUE_COUNTER(cnt_wcpu[i].sys), NULL,
                             &(label_pair_const_t){.name="wpar_name", .value=wname}, NULL);

        prev_wcpu[i].sys = wcpu.psys;
        prev_wcpu[i].user = wcpu.puser;
    }

    plugin_dispatch_metric_family_array(fams, FAM_WPAR_MAX, 0);

    return 0;
}

static int wpar_init(void)
{
    pagesize = getpagesize ();
    return 0;
}

void module_register (void)
{
    plugin_register_init ("wpar", wpar_init);
    plugin_register_read ("wpar", wpar_read);
}
