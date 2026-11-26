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
#include "libutils/kstat.h"
/* Using kstats chain to retrieve the boot time on Solaris / OpenSolaris systems */
#elif defined(HAVE_SYS_SYSCTL_H)
#include <sys/sysctl.h>
/* Using sysctl interface to retrieve the boot time on *BSD / Darwin / OS X systems */
#elif defined(HAVE_PERFSTAT)
#include <libperfstat.h>
#include <sys/protosw.h>
/* Using perfstat_cpu_total to retrieve the boot time in AIX */
#else
#error "No applicable input method."
#endif

#ifdef HAVE_KSTAT_H
static kstat_ctl_t *kc;
static kstat_t *ksp;
#endif


static metric_family_t fam = {
    .name = "system_uptime_seconds",
    .type = METRIC_TYPE_GAUGE,
    .help = "System uptime",
};

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
    long long boot_time = get_kstat_value(ksp, "boot_time");
    if (unlikely(boot_time < 0)) {
        PLUGIN_ERROR("kstat_data_lookup (boot_time) failed.");
        return -1;
    }
    if (unlikely(boot_time == 0)) {
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
    if (elapsed < 0)
        return -1;

    metric_family_append(&fam, VALUE_GAUGE(elapsed), NULL, NULL);

    plugin_dispatch_metric_family(&fam, 0);

    return 0;
}

static int uptime_init(void)
{
#if defined(HAVE_LIBKSTAT)
    if (kc == NULL)
        kc = kstat_open();

    if (kc == NULL) {
        PLUGIN_ERROR("kstat_open failed.");
        return -1;
    }

    if (get_kstat(kc, &ksp, "unix", 0, "system_misc") != 0) {
        ksp = NULL;
        return -1;
    }
#endif
    return 0;
}

static int uptime_shutdown(void)
{
#if defined(HAVE_LIBKSTAT)
    if (kc != NULL)
        kstat_close(kc);
#endif
    return 0;
}

void module_register(void)
{
    plugin_register_init("uptime", uptime_init);
    plugin_register_shutdown("uptime", uptime_shutdown);
    plugin_register_read("uptime", uptime_read);
}
