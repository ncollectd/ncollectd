// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2020 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2009 Simon Kuhnle
// SPDX-FileCopyrightText: Copyright (C) 2009-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Simon Kuhnle <simon at blarzwurst.de>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include "memory.h"

#if defined(HAVE_SYS_SYSCTL_H) && (defined(HAVE_SYSCTLBYNAME) || defined(KERNEL_OPENBSD))
/* Implies BSD variant */
#    include <sys/sysctl.h>
#endif

#ifdef HAVE_SYS_VMMETER_H
#    include <sys/vmmeter.h>
#endif

static int pagesize;

#ifdef HAVE_SYSCTLBYNAME
#    if defined(HAVE_SYSCTL) && defined(KERNEL_NETBSD)
#        include <unistd.h> /* getpagesize() */
#    endif
#endif

#ifdef KERNEL_NETBSD
#    include <uvm/uvm_extern.h>
#endif

extern metric_family_t fams[];

int memory_read(void)
{
#if defined(HAVE_SYSCTL) && defined(KERNEL_NETBSD)
    if (pagesize == 0)
        return EINVAL;

    int mib[] = {CTL_VM, VM_UVMEXP2};
    struct uvmexp_sysctl uvmexp = {0};
    size_t size = sizeof(uvmexp);
    if (sysctl(mib, STATIC_ARRAY_SIZE(mib), &uvmexp, &size, NULL, 0) < 0) {
        PLUGIN_WARNING("sysctl failed: %s", STRERRNO);
        return errno;
    }

    int64_t accounted = uvmexp.wired + uvmexp.active + uvmexp.inactive + uvmexp.free;
    double mem_kernel = NAN;
    if (uvmexp.npages > accounted)
        mem_kernel = (double)((uvmexp.npages - accounted) * pagesize);

    metric_family_append(&fams[FAM_MEMORY_WIRED_BYTES],
                         VALUE_GAUGE(uvmexp.wired * pagesize), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_ACTIVE_BYTES],
                         VALUE_GAUGE(uvmexp.active * pagesize), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_INACTIVE_BYTES],
                         VALUE_GAUGE(uvmexp.inactive * pagesize), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_FREE_BYTES],
                         VALUE_GAUGE(uvmexp.free * pagesize), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_FREE_BYTES], VALUE_GAUGE(mem_kernel), NULL, NULL);

#elif defined(HAVE_SYSCTL) && defined(KERNEL_OPENBSD)
    /* OpenBSD variant does not have HAVE_SYSCTLBYNAME */
    if (pagesize == 0)
        return EINVAL;

    int mib[] = {CTL_VM, VM_METER};
    struct vmtotal vmtotal = {0};
    size_t size = sizeof(vmtotal);
    if (sysctl(mib, STATIC_ARRAY_SIZE(mib), &vmtotal, &size, NULL, 0) < 0) {
        PLUGIN_ERROR("sysctl failed: %s", STRERRNO);
        return errno;
    }

    metric_family_append(&fams[FAM_MEMORY_ACTIVE_BYTES],
                         VALUE_GAUGE(vmtotal.t_arm * pagesize), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_INACTIVE_BYTES],
                         VALUE_GAUGE((vmtotal.t_rm - vmtotal.t_arm) * pagesize), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_FREE_BYTES],
                         VALUE_GAUGE(vmtotal.t_free * pagesize), NULL, NULL);

#elif defined(HAVE_SYSCTLBYNAME)
    /*
     * vm.stats.vm.v_page_size: 4096
     * vm.stats.vm.v_page_count: 246178
     * vm.stats.vm.v_free_count: 28760
     * vm.stats.vm.v_wire_count: 37526
     * vm.stats.vm.v_active_count: 55239
     * vm.stats.vm.v_inactive_count: 113730
     * vm.stats.vm.v_cache_count: 10809
     */
    struct {
        char const *sysctl_key;
        int fam;
    } metrics[] = {
        {"vm.stats.vm.v_page_size"},
        {"vm.stats.vm.v_free_count",     FAM_MEMORY_FREE_BYTES    },
        {"vm.stats.vm.v_wire_count",     FAM_MEMORY_WIRED_BYTES   },
        {"vm.stats.vm.v_active_count",   FAM_MEMORY_ACTIVE_BYTES  },
        {"vm.stats.vm.v_inactive_count", FAM_MEMORY_INACTIVE_BYTES},
        {"vm.stats.vm.v_cache_count",    FAM_MEMORY_CACHED_BYTES  },
    };

    double page_size = 0;
    for (size_t i = 0; i < STATIC_ARRAY_SIZE(metrics); i++) {
        long value = 0;
        size_t value_len = sizeof(value);

        int status = sysctlbyname(metrics[i].sysctl_key, (void *)&value, &value_len, NULL, 0);
        if (status != 0) {
            PLUGIN_WARNING("sysctlbyname(\"%s\") failed: %s", metrics[i].sysctl_key, STRERROR(status));
            continue;
        }

        if (i == 0) {
            page_size = (double)value;
            continue;
        }

        metric_family_append(&fams[metrics[i].fam], VALUE_GAUGE(value * page_size), NULL, NULL);
    }
#else
#error "No applicable input method."
#endif

    plugin_dispatch_metric_family_array(fams, FAM_MEMORY_MAX, 0);

    return 0;
}

int memory_init(void)
{
    pagesize = getpagesize();
    if (pagesize <= 0) {
        PLUGIN_ERROR("Invalid pagesize: %i", pagesize);
        return -1;
    }
    return 0;
}
