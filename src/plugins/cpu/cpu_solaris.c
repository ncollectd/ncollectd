// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2014 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2008 Oleg King
// SPDX-FileCopyrightText: Copyright (C) 2009 Simon Kuhnle
// SPDX-FileCopyrightText: Copyright (C) 2009-2024 Manuel Sanmartín
// SPDX-FileCopyrightText: Copyright (C) 2013-2014 Pierre-Yves Ritschard
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Oleg King <king2 at kaluga.ru>
// SPDX-FileContributor: Simon Kuhnle <simon at blarzwurst.de>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>
// SPDX-FileContributor: Pierre-Yves Ritschard <pyr at spootnik.org>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/itoa.h"

#include "cpu.h"

#include <sys/sysinfo.h>
#include <kstat.h>

static kstat_ctl_t *kc;
static kstat_t **ksp;
static size_t ksp_size;
static size_t numcpu;
static double tick_hz = 100.0;

extern metric_family_t fams[];

static int cpu_kstat_scan(void)
{
    int status = 0;

    size_t n = 0;
    for (kstat_t *ksp_chain = kc->kc_chain; ksp_chain != NULL; ksp_chain = ksp_chain->ks_next) {
        if (strncmp(ksp_chain->ks_module, "cpu_stat", 8) == 0)
            n++;
    }

    if (n > ksp_size) {
        kstat_t **tmp = realloc(ksp, n * sizeof(*ksp));
        if (tmp == NULL) {
            PLUGIN_ERROR("realloc failed.");
            status = -1;
        } else {
            ksp = tmp;
            ksp_size = n;
        }
    }

    /* Solaris doesn't count linear.. *sigh* */
    numcpu = 0;
    for (kstat_t *ksp_chain = kc->kc_chain;
         (numcpu < ksp_size) && (ksp_chain != NULL);
         ksp_chain = ksp_chain->ks_next) {
        if (strncmp(ksp_chain->ks_module, "cpu_stat", 8) == 0)
            ksp[numcpu++] = ksp_chain;
    }

    return status;
}

int cpu_read(void)
{
    cdtime_t now = cdtime();

    static cpu_stat_t cs;

    if (kc == NULL)
        return -1;

    kid_t kid = kstat_chain_update(kc);
    if (kid < 0) {
        PLUGIN_ERROR("kstat_chain_update failed.");
        return -1;
    }

    /* The chain has changed: the cached kstat_t pointers may have been freed. */
    if (kid > 0)
        cpu_kstat_scan();

    uint64_t cpu_all_idle = 0;
    uint64_t cpu_all_user = 0;
    uint64_t cpu_all_system = 0;
    uint64_t cpu_all_wait = 0;

    for (size_t cpu = 0; cpu < numcpu; cpu++) {
        if (strncmp(ksp[cpu]->ks_module, "cpu_stat", 8) != 0)
            continue;

        if (kstat_read(kc, ksp[cpu], &cs) == -1)
            continue; /* error message? */

        char buffer_cpu[ITOA_MAX];
        itoa(ksp[cpu]->ks_instance, buffer_cpu);

        uint64_t cpu_idle   = cs.cpu_sysinfo.cpu[CPU_IDLE];
        uint64_t cpu_user   = cs.cpu_sysinfo.cpu[CPU_USER];
        uint64_t cpu_system = cs.cpu_sysinfo.cpu[CPU_KERNEL];
        uint64_t cpu_wait   = cs.cpu_sysinfo.cpu[CPU_WAIT];

        cpu_all_idle += cpu_idle;
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)cpu_idle/tick_hz), NULL,
                             &LABEL_PAIR_CONST("state", "idle"),
                             &LABEL_PAIR_CONST("cpu", buffer_cpu), NULL);
        cpu_all_user += cpu_user;
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)cpu_user/tick_hz), NULL,
                             &LABEL_PAIR_CONST("state", "user"),
                             &LABEL_PAIR_CONST("cpu", buffer_cpu), NULL);
        cpu_all_system += cpu_system;
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)cpu_system/tick_hz), NULL,
                             &LABEL_PAIR_CONST("state", "system"),
                             &LABEL_PAIR_CONST("cpu", buffer_cpu), NULL);
        cpu_all_wait += cpu_wait;
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)cpu_wait/tick_hz), NULL,
                             &LABEL_PAIR_CONST("state", "wait"),
                             &LABEL_PAIR_CONST("cpu", buffer_cpu), NULL);
    }

    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cpu_all_idle/tick_hz), NULL,
                         &LABEL_PAIR_CONST("state", "idle"), NULL);
    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cpu_all_user/tick_hz), NULL,
                         &LABEL_PAIR_CONST("state", "user"), NULL);
    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cpu_all_system/tick_hz), NULL,
                         &LABEL_PAIR_CONST("state", "system"), NULL);
    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cpu_all_wait/tick_hz), NULL,
                         &LABEL_PAIR_CONST("state", "wait"), NULL);

    metric_family_append(&fams[FAM_CPU_COUNT], VALUE_GAUGE(numcpu), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_CPU_MAX, now);

    return 0;
}

int cpu_init(void)
{
    numcpu = 0;

    long ticks = sysconf(_SC_CLK_TCK);
    if (ticks <= 0) {
        PLUGIN_ERROR("sysconf(_SC_CLK_TCK) failed: %s.", STRERRNO);
        return -1;
    }
    tick_hz = ticks;

    if (kc == NULL)
        kc = kstat_open();

    if (kc == NULL) {
        PLUGIN_ERROR("kstat_open failed.");
        return -1;
    }

    if (cpu_kstat_scan() != 0)
        return -1;

    return 0;
}

int cpu_shutdown(void)
{
    free(ksp);
    ksp = NULL;
    ksp_size = 0;
    numcpu = 0;

    if (kc != NULL) {
        kstat_close(kc);
        kc = NULL;
    }

    return 0;
}
