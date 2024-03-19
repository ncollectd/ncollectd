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

#include <libperfstat.h>
#include <sys/protosw.h>

#include "cpu.h"

static perfstat_cpu_t *perfcpu;
static int pnumcpu;

extern metric_family_t fams[];

int cpu_read(void)
{
    cdtime_t now = cdtime();

    perfstat_cpu_total_t cputotal = {0};
    if (!perfstat_cpu_total(NULL, &cputotal, sizeof(cputotal), 1)) {
        PLUGIN_WARNING("perfstat_cpu_total: %s", STRERRNO);
        return -1;
    }

    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cputotal.pidle/100.0), NULL,
                         &(label_pair_const_t){.name="state", "idle"}, NULL);
    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cputotal.puser/100.0), NULL,
                         &(label_pair_const_t){.name="state", "user"}, NULL);
    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cputotal.psys/100.0), NULL,
                         &(label_pair_const_t){.name="state", "system"}, NULL);
    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cputotal.pwait/100.0), NULL,
                         &(label_pair_const_t){.name="state", "wait"}, NULL);


    int numcpu = perfstat_cpu(NULL, NULL, sizeof(perfstat_cpu_t), 0);
    if (numcpu == -1) {
        PLUGIN_WARNING("perfstat_cpu: %s", STRERRNO);
        plugin_dispatch_metric_family_array(fams, FAM_CPU_MAX, 0);
        return -1;
    }

    if ((pnumcpu != numcpu) || (perfcpu == NULL)) {
        free(perfcpu);
        perfcpu = malloc(numcpu * sizeof(perfstat_cpu_t));
    }
    pnumcpu = numcpu;

    perfstat_id_t id = {0};
    int cpus = perfstat_cpu(&id, perfcpu, sizeof(perfstat_cpu_t), numcpu);
    if (cpus < 0) {
        PLUGIN_WARNING("perfstat_cpu: %s", STRERRNO);
        plugin_dispatch_metric_family_array(fams, FAM_CPU_MAX, 0);
        return -1;
    }

    for (int i = 0; i < cpus; i++) {
        char buffer_cpu[ITOA_MAX];
        itoa(i, buffer_cpu);

        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)perfcpu[i].user/100.0), NULL,
                             &(label_pair_const_t){.name="state", "user"},
                             &(label_pair_const_t){.name="cpu", buffer_cpu}, NULL);
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)perfcpu[i].sys/100.0), NULL,
                             &(label_pair_const_t){.name="state", "system"},
                             &(label_pair_const_t){.name="cpu", buffer_cpu}, NULL);
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)perfcpu[i].idle/100.0), NULL,
                             &(label_pair_const_t){.name="state", "idle"},
                             &(label_pair_const_t){.name="cpu", buffer_cpu}, NULL);
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)perfcpu[i].wait/100.0), NULL,
                             &(label_pair_const_t){.name="state", "wait"},
                             &(label_pair_const_t){.name="cpu", buffer_cpu}, NULL);
    }

    metric_family_append(&fams[FAM_CPU_COUNT], VALUE_GAUGE(cpus), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_CPU_MAX, now);

    return 0;
}
