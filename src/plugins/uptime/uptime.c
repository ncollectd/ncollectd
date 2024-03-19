// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2009 Marco Chiappero
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Marco Chiappero <marco at absence.it>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#ifdef KERNEL_LINUX
#include <sys/sysinfo.h>
#elif defined(HAVE_LIBKSTAT)
/* Using kstats chain to retrieve the boot time on Solaris / OpenSolaris systems */
#elif defined(HAVE_SYS_SYSCTL_H)
#include <sys/sysctl.h>
/* Using sysctl interface to retrieve the boot time on *BSD / Darwin / OS X systems */
#elif defined(HAVE_PERFSTAT)
#include <libperfstat.h>
#include <sys/protosw.h>
/* Using perfstat_cpu_total to retrive the boot time in AIX */
#else
#error "No applicable input method."
#endif

#ifdef HAVE_KSTAT_H
#include <kstat.h>
static kstat_ctl_t *kc;
#endif


static metric_family_t fam = {
    .name = "system_uptime_seconds",
    .type = METRIC_TYPE_GAUGE,
    .help = "System uptime",
};

/*
 * On most unix systems the uptime is calculated by looking at the boot
 * time (stored in unix time, since epoch) and the current one. We are
 * going to do the same, reading the boot time value while executing
 * the uptime_init function (there is no need to read, every time the
 * plugin_read is called, a value that won't change). However, since
 * uptime_init is run only once, if the function fails in retrieving
 * the boot time, the plugin is unregistered and there is no chance to
 * try again later. Nevertheless, this is very unlikely to happen.
 */
static time_t uptime_get_sys(void)
{
    time_t result = 0;

#ifdef KERNEL_LINUX
    struct sysinfo info;

    int status = sysinfo(&info);
    if (unlikely(status != 0)) {
        PLUGIN_ERROR("Error calling sysinfo: %s", STRERRNO);
        return -1;
    }

    result = (time_t)info.uptime;

#elif defined(HAVE_LIBKSTAT)
    /* kstats chain already opened by update_kstat (using *kc), verify everything
     * went fine. */
    if (unlikely(kc == NULL)) {
        PLUGIN_ERROR("kstat chain control structure not available.");
        return -1;
    }

    if (kstat_chain_update(kc) < 0) {
        PLUGIN_ERROR("kstat_chain_update failed.");
        return -1;
    }

    kstat_t *ksp = kstat_lookup(kc, "unix", 0, "system_misc");
    if (unlikely(ksp == NULL)) {
        PLUGIN_ERROR("Cannot find unix:0:system_misc kstat.");
        return -1;
    }

    kid_t kcid = kstat_read(kc, ksp, NULL);
    if (unlikely(kcid < 0)) {
        PLUGIN_ERROR("kstat_read failed.");
        return -1;
    }

    kstat_named_t *knp = (kstat_named_t *)kstat_data_lookup(ksp, "boot_time");
    if (unlikely(knp == NULL)) {
        PLUGIN_ERROR("kstat_data_lookup (boot_time) failed.");
        return -1;
    }

    if (unlikely(knp->value.ui32 == 0)) {
        PLUGIN_ERROR("kstat_data_lookup returned success, but `boottime' is zero!");
        return -1;
    }

    result = time(NULL) - (time_t)knp->value.ui32;

#elif defined(HAVE_SYS_SYSCTL_H)
    int mib[] = {CTL_KERN, KERN_BOOTTIME};
    struct timeval boottv = {0};
    size_t boottv_len = sizeof(boottv);

    int status = sysctl(mib, STATIC_ARRAY_SIZE(mib), &boottv, &boottv_len,
                         /* new_value = */ NULL, /* new_length = */ 0);
    if (unlikely(status != 0)) {
        PLUGIN_ERROR("No value read from sysctl interface: %s", STRERRNO);
        return -1;
    }

    if (unlikely(boottv.tv_sec == 0)) {
        PLUGIN_ERROR("sysctl(3) returned success, but `boottime' is zero!");
        return -1;
    }

    result = time(NULL) - boottv.tv_sec;

#elif defined(HAVE_PERFSTAT)
    perfstat_cpu_total_t cputotal;

    int status = perfstat_cpu_total(NULL, &cputotal, sizeof(perfstat_cpu_total_t), 1);
    if (unlikely(status < 0)) {
        PLUGIN_ERROR("perfstat_cpu_total: %s", STRERRNO);
        return -1;
    }

    int hertz = sysconf(_SC_CLK_TCK);
    if (hertz <= 0)
        hertz = HZ;

    result = cputotal.lbolt / hertz;
#endif

    return result;
}

static int uptime_read(void)
{
    /* calculate the amount of time elapsed since boot, AKA uptime */
    time_t elapsed = uptime_get_sys();

    metric_family_append(&fam, VALUE_GAUGE(elapsed), NULL, NULL);

    plugin_dispatch_metric_family(&fam, 0);

    return 0;
}

void module_register(void)
{
    plugin_register_read("uptime", uptime_read);
}
