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

#ifdef HAVE_MACH_KERN_RETURN_H
#    include <mach/kern_return.h>
#endif
#ifdef HAVE_MACH_MACH_INIT_H
#    include <mach/mach_init.h>
#endif
#ifdef HAVE_MACH_HOST_PRIV_H
#    include <mach/host_priv.h>
#endif
#ifdef HAVE_MACH_MACH_ERROR_H
#    include <mach/mach_error.h>
#endif
#ifdef HAVE_MACH_PROCESSOR_INFO_H
#    include <mach/processor_info.h>
#endif
#ifdef HAVE_MACH_PROCESSOR_H
#    include <mach/processor.h>
#endif
#ifdef HAVE_MACH_VM_MAP_H
#    include <mach/vm_map.h>
#endif

#include "cpu.h"

static mach_port_t port_host;
static processor_port_array_t cpu_list;
static mach_msg_type_number_t cpu_list_len;

extern metric_family_t fams[];

int cpu_init(void)
{

    port_host = mach_host_self();

    kern_return_t status = host_processors(port_host, &cpu_list, &cpu_list_len);
    if (status == KERN_INVALID_ARGUMENT) {
        PLUGIN_ERROR("Don't have a privileged host control port. "
                     "The most common cause for this problem is "
                     "that ncollectd is running without root "
                     "privileges, which are required to read CPU "
                     "load information.");
        cpu_list_len = 0;
        return -1;
    }
    if (status != KERN_SUCCESS) {
        PLUGIN_ERROR("host_processors() failed with status %d.", (int)status);
        cpu_list_len = 0;
        return -1;
    }

    PLUGIN_INFO("Found %i processor%s.", (int)cpu_list_len, cpu_list_len == 1 ? "" : "s");

    return 0;
}

int cpu_read(void)
{
    processor_cpu_load_info_data_t cpu_info;
    mach_msg_type_number_t cpu_info_len;

    host_t cpu_host;

    uint64_t cpu_all_user = 0;
    uint64_t cpu_all_nice = 0;
    uint64_t cpu_all_system = 0;
    uint64_t cpu_all_idle = 0;

    cdtime_t now = cdtime();

    for (mach_msg_type_number_t cpu = 0; cpu < cpu_list_len; cpu++) {
        cpu_host = 0;
        cpu_info_len = PROCESSOR_BASIC_INFO_COUNT;

        int status = processor_info(cpu_list[cpu], PROCESSOR_CPU_LOAD_INFO, &cpu_host,
                                    (processor_info_t)&cpu_info, &cpu_info_len);
        if (status != KERN_SUCCESS) {
            PLUGIN_ERROR("processor_info (PROCESSOR_CPU_LOAD_INFO) failed: %s",
                         mach_error_string(status));
            continue;
        }

        if (cpu_info_len < CPU_STATE_MAX) {
            PLUGIN_ERROR("processor_info returned only %i elements..", cpu_info_len);
            continue;
        }

        char buffer_cpu[ITOA_MAX];
        itoa(cpu, buffer_cpu);

        cpu_all_user += cpu_info.cpu_ticks[CPU_STATE_USER];
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)cpu_info.cpu_ticks[CPU_STATE_USER]/100.0),
                             NULL,
                             &(label_pair_const_t){.name="state", "usage"},
                             &(label_pair_const_t){.name="cpu", buffer_cpu}, NULL);
        cpu_all_nice += cpu_info.cpu_ticks[CPU_STATE_NICE];
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)cpu_info.cpu_ticks[CPU_STATE_NICE]/100.0),
                             NULL,
                             &(label_pair_const_t){.name="state", "nice"},
                             &(label_pair_const_t){.name="cpu", buffer_cpu}, NULL);
        cpu_all_system += cpu_info.cpu_ticks[CPU_STATE_SYSTEM];
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)cpu_info.cpu_ticks[CPU_STATE_SYSTEM]/100.0),
                             NULL,
                             &(label_pair_const_t){.name="state", "sys"},
                             &(label_pair_const_t){.name="cpu", buffer_cpu}, NULL);
        cpu_all_idle += cpu_info.cpu_ticks[CPU_STATE_IDLE];
        metric_family_append(&fams[FAM_CPU_USAGE],
                             VALUE_COUNTER_FLOAT64((double)cpu_info.cpu_ticks[CPU_STATE_IDLE]/100.0),
                             NULL,
                             &(label_pair_const_t){.name="state", "idle"},
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

    metric_family_append(&fams[FAM_CPU_COUNT], VALUE_GAUGE(cpu_list_len), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_CPU_MAX, now);

    return 0;
}
