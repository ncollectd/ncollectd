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

#include "processes.h"

#include <kvm.h>
#include <sys/param.h>
#include <sys/user.h>
#include <sys/sysctl.h>

static int pagesize;

extern metric_family_t fams[];

extern procstat_t *list_head_g;

extern bool want_init;

int ps_read(void)
{
    double proc_state[PROC_STATE_MAX];

    for (size_t i = 0; i < PROC_STATE_MAX; i++) {
        proc_state[i] = NAN;
    }

    int running = 0;
    int sleeping = 0;
    int zombies = 0;
    int stopped = 0;
    int blocked = 0;
    int idle = 0;
    int wait = 0;

    kvm_t *kd;
    char errbuf[_POSIX2_LINE_MAX];
    struct kinfo_proc *procs; /* array of processes */
    struct kinfo_proc *proc_ptr = NULL;
    int count; /* returns number of processes */

    process_entry_t pse;

    ps_list_reset();

    /* Open the kvm interface, get a descriptor */
    kd = kvm_openfiles(NULL, "/dev/null", NULL, 0, errbuf);
    if (kd == NULL) {
        PLUGIN_ERROR("Cannot open kvm interface: %s", errbuf);
        return 0;
    }

    /* Get the list of processes. */
    procs = kvm_getprocs(kd, KERN_PROC_ALL, 0, &count);
    if (procs == NULL) {
        PLUGIN_ERROR("Cannot get kvm processes list: %s", kvm_geterr(kd));
        kvm_close(kd);
        return 0;
    }

    /* Iterate through the processes in kinfo_proc */
    for (int i = 0; i < count; i++) {
        /* Create only one process list entry per _process_, i.e.
         * filter out threads (duplicate PID entries). */
        if ((proc_ptr == NULL) || (proc_ptr->kp_pid != procs[i].kp_pid)) {
            char cmdline[CMDLINE_BUFFER_SIZE] = "";
            bool have_cmdline = 0;

            proc_ptr = &(procs[i]);
            /* Don't probe system processes and processes without arguments */
            if (((procs[i].kp_flags & P_SYSTEM) == 0) && (procs[i].kp_comm[0] != '\0')) {
                char **argv;
                int argc;
                int status;

                /* retrieve the arguments */
                argv = kvm_getargv(kd, proc_ptr, /* nchr = */ 0);
                argc = 0;
                if ((argv != NULL) && (argv[0] != NULL)) {
                    while (argv[argc] != NULL)
                        argc++;

                    status = strjoin(cmdline, sizeof(cmdline), argv, argc, " ");
                    if (status < 0)
                        PLUGIN_WARNING("Command line did not fit into buffer.");
                    else
                        have_cmdline = 1;
                }
            }

            memset(&pse, 0, sizeof(pse));
            pse.id = procs[i].kp_pid;

            pse.num_proc = 1;
            pse.num_lwp = procs[i].kp_nthreads;

            pse.vmem_size = procs[i].kp_vm_map_size;
            pse.vmem_rss = procs[i].kp_vm_rssize * pagesize;
            pse.vmem_data = procs[i].kp_vm_dsize * pagesize;
            pse.vmem_code = procs[i].kp_vm_tsize * pagesize;
            pse.stack_size = procs[i].kp_vm_ssize * pagesize;
            pse.vmem_minflt_counter = procs[i].kp_ru.ru_minflt;
            pse.vmem_majflt_counter = procs[i].kp_ru.ru_majflt;

            pse.cpu_user_counter = 0;
            pse.cpu_system_counter = 0;

            pse.cpu_user_counter = procs[i].kp_ru.ru_utime.tv_usec +
                                   (1000000lu * procs[i].kp_ru.ru_utime.tv_sec);
            pse.cpu_system_counter = procs[i].kp_ru.ru_stime.tv_usec +
                                     (1000000lu * procs[i].kp_ru.ru_stime.tv_sec);

            /* no I/O data */
            pse.io_rchar = -1;
            pse.io_wchar = -1;
            pse.io_syscr = -1;
            pse.io_syscw = -1;
            pse.io_diskr = -1;
            pse.io_diskw = -1;

            /* file descriptor count not implemented */
            pse.num_fd = 0;

            /* Number of memory mappings */
            pse.num_maps = 0;

            /* context switch counters not implemented */
            pse.cswitch_vol = -1;
            pse.cswitch_invol = -1;

            pse.sched_running = -1;
            pse.sched_waiting = -1;
            pse.sched_timeslices = -1;

            ps_list_add(procs[i].kp_comm, have_cmdline ? cmdline : NULL, &pse);

            switch (procs[i].kp_stat) {
            case SSTOP:
                stopped++;
                break;
            case SACTIVE:
                running++;
                break;
            case SIDL:
                idle++;
                break;
            case SCORE:
                blocked++;
                break;
            case SZOMB:
                zombies++;
                break;
            }
        }
    }

    kvm_close(kd);

    proc_state[PROC_STATE_RUNNING] = running;
    proc_state[PROC_STATE_SLEEPING] = sleeping;
    proc_state[PROC_STATE_ZOMBIES] = zombies;
    proc_state[PROC_STATE_STOPPED] = stopped;
    proc_state[PROC_STATE_BLOCKED] = blocked;
    proc_state[PROC_STATE_IDLE] = idle;
    proc_state[PROC_STATE_WAIT] = wait;
    ps_submit_state(proc_state);

    for (procstat_t *ps_ptr = list_head_g; ps_ptr != NULL; ps_ptr = ps_ptr->next)
        ps_metric_append_proc_list(ps_ptr);

    want_init = false;

    plugin_dispatch_metric_family_array(fams, FAM_PROC_MAX, 0);
    return 0;
}

int ps_init(void)
{
    pagesize = getpagesize();

    return 0;
}
