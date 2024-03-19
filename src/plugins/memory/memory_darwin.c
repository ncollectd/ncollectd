// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2020 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2009 Simon Kuhnle
// SPDX-FileCopyrightText: Copyright (C) 2009-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Simon Kuhnle <simon at blarzwurst.de>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#ifdef HAVE_MACH_KERN_RETURN_H
#include <mach/kern_return.h>
#endif
#ifdef HAVE_MACH_MACH_INIT_H
#include <mach/mach_init.h>
#endif
#ifdef HAVE_MACH_MACH_HOST_H
#include <mach/mach_host.h>
#endif
#ifdef HAVE_MACH_HOST_PRIV_H
#include <mach/host_priv.h>
#endif
#ifdef HAVE_MACH_VM_STATISTICS_H
#include <mach/vm_statistics.h>
#endif

static mach_port_t port_host;
static vm_size_t pagesize;

#include "memory.h"

extern metric_family_t fams[];

int memory_read(void)
{
    if (!port_host || !pagesize)
        return EINVAL;

    vm_statistics_data_t vm_data = {0};
    mach_msg_type_number_t vm_data_len = sizeof(vm_data) / sizeof(natural_t);
    kern_return_t status = host_statistics(port_host, HOST_VM_INFO,
                                           (host_info_t)&vm_data, &vm_data_len);
    if (status != KERN_SUCCESS) {
        PLUGIN_ERROR("host_statistics failed and returned the value %d", (int)status);
        return (int)status;
    }

    /*
     * From <http://docs.info.apple.com/article.html?artnum=107918>:
     *
     * Wired memory
     *   This information can't be cached to disk, so it must stay in RAM.
     *   The amount depends on what applications you are using.
     *
     * Active memory
     *   This information is currently in RAM and actively being used.
     *
     * Inactive memory
     *   This information is no longer being used and has been cached to
     *   disk, but it will remain in RAM until another application needs
     *   the space. Leaving this information in RAM is to your advantage if
     *   you (or a client of your computer) come back to it later.
     *
     * Free memory
     *   This memory is not being used.
     */

    metric_family_append(&fams[FAM_MEMORY_WIRED_BYTES],
                         VALUE_GAUGE(vm_data.wire_count * pagesize), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_ACTIVE_BYTES],
                         VALUE_GAUGE(vm_data.active_count * pagesize), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_INACTIVE_BYTES],
                         VALUE_GAUGE(vm_data.inactive_count * pagesize), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_FREE_BYTES],
                         VALUE_GAUGE(vm_data.free_count * pagesize), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_MEMORY_MAX, 0);

    return 0;
}

int memory_init(void)
{
    port_host = mach_host_self();
    host_page_size(port_host, &pagesize);
    return 0;
}
