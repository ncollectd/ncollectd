// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2014 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2008 Oleg King
// SPDX-FileCopyrightText: Copyright (C) 2009 Simon Kuhnle
// SPDX-FileCopyrightText: Copyright (C) 2009 Manuel Sanmartin
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

#if defined(HAVE_SYSCTL) && (defined(HAVE_SYSCTLBYNAME) || defined(__OpenBSD__))
/* Implies BSD variant */
#    include <sys/sysctl.h>
#endif

#ifdef KERNEL_OPENBSD
#include <sys/sched.h>
#endif

#ifdef KERNEL_DRAGONFLY
#include <sys/user.h>
#endif

#ifdef HAVE_SYS_DKSTAT_H
/* implies BSD variant */
#include <sys/dkstat.h>

#   if !defined(CP_USER) || !defined(CP_NICE) || !defined(CP_SYS) ||  \
       !defined(CP_INTR) || !defined(CP_IDLE) || !defined(CPUSTATES)
#       define CP_USER   0
#       define CP_NICE   1
#       define CP_SYS    2
#       define CP_INTR   3
#       define CP_IDLE   4
#       define CPUSTATES 5
#    endif
#endif

#if defined(HAVE_SYSCTL) && (defined(HAVE_SYSCTLBYNAME) || defined(__OpenBSD__))

/* Implies BSD variant */
#    if defined(CTL_HW) && defined(HW_NCPU) && defined(CTL_KERN) && \
        (defined(KERN_CPTIME) || defined(KERN_CP_TIME) || defined(KERN_CPTIME2)) && \
        defined(CPUSTATES)
#        define CAN_USE_SYSCTL 1
#    else
#        define CAN_USE_SYSCTL 0
#    endif
#else
#    define CAN_USE_SYSCTL 0
#endif

#if !CAN_USE_SYSCTL && !defined(HAVE_SYSCTLBYNAME)
#    error "No applicable input method."
#endif

extern metric_family_t fams[];

#if CAN_USE_SYSCTL
/* Only possible for (Open) BSD variant */
static size_t numcpu;
#elif defined(HAVE_SYSCTLBYNAME)
/* Implies BSD variant */
static int numcpu;
#ifdef HAVE_SYSCTL_KERN_CP_TIMES
static int maxcpu;
#endif

#endif

int cpu_init(void)
{
#if CAN_USE_SYSCTL
    /* Only on (Open) BSD variant */
    size_t numcpu_size;
    int mib[2] = {CTL_HW, HW_NCPU};
    int status;

    numcpu = 0;
    numcpu_size = sizeof(numcpu);

    status = sysctl(mib, STATIC_ARRAY_SIZE(mib), &numcpu, &numcpu_size, NULL, 0);
    if (status == -1) {
        PLUGIN_WARNING("sysctl: %s", STRERRNO);
        return -1;
    }
    /* #endif CAN_USE_SYSCTL */

#elif defined(HAVE_SYSCTLBYNAME)
    /* Only on BSD variant */
    size_t numcpu_size = sizeof(numcpu);

    if (sysctlbyname("hw.ncpu", &numcpu, &numcpu_size, NULL, 0) < 0) {
        PLUGIN_WARNING("sysctlbyname(hw.ncpu): %s", STRERRNO);
        return -1;
    }

#ifdef HAVE_SYSCTL_KERN_CP_TIMES
    numcpu_size = sizeof(maxcpu);

    if (sysctlbyname("kern.smp.maxcpus", &maxcpu, &numcpu_size, NULL, 0) < 0) {
        PLUGIN_WARNING("sysctlbyname(kern.smp.maxcpus): %s", STRERRNO);
        return -1;
    }
#else
    if (numcpu != 1)
        PLUGIN_NOTICE("Only one processor supported when using 'sysctlbyname' (found %i)", numcpu);
#endif

#endif
    return 0;
}


