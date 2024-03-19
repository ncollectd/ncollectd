// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2008 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileCopyrightText: Copyright (C) 2013 Vedran Bartonicek
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>
// SPDX-FileContributor: Vedran Bartonicek <vbartoni at gmail.com>

#define _DEFAULT_SOURCE
#define _BSD_SOURCE

#include "plugin.h"
#include "libutils/common.h"

#include <unistd.h>

#ifdef HAVE_SYS_LOADAVG_H
#include <sys/loadavg.h>
#endif

#if defined(KERNEL_LINUX)
static char *path_proc_loadavg;

#elif defined(HAVE_GETLOADAVG)
#if !defined(LOADAVG_1MIN) || !defined(LOADAVG_5MIN) || !defined(LOADAVG_15MIN)
#define LOADAVG_1MIN 0
#define LOADAVG_5MIN 1
#define LOADAVG_15MIN 2
#endif
#endif

#ifdef HAVE_PERFSTAT
#include <libperfstat.h>
#include <sys/proc.h> /* AIX 5 */
#include <sys/protosw.h>
#endif

enum {
    FAM_LOAD_1MIN,
    FAM_LOAD_5MIN,
    FAM_LOAD_15MIN,
    FAM_LOAD_MAX
};

static metric_family_t fams[FAM_LOAD_MAX] = {
    [FAM_LOAD_1MIN] = {
        .name = "system_load_1m",
        .type = METRIC_TYPE_GAUGE,
        .help = "System load average for the past 1 minute.",
    },
    [FAM_LOAD_5MIN] = {
        .name = "system_load_5m",
        .type = METRIC_TYPE_GAUGE,
        .help = "System load average for the past 5 minutes.",
    },
    [FAM_LOAD_15MIN] = {
        .name = "system_load_15m",
        .type = METRIC_TYPE_GAUGE,
        .help = "System load average for the past 15 minutes.",
    },
};

static int load_read(void)
{
    double snum = 0;
    double mnum = 0;
    double lnum = 0;

#if defined(KERNEL_LINUX)
    char buffer[64];
    ssize_t size = read_file(path_proc_loadavg, buffer, sizeof(buffer));
    if (unlikely(size < 0)) {
        PLUGIN_ERROR("read %s : %s", path_proc_loadavg, STRERRNO);
        return -1;
    }

    char *fields[3];
    int numfields = strsplit(buffer, fields, 3);
    if (unlikely(numfields < 3))
        return -1;

    snum = atof(fields[0]);
    mnum = atof(fields[1]);
    lnum = atof(fields[2]);

#elif defined(HAVE_GETLOADAVG)
    double load[3];
    int num = getloadavg(load, 3);
    if (unlikely(num != 3)) {
        PLUGIN_WARNING("getloadavg failed: %s", STRERRNO);
        return -1;
    }

    snum = load[LOADAVG_1MIN];
    mnum = load[LOADAVG_5MIN];
    lnum = load[LOADAVG_15MIN];

#elif defined(HAVE_PERFSTAT)
    perfstat_cpu_total_t cputotal;

    int status = perfstat_cpu_total(NULL, &cputotal, sizeof(perfstat_cpu_total_t), 1);
    if (status < 0) {
        PLUGIN_WARNING("perfstat_cpu : %s", STRERRNO);
        return -1;
    }

    snum = (double)cputotal.loadavg[0] / (double)(1 << SBITS);
    mnum = (double)cputotal.loadavg[1] / (double)(1 << SBITS);
    lnum = (double)cputotal.loadavg[2] / (double)(1 << SBITS);

#else
#error "No applicable input method."
#endif

    metric_family_append(&fams[FAM_LOAD_1MIN], VALUE_GAUGE(snum), NULL, NULL);
    metric_family_append(&fams[FAM_LOAD_5MIN], VALUE_GAUGE(mnum), NULL, NULL);
    metric_family_append(&fams[FAM_LOAD_15MIN], VALUE_GAUGE(lnum), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_LOAD_MAX, 0);

    return 0;
}

#ifdef KERNEL_LINUX
static int load_init(void)
{
    path_proc_loadavg = plugin_procpath("loadavg");
    if (path_proc_loadavg == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int load_shutdown(void)
{
    free(path_proc_loadavg);
    return 0;
}
#endif

void module_register(void)
{
#ifdef KERNEL_LINUX
    plugin_register_init("load", load_init);
    plugin_register_shutdown("load", load_shutdown);
#endif
    plugin_register_read("load", load_read);
}
