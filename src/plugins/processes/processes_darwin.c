// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2005 Lyonel Vincent
// SPDX-FileCopyrightText: Copyright (C) 2006-2017 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2008 Oleg King
// SPDX-FileCopyrightText: Copyright (C) 2009 Sebastian Harl
// SPDX-FileCopyrightText: Copyright (C) 2009 Andrés J. Díaz
// SPDX-FileCopyrightText: Copyright (C) 2009-2024 Manuel Sanmartín
// SPDX-FileCopyrightText: Copyright (C) 2010 Clément Stenac
// SPDX-FileCopyrightText: Copyright (C) 2012 Cosmin Ioiart
// SPDX-FileContributor: Lyonel Vincent <lyonel at ezix.org>
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Oleg King <king2 at kaluga.ru>
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>
// SPDX-FileContributor: Andrés J. Díaz <ajdiaz at connectical.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>
// SPDX-FileContributor: Clément Stenac <clement.stenac at diwi.org>
// SPDX-FileContributor: Cosmin Ioiart <cioiart at gmail.com>
// SPDX-FileContributor: Pavel Rochnyack <pavel2000 at ngs.ru>
// SPDX-FileContributor: Wilfried Goesgens <dothebart at citadel.org>

#include "plugin.h"
#include "libutils/common.h"

/* Include header files for the mach system, if they exist.. */
#ifdef HAVE_MACH_MACH_INIT_H
#    include <mach/mach_init.h>
#endif
#ifdef HAVE_MACH_HOST_PRIV_H
#    include <mach/host_priv.h>
#endif
#ifdef HAVE_MACH_MACH_ERROR_H
#    include <mach/mach_error.h>
#endif
#ifdef HAVE_MACH_MACH_HOST_H
#    include <mach/mach_host.h>
#endif
#ifdef HAVE_MACH_MACH_PORT_H
#    include <mach/mach_port.h>
#endif
#ifdef HAVE_MACH_MACH_TYPES_H
#    include <mach/mach_types.h>
#endif
#ifdef HAVE_MACH_MESSAGE_H
#    include <mach/message.h>
#endif
#ifdef HAVE_MACH_PROCESSOR_SET_H
#    include <mach/processor_set.h>
#endif
#ifdef HAVE_MACH_TASK_H
#    include <mach/task.h>
#endif
#ifdef HAVE_MACH_THREAD_ACT_H
#    include <mach/thread_act.h>
#endif
#ifdef HAVE_MACH_VM_REGION_H
#    include <mach/vm_region.h>
#endif
#ifdef HAVE_MACH_VM_MAP_H
#    include <mach/vm_map.h>
#endif
#ifdef HAVE_MACH_VM_PROT_H
#    include <mach/vm_prot.h>
#endif
#ifdef HAVE_SYS_SYSCTL_H
#    include <sys/sysctl.h>
#endif

static mach_port_t port_host_self;
static mach_port_t port_task_self;

static processor_set_name_array_t pset_list;
static mach_msg_type_number_t pset_list_len;

#include "processes.h"

extern metric_family_t fams[];
extern procstat_t *list_head_g;
extern bool want_init;

static int mach_get_task_name(task_t t, int *pid, char *name, size_t name_max_len)
{
    int mib[4];
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;

    if (pid_for_task(t, pid) != KERN_SUCCESS)
        return -1;
    mib[3] = *pid;

    struct kinfo_proc kp;
    size_t kp_size = sizeof(kp);
    if (sysctl(mib, 4, &kp, &kp_size, NULL, 0) != 0)
        return -1;

    if (name_max_len > (MAXCOMLEN + 1))
        name_max_len = MAXCOMLEN + 1;

    strncpy(name, kp.kp_proc.p_comm, name_max_len - 1);
    name[name_max_len - 1] = '\0';

    DEBUG("pid = %i; name = %s;", *pid, name);

    /* We don't do the special handling for `p_comm == "LaunchCFMApp"' as
     * `top' does it, because it is a lot of work and only used when
     * debugging. -octo */

    return 0;
}