int cpu_read(void)
{
    cdtime_t now = cdtime();

#if CAN_USE_SYSCTL
    /* Only on (Open) BSD variant */
    uint64_t cpuinfo[numcpu][CPUSTATES];
    size_t cpuinfo_size;
    int status;

    if (numcpu < 1) {
        PLUGIN_ERROR("Could not determine number of installed CPUs using sysctl(3).");
        return -1;
    }

    memset(cpuinfo, 0, sizeof(cpuinfo));

#if defined(KERN_CP_TIME) && defined(KERNEL_NETBSD)
    {
        int mib[] = {CTL_KERN, KERN_CP_TIME};

        cpuinfo_size = sizeof(cpuinfo[0]) * numcpu * CPUSTATES;
        status = sysctl(mib, 2, cpuinfo, &cpuinfo_size, NULL, 0);
        if (status == -1) {
            char errbuf[1024];

            PLUGIN_ERROR("sysctl failed: %s.", sstrerror(errno, errbuf, sizeof(errbuf)));
            return -1;
        }
        if (cpuinfo_size == (sizeof(cpuinfo[0]) * CPUSTATES)) {
            numcpu = 1;
        }
    }
#else
#if defined(KERN_CPTIME2)
    if (numcpu > 1) {
        for (size_t i = 0; i < numcpu; i++) {
            int mib[] = {CTL_KERN, KERN_CPTIME2, i};

            cpuinfo_size = sizeof(cpuinfo[0]);

            status = sysctl(mib, STATIC_ARRAY_SIZE(mib), cpuinfo[i], &cpuinfo_size, NULL, 0);
            if (status == -1) {
                PLUGIN_ERROR("sysctl failed: %s.", STRERRNO);
                return -1;
            }
        }
    } else
#endif
    {
        int mib[] = {CTL_KERN, KERN_CPTIME};
        long cpuinfo_tmp[CPUSTATES];

        cpuinfo_size = sizeof(cpuinfo_tmp);

        status = sysctl(mib, STATIC_ARRAY_SIZE(mib), &cpuinfo_tmp, &cpuinfo_size, NULL, 0);
        if (status == -1) {
            PLUGIN_ERROR("sysctl failed: %s.", STRERRNO);
            return -1;
        }

        for (int i = 0; i < CPUSTATES; i++) {
            cpuinfo[0][i] = cpuinfo_tmp[i];
        }
    }
#endif
    uint64_t cpu_all_user = 0;
    uint64_t cpu_all_nice = 0;
    uint64_t cpu_all_system = 0;
    uint64_t cpu_all_idle = 0;
    uint64_t cpu_all_interrupt = 0;

    for (size_t i = 0; i < numcpu; i++) {
        char buffer_cpu[ITOA_MAX];
        itoa(i, buffer_cpu);

        cpu_all_user += cpuinfo[i][CP_USER];
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)cpuinfo[i][CP_USER]/100.0), NULL,
                             &(label_pair_const_t){.name="state", "usage"},
                             &(label_pair_const_t){.name="cpu", buffer_cpu}, NULL);
        cpu_all_nice += cpuinfo[i][CP_NICE];
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)cpuinfo[i][CP_NICE]/100.0), NULL,
                             &(label_pair_const_t){.name="state", "nice"},
                             &(label_pair_const_t){.name="cpu", buffer_cpu}, NULL);
        cpu_all_system += cpuinfo[i][CP_SYS];
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)cpuinfo[i][CP_SYS]/100.0), NULL,
                             &(label_pair_const_t){.name="state", "sys"},
                             &(label_pair_const_t){.name="cpu", buffer_cpu}, NULL);
        cpu_all_idle += cpuinfo[i][CP_IDLE];
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)cpuinfo[i][CP_IDLE]/100.0), NULL,
                             &(label_pair_const_t){.name="state", "idle"},
                             &(label_pair_const_t){.name="cpu", buffer_cpu}, NULL);
        cpu_all_interrupt += cpuinfo[i][CP_INTR];
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)cpuinfo[i][CP_INTR]/100.0), NULL,
                             &(label_pair_const_t){.name="state", "interrupt"},
                             &(label_pair_const_t){.name="cpu", buffer_cpu}, NULL);
    }

    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cpu_all_user/100.0), NULL,
                         &(label_pair_const_t){.name="state", "usage"}, NULL);
    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cpu_all_nice/100.0), NULL,
                         &(label_pair_const_t){.name="state", "nice"}, NULL);
    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cpu_all_system/100.0), NULL,
                         &(label_pair_const_t){.name="state", "sys"}, NULL);
    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cpu_all_idle/100.0), NULL,
                         &(label_pair_const_t){.name="state", "idle"}, NULL);
    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cpu_all_interrupt/100.0), NULL,
                         &(label_pair_const_t){.name="state", "interrupt"}, NULL);

