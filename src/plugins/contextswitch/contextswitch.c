// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2009 Patrik Weiskircher
// SPDX-FileCopyrightText: Copyright (C) 2010 Kimo Rosenbaum
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Patrik Weiskircher <weiskircher at inqnet.at>
// SPDX-FileContributor: Kimo Rosenbaum <http://github.com/kimor79>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#if defined(HAVE_SYSCTLBYNAME) && defined(HAVE_SYS_SYSCTL_H)
#include <sys/sysctl.h>
/* no global variables */
/* #endif HAVE_SYSCTLBYNAME */

#elif defined(KERNEL_LINUX)
static char *path_proc_stat;
/* #endif KERNEL_LINUX */

#elif defined(HAVE_PERFSTAT)
#include <libperfstat.h>
#include <sys/protosw.h>
/* #endif HAVE_PERFSTAT */

#else
#error "No applicable input method."
#endif

static  metric_family_t fam = {
    .name = "system_context_switches",
    .type = METRIC_TYPE_COUNTER,
    .help = "Total number of context switches across all CPUs."
};

static int cs_read(void)
{
    uint64_t context_switches = 0;

#ifdef HAVE_SYSCTLBYNAME
    int value = 0;
    size_t value_len = sizeof(value);
    int status = sysctlbyname("vm.stats.sys.v_swtch", &value, &value_len,
                          /* new pointer = */ NULL, /* new length = */ 0);
    if (status != 0) {
        PLUGIN_ERROR("sysctlbyname (vm.stats.sys.v_swtch) failed");
        return -1;
    }

    context_switches = (uint64_t)value;

#elif defined(KERNEL_LINUX)
    FILE *fh = fopen(path_proc_stat, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("unable to open '%s': %s", path_proc_stat, STRERRNO);
        return -1;
    }

    char buffer[64];
    char *fields[3];
    int status = -2;
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        int numfields = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (numfields != 2)
            continue;

        if (strcmp("ctxt", fields[0]) != 0)
            continue;

        errno = 0;
        char *endptr = NULL;
        context_switches = (uint64_t)strtoll(fields[1], &endptr, /* base = */ 10);
        if ((endptr == fields[1]) || (errno != 0)) {
            PLUGIN_ERROR("Cannot parse ctxt value: %s", fields[1]);
            status = -1;
            break;
        }

        status = 0;
        break;
    }
    fclose(fh);

    if (status == -2) {
        PLUGIN_ERROR("Unable to find context switch value.");
        return status;
    }

#elif defined(HAVE_PERFSTAT)
    perfstat_cpu_total_t perfcputotal;
    int status = perfstat_cpu_total(NULL, &perfcputotal, sizeof(perfstat_cpu_total_t), 1);
    if (status < 0) {
        PLUGIN_ERROR("perfstat_cpu_total: %s", STRERRNO);
        return -1;
    }

    context_switches = (uint64_t)perfcputotal.pswitch;
    status = 0;
#endif

    metric_family_append(&fam, VALUE_COUNTER(context_switches), NULL, NULL);
    plugin_dispatch_metric_family(&fam, 0);

    return status;
}

#ifdef KERNEL_LINUX
static int cs_init(void)
{
    path_proc_stat = plugin_procpath("stat");
    if (path_proc_stat == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int cs_shutdown(void)
{
    free(path_proc_stat);
    return 0;
}
#endif

void module_register(void)
{
#ifdef KERNEL_LINUX
    plugin_register_init("contextswitch", cs_init);
    plugin_register_shutdown("contextswitch", cs_shutdown);
#endif
    plugin_register_read("contextswitch", cs_read);
}