int ps_read(void)
{
    double proc_state[PROC_STATE_MAX];

    for (size_t i = 0; i < PROC_STATE_MAX; i++) {
        proc_state[i] = NAN;
    }

    kern_return_t status;

    processor_set_t port_pset_priv;

    task_array_t task_list;
    mach_msg_type_number_t task_list_len;

    int task_pid = 0;
    char task_name[MAXCOMLEN + 1];

    thread_act_array_t thread_list;
    mach_msg_type_number_t thread_list_len;
    thread_basic_info_data_t thread_data;
    mach_msg_type_number_t thread_data_len;

    int running = 0;
    int sleeping = 0;
    int zombies = 0;
    int stopped = 0;
    int blocked = 0;

    procstat_t *ps;
    process_entry_t pse;

    ps_list_reset();

    /*
     * The Mach-concept is a little different from the traditional UNIX
     * concept: All the work is done in threads. Threads are contained in
     * `tasks'. Therefore, `task status' doesn't make much sense, since
     * it's actually a `thread status'.
     * Tasks are assigned to sets of processors, so that's where you go to
     * get a list.
     */
    for (mach_msg_type_number_t pset = 0; pset < pset_list_len; pset++) {
        if ((status = host_processor_set_priv(port_host_self, pset_list[pset],
                                              &port_pset_priv)) != KERN_SUCCESS) {
            PLUGIN_ERROR("host_processor_set_priv failed: %s\n", mach_error_string(status));
            continue;
        }

        if ((status = processor_set_tasks(port_pset_priv, &task_list, &task_list_len)) != KERN_SUCCESS) {
            PLUGIN_ERROR("processor_set_tasks failed: %s\n", mach_error_string(status));
            mach_port_deallocate(port_task_self, port_pset_priv);
            continue;
        }

        for (mach_msg_type_number_t task = 0; task < task_list_len; task++) {
            ps = NULL;
            if (mach_get_task_name(task_list[task], &task_pid, task_name, PROCSTAT_NAME_LEN) == 0) {
                /* search for at least one match */
// FIXME
#if 0
                for (ps = list_head_g; ps != NULL; ps = ps->next)
                    /* FIXME: cmdline should be here instead of NULL */
                    if (ps_list_match(task_name, NULL, ps) == 1)
                        break;
#endif
            }

            /* Collect more detailed statistics for this process */
            if (ps != NULL) {
                task_basic_info_data_t task_basic_info;
                mach_msg_type_number_t task_basic_info_len;
                task_events_info_data_t task_events_info;
                mach_msg_type_number_t task_events_info_len;
                task_absolutetime_info_data_t task_absolutetime_info;
                mach_msg_type_number_t task_absolutetime_info_len;

                memset(&pse, '\0', sizeof(pse));
                pse.id = task_pid;

                task_basic_info_len = TASK_BASIC_INFO_COUNT;
                status = task_info(task_list[task], TASK_BASIC_INFO,
                                   (task_info_t)&task_basic_info, &task_basic_info_len);
                if (status != KERN_SUCCESS) {
                    PLUGIN_ERROR("task_info failed: %s", mach_error_string(status));
                    continue; /* with next thread_list */
                }

                task_events_info_len = TASK_EVENTS_INFO_COUNT;
                status = task_info(task_list[task], TASK_EVENTS_INFO,
                                   (task_info_t)&task_events_info, &task_events_info_len);
                if (status != KERN_SUCCESS) {
                    PLUGIN_ERROR("task_info failed: %s", mach_error_string(status));
                    continue; /* with next thread_list */
                }

                task_absolutetime_info_len = TASK_ABSOLUTETIME_INFO_COUNT;
                status = task_info(task_list[task], TASK_ABSOLUTETIME_INFO,
                                                     (task_info_t)&task_absolutetime_info,
                                                     &task_absolutetime_info_len);
                if (status != KERN_SUCCESS) {
                    PLUGIN_ERROR("task_info failed: %s", mach_error_string(status));
                    continue; /* with next thread_list */
                }

                pse.num_proc++;
                pse.vmem_size = task_basic_info.virtual_size;
                pse.vmem_rss = task_basic_info.resident_size;
                /* Does not seem to be easily exposed */
                pse.vmem_data = 0;
                pse.vmem_code = 0;

                pse.io_rchar = -1;
                pse.io_wchar = -1;
                pse.io_syscr = -1;
                pse.io_syscw = -1;
                pse.io_diskr = -1;
                pse.io_diskw = -1;

                /* File descriptor count not implemented */
                pse.num_fd = 0;

                /* Number of memory mappings */
                pse.num_maps = 0;

                pse.vmem_minflt_counter = task_events_info.cow_faults;
                pse.vmem_majflt_counter = task_events_info.faults;

                pse.cpu_user_counter = task_absolutetime_info.total_user;
                pse.cpu_system_counter = task_absolutetime_info.total_system;

                /* context switch counters not implemented */
                pse.cswitch_vol = -1;
                pse.cswitch_invol = -1;

                pse.sched_running = -1;
                pse.sched_waiting = -1;
                pse.sched_timeslices = -1;
            }

            status = task_threads(task_list[task], &thread_list, &thread_list_len);
            if (status != KERN_SUCCESS) {
                /* Apple's `top' treats this case a zombie. It
                 * makes sense to some extend: A `zombie'
                 * thread is nonsense, since the task/process
                 * is dead. */
                zombies++;
                PLUGIN_DEBUG("task_threads failed: %s", mach_error_string(status));
                if (task_list[task] != port_task_self)
                    mach_port_deallocate(port_task_self, task_list[task]);
                continue; /* with next task_list */
            }

            for (mach_msg_type_number_t thread = 0; thread < thread_list_len;
                     thread++) {
                thread_data_len = THREAD_BASIC_INFO_COUNT;
                status = thread_info(thread_list[thread], THREAD_BASIC_INFO,
                                                         (thread_info_t)&thread_data, &thread_data_len);
                if (status != KERN_SUCCESS) {
                    PLUGIN_ERROR("thread_info failed: %s", mach_error_string(status));
                    if (task_list[task] != port_task_self)
                        mach_port_deallocate(port_task_self, thread_list[thread]);
                    continue; /* with next thread_list */
                }

                if (ps != NULL)
                    pse.num_lwp++;

                switch (thread_data.run_state) {
                case TH_STATE_RUNNING:
                    running++;
                    break;
                case TH_STATE_STOPPED:
                /* What exactly is `halted'? */
                case TH_STATE_HALTED:
                    stopped++;
                    break;
                case TH_STATE_WAITING:
                    sleeping++;
                    break;
                case TH_STATE_UNINTERRUPTIBLE:
                    blocked++;
                    break;
                /* There is no `zombie' case here,
                 * since there are no zombie-threads.
                 * There's only zombie tasks, which are
                 * handled above. */
                default:
                    PLUGIN_WARNING("Unknown thread status: %i", thread_data.run_state);
                    break;
                } /* switch (thread_data.run_state) */

                if (task_list[task] != port_task_self) {
                    status = mach_port_deallocate(port_task_self, thread_list[thread]);
                    if (status != KERN_SUCCESS)
                        PLUGIN_ERROR("mach_port_deallocate failed: %s", mach_error_string(status));
                }
            } /* for (thread_list) */

            if ((status = vm_deallocate(port_task_self, (vm_address_t)thread_list,
                                        thread_list_len * sizeof(thread_act_t))) != KERN_SUCCESS) {
                PLUGIN_ERROR("vm_deallocate failed: %s", mach_error_string(status));
            }
            thread_list = NULL;
            thread_list_len = 0;

            /* Only deallocate the task port, if it isn't our own.
             * Don't know what would happen in that case, but this
             * is what Apple's top does.. ;) */
            if (task_list[task] != port_task_self) {
                status = mach_port_deallocate(port_task_self, task_list[task]);
                if (status != KERN_SUCCESS)
                    PLUGIN_ERROR("mach_port_deallocate failed: %s", mach_error_string(status));
            }

            if (ps != NULL)
                /* FIXME: cmdline should be here instead of NULL */
                ps_list_add(task_name, NULL, task_pid, &pse);
        }

        if ((status = vm_deallocate(port_task_self, (vm_address_t)task_list,
                                    task_list_len * sizeof(task_t))) != KERN_SUCCESS) {
            PLUGIN_ERROR("vm_deallocate failed: %s", mach_error_string(status));
        }
        task_list = NULL;
        task_list_len = 0;

        if ((status = mach_port_deallocate(port_task_self, port_pset_priv)) != KERN_SUCCESS) {
            PLUGIN_ERROR("mach_port_deallocate failed: %s", mach_error_string(status));
        }
    }

    proc_state[PROC_STATE_RUNNING] = running;
    proc_state[PROC_STATE_SLEEPING] = sleeping;
    proc_state[PROC_STATE_ZOMBIES] = zombies;
    proc_state[PROC_STATE_STOPPED] = stopped;
    proc_state[PROC_STATE_BLOCKED] = blocked;
    ps_submit_state(proc_state);

    for (ps = list_head_g; ps != NULL; ps = ps->next)
        ps_metric_append_proc_list(ps);

    want_init = false;

    plugin_dispatch_metric_family_array(fams, FAM_PROC_MAX, 0);

    return 0;
}

int ps_init(void)
{
    port_host_self = mach_host_self();
    port_task_self = mach_task_self();

    if (pset_list != NULL) {
        vm_deallocate(port_task_self, (vm_address_t)pset_list,
                                      pset_list_len * sizeof(processor_set_t));
        pset_list = NULL;
        pset_list_len = 0;
    }

    kern_return_t status = host_processor_sets(port_host_self, &pset_list, &pset_list_len);
    if (status != KERN_SUCCESS) {
        PLUGIN_ERROR("host_processor_sets failed: %s\n", mach_error_string(status));
        pset_list = NULL;
        pset_list_len = 0;
        return -1;
    }

    return 0;
}