#elif defined(HAVE_SYSCTLBYNAME) && defined(HAVE_SYSCTL_KERN_CP_TIMES)

    /* Only on BSD variant */
    long cpuinfo[maxcpu][CPUSTATES];
    size_t cpuinfo_size;

    memset(cpuinfo, 0, sizeof(cpuinfo));

    cpuinfo_size = sizeof(cpuinfo);
    if (sysctlbyname("kern.cp_times", &cpuinfo, &cpuinfo_size, NULL, 0) < 0) {
        PLUGIN_ERROR("sysctlbyname failed: %s.", STRERRNO);
        return -1;
    }

    uint64_t cpu_all_user = 0;
    uint64_t cpu_all_nice = 0;
    uint64_t cpu_all_system = 0;
    uint64_t cpu_all_idle = 0;
    uint64_t cpu_all_interrupt = 0;

    for (int i = 0; i < numcpu; i++) {
        char buffer_cpu[ITOA_MAX];
        itoa(i, buffer_cpu);

        cpu_all_user += cpuinfo[i][CP_USER];
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)cpuinfo[i][CP_USER]/100.0), NULL,
                             &(label_pair_const_t){.name="state", "usage"},
                             &(label_pair_const_t){.name="cpu", buffer_cpu}, NULL);
        cpu_all_nice += cpuinfo[i][CP_NICE];
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)cpuinfo[i][CP_NICE]/100.0), NULL,
                             &(label_pair_const_t){.name="state", "nice"},
                             &(label_pair_const_t){.name="cpu", buffer_cpu}, NULL);
        cpu_all_system += cpuinfo[i][CP_SYS];
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)cpuinfo[i][CP_SYS]/100.0), NULL,
                             &(label_pair_const_t){.name="state", "sys"},
                             &(label_pair_const_t){.name="cpu", buffer_cpu}, NULL);
        cpu_all_idle += cpuinfo[i][CP_IDLE];
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)cpuinfo[i][CP_IDLE]/100.0), NULL,
                             &(label_pair_const_t){.name="state", "idle"},
                             &(label_pair_const_t){.name="cpu", buffer_cpu}, NULL);
        cpu_all_interrupt += cpuinfo[i][CP_INTR];
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)cpuinfo[i][CP_INTR]/100.0), NULL,
                             &(label_pair_const_t){.name="state", "interrupt"},
                             &(label_pair_const_t){.name="cpu", buffer_cpu}, NULL);
    }

    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cpu_all_user/100.0), NULL,
                         &(label_pair_const_t){.name="state", "usage"}, NULL);
    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cpu_all_nice/100.0), NULL,
                         &(label_pair_const_t){.name="state", "nice"}, NULL);
    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cpu_all_system/100.0), NULL,
                         &(label_pair_const_t){.name="state", "sys"}, NULL);
    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cpu_all_idle/100.0), NULL,
                         &(label_pair_const_t){.name="state", "idle"}, NULL);
    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cpu_all_interrupt/100.0), NULL,
                         &(label_pair_const_t){.name="state", "interrupt"}, NULL);

#elif defined(HAVE_SYSCTLBYNAME)
    /* Only on BSD variant */
    long cpuinfo[CPUSTATES];
    size_t cpuinfo_size;

    cpuinfo_size = sizeof(cpuinfo);

    if (sysctlbyname("kern.cp_time", &cpuinfo, &cpuinfo_size, NULL, 0) < 0) {
        PLUGIN_ERROR("sysctlbyname failed: %s.", STRERRNO);
        return -1;
    }

    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cpuinfo[CP_USER]/100.0), NULL,
                         &(label_pair_const_t){.name="state", "usage"}, NULL);
    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cpuinfo[CP_NICE]/100.0), NULL,
                         &(label_pair_const_t){.name="state", "nice"}, NULL);
    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cpuinfo[CP_SYS]/100.0), NULL,
                         &(label_pair_const_t){.name="state", "sys"}, NULL);
    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cpuinfo[CP_IDLE]/100.0), NULL,
                         &(label_pair_const_t){.name="state", "idle"}, NULL);
    metric_family_append(&fams[FAM_CPU_ALL_USAGE],
                         VALUE_COUNTER_FLOAT64((double)cpuinfo[CP_INTR]/100.0), NULL,
                         &(label_pair_const_t){.name="state", "interrupt"}, NULL;

#endif

    metric_family_append(&fams[FAM_CPU_COUNT], VALUE_GAUGE(numcpu), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_CPU_MAX, now);

    return 0;
}
