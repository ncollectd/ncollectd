// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2016 Pavel Rochnyak
// SPDX-FileContributor: Pavel Rochnyak <pavel2000 ngs.ru>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/itoa.h"

#include <xenctrl.h>

#ifdef XENCTRL_HAS_XC_INTERFACE
// Xen-4.1+
#define XC_INTERFACE_INIT_ARGS NULL, NULL, 0
static xc_interface *xc_handle;
#else
// For xen-3.4/xen-4.0
#include <string.h>
#define xc_strerror(xc_interface, errcode) strerror(errcode)
#define XC_INTERFACE_INIT_ARGS
typedef int xc_interface;
static xc_interface xc_handle = 0;
#endif

static uint32_t num_cpus = 0;
static xc_cpuinfo_t *cpu_info;

static metric_family_t fam_xen_cpu_idle_time = {
    .name = "xen_cpu_idle_time",
    .type = METRIC_TYPE_GAUGE,
    .help = NULL,
};

static int xencpu_read(void)
{
    cdtime_t now = cdtime();

    int nr_cpus;

    int rc = xc_getcpuinfo(xc_handle, num_cpus, cpu_info, &nr_cpus);
    if (rc < 0) {
        PLUGIN_ERROR("xc_getcpuinfo() Failed: %d %s\n", rc, xc_strerror(xc_handle, errno));
        return -1;
    }

    for (int cpu = 0; cpu < nr_cpus; cpu++) {
        char ncpu[ITOA_MAX];
        uitoa(cpu, ncpu);
        metric_family_append(&fam_xen_cpu_idle_time, VALUE_COUNTER(cpu_info[cpu].idletime), NULL,
                             &(label_pair_const_t){.name="cpu", .value=ncpu}, NULL);
    }

    plugin_dispatch_metric_family(&fam_xen_cpu_idle_time, now);

    return 0;
}

static int xencpu_shutdown(void)
{
    free(cpu_info);
    xc_interface_close(xc_handle);
    return 0;
}

static int xencpu_init(void)
{
    xc_handle = xc_interface_open(XC_INTERFACE_INIT_ARGS);
    if (!xc_handle) {
        PLUGIN_ERROR("xc_interface_open() failed");
        return -1;
    }

    xc_physinfo_t *physinfo = calloc(1, sizeof(*physinfo));
    if (physinfo == NULL) {
        PLUGIN_ERROR("calloc() for physinfo failed.");
        xc_interface_close(xc_handle);
        return ENOMEM;
    }

    if (xc_physinfo(xc_handle, physinfo) < 0) {
        PLUGIN_ERROR("xc_physinfo() failed");
        xc_interface_close(xc_handle);
        free(physinfo);
        return -1;
    }

    num_cpus = physinfo->nr_cpus;
    free(physinfo);

    PLUGIN_INFO("Found %" PRIu32 " processors.", num_cpus);

    cpu_info = calloc(num_cpus, sizeof(*cpu_info));
    if (cpu_info == NULL) {
        PLUGIN_ERROR("calloc() for num_cpus failed.");
        xc_interface_close(xc_handle);
        return ENOMEM;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_init("xencpu", xencpu_init);
    plugin_register_read("xencpu", xencpu_read);
    plugin_register_shutdown("xencpu", xencpu_shutdown);
}
