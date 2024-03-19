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

/* colleague tells me that Sun doesn't sell systems with more than 100 or so CPUs.. */
#define MAX_NUMCPU 256
static kstat_ctl_t *kc;
static kstat_t *ksp[MAX_NUMCPU];
static size_t numcpu;

extern metric_family_t fams[];

int cpu_read(void)
{
    cdtime_t now = cdtime();

    static cpu_stat_t cs;

    if (kc == NULL)
        return -1;

    if (kstat_chain_update(kc) < 0) {
        PLUGIN_ERROR("kstat_chain_update failed.");
        return -1;
    }

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
                             VALUE_COUNTER_FLOAT64((double)cpu_idle/100.0), NULL,
                             &(label_pair_const_t){.name="state", "idle"},
                             &(label_pair_const_t){.name="cpu", buffer_cpu}, NULL);
        cpu_all_user += cpu_user;
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)cpu_user/100.0), NULL,
                             &(label_pair_const_t){.name="state", "user"},
                             &(label_pair_const_t){.name="cpu", buffer_cpu}, NULL);
        cpu_all_system += cpu_system;
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)cpu_system/100.0), NULL,
                             &(label_pair_const_t){.name="state", "system"},
                             &(label_pair_const_t){.name="cpu", buffer_cpu}, NULL);
        cpu_all_wait += cpu_wait;
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)cpu_wait/100.0), NULL,
                             &(label_pair_const_t){.name="state", "wait"},
                             &(label_pair_const_t){.name="cpu", buffer_cpu}, NULL);
    }

    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cpu_all_idle/100.0), NULL,
                         &(label_pair_const_t){.name="state", "idle"}, NULL);
    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cpu_all_user/100.0), NULL,
                         &(label_pair_const_t){.name="state", "user"}, NULL);
    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cpu_all_system/100.0), NULL,
                         &(label_pair_const_t){.name="state", "system"}, NULL);
    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cpu_all_wait/100.0), NULL,
                         &(label_pair_const_t){.name="state", "wait"}, NULL);

    metric_family_append(&fams[FAM_CPU_COUNT], VALUE_GAUGE(numcpu), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_CPU_MAX, now);

    return 0;
}

int cpu_init(void)
{
    kstat_t *ksp_chain;

    numcpu = 0;

    if (kc == NULL)
        kc = kstat_open();

    if (kc == NULL) {
        PLUGIN_ERROR("kstat_open failed.");
        return -1;
    }

    /* Solaris doesn't count linear.. *sigh* */
    for (numcpu = 0, ksp_chain = kc->kc_chain;
         (numcpu < MAX_NUMCPU) && (ksp_chain != NULL);
         ksp_chain = ksp_chain->ks_next) {
        if (strncmp(ksp_chain->ks_module, "cpu_stat", 8) == 0)
            ksp[numcpu++] = ksp_chain;
    }

    return 0;
}
