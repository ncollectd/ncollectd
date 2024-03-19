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

#include <kvm.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include "processes.h"

static int pagesize;
static unsigned int maxslp;

extern metric_family_t fams[];
extern procstat_t *list_head_g;
extern bool want_init;

int ps_read(void)
{

    double proc_state[PROC_STATE_MAX];

    for (size_t i = 0; i < PROC_STATE_MAX; i++) {
        proc_state[i] = NAN;
    }

// HAVE_LIBKVM_GETPROCS && HAVE_STRUCT_KINFO_PROC2_NETBSD
    int running = 0;
    int sleeping = 0;
    int zombies = 0;
    int stopped = 0;
    int blocked = 0;
    int idle = 0;
    int wait = 0;

    kvm_t *kd;
    char errbuf[_POSIX2_LINE_MAX];
    struct kinfo_proc2 *procs; /* array of processes */
    struct kinfo_proc2 *proc_ptr = NULL;
    struct kinfo_proc2 *p;
    int count; /* returns number of processes */
    int i;
    int l, nlwps;
    struct kinfo_lwp *kl;

    procstat_t *ps_ptr;
    process_entry_t pse;

    ps_list_reset();

    /* Open the kvm interface, get a descriptor */
    kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
    if (kd == NULL) {
        ERROR("processes plugin: Cannot open kvm interface: %s", errbuf);
        return (0);
    }

    /* Get the list of processes. */
    procs =
            kvm_getproc2(kd, KERN_PROC_ALL, 0, sizeof(struct kinfo_proc2), &count);
    if (procs == NULL) {
        ERROR("processes plugin: Cannot get kvm processes list: %s",
                    kvm_geterr(kd));
        kvm_close(kd);
        return (0);
    }

    /* Iterate through the processes in kinfo_proc */
    for (i = 0; i < count; i++) {
        /* Create only one process list entry per _process_, i.e.
         * filter out threads (duplicate PID entries). */
        if ((proc_ptr == NULL) || (proc_ptr->p_pid != procs[i].p_pid)) {
            char cmdline[CMDLINE_BUFFER_SIZE] = "";
            _Bool have_cmdline = 0;

            proc_ptr = &(procs[i]);
            /* Don't probe system processes and processes without arguments */
            if (((procs[i].p_flag & P_SYSTEM) == 0) && (procs[i].p_comm[0] != 0)) {
                char **argv;
                int argc;
                int status;

                /* retrieve the arguments */
                argv = kvm_getargv2(kd, proc_ptr, 0);
                argc = 0;
                if ((argv != NULL) && (argv[0] != NULL)) {
                    while (argv[argc] != NULL)
                        argc++;

                    status = strjoin(cmdline, sizeof(cmdline), argv, argc, " ");
                    if (status < 0)
                        WARNING("processes plugin: Command line did not fit into buffer.");
                    else
                        have_cmdline = 1;
                }
            } /* if (process has argument list) */

            memset(&pse, 0, sizeof(pse));
            pse.id = procs[i].p_pid;

            pse.num_proc = 1;
            pse.num_lwp = procs[i].p_nlwps;

            pse.vmem_size = procs[i].p_uru_maxrss * pagesize;
            pse.vmem_rss = procs[i].p_vm_rssize * pagesize;
            pse.vmem_data = procs[i].p_vm_dsize * pagesize;
            pse.vmem_code = procs[i].p_vm_tsize * pagesize;
            pse.stack_size = procs[i].p_vm_ssize * pagesize;
            pse.vmem_minflt_counter = procs[i].p_uru_minflt;
            pse.vmem_majflt_counter = procs[i].p_uru_majflt;

            pse.cpu_user_counter = 0;
            pse.cpu_system_counter = 0;
            /* context switch counters not implemented */
            pse.cswitch_vol = -1;
            pse.cswitch_invol = -1;

            pse.sched_running = -1;
            pse.sched_waiting = -1;
            pse.sched_timeslices = -1;

            /*
             * The u-area might be swapped out, and we can't get
             * at it because we have a crashdump and no swap.
             * If it's here fill in these fields, otherwise, just
             * leave them 0.
             */
            if (procs[i].p_flag & P_INMEM) {
                pse.cpu_user_counter =
                        procs[i].p_uutime_usec + (1000000lu * procs[i].p_uutime_sec);
                pse.cpu_system_counter =
                        procs[i].p_ustime_usec + (1000000lu * procs[i].p_ustime_sec);
            }

            /* no I/O data */
            pse.io_rchar = -1;
            pse.io_wchar = -1;
            pse.io_syscr = procs[i].p_uru_inblock;
            pse.io_syscw = procs[i].p_uru_oublock;

            /* file descriptor count not implemented */
            pse.num_fd = 0;

            /* Number of memory mappings */
            pse.num_maps = 0;

            /* context switch counters not implemented */
            pse.cswitch_vol = -1;
            pse.cswitch_invol = -1;

            ps_list_add(procs[i].p_comm, have_cmdline ? cmdline : NULL, &pse);
        } /* if ((proc_ptr == NULL) || (proc_ptr->ki_pid != procs[i].ki_pid)) */

        /* system processes' LWPs end up in "running" state */
        if ((procs[i].p_flag & P_SYSTEM) != 0)
            continue;

        switch (procs[i].p_realstat) {
        case SSTOP:
        case SACTIVE:
        case SIDL:
            p = &(procs[i]);
            /* get info about LWPs */
            kl = kvm_getlwps(kd, p->p_pid, (u_long)p->p_paddr,
                                             sizeof(struct kinfo_lwp), &nlwps);

            for (l = 0; kl && l < nlwps; l++) {
                switch (kl[l].l_stat) {
                case LSONPROC:
                case LSRUN:
                    running++;
                    break;
                case LSSLEEP:
                    if (kl[l].l_flag & L_SINTR) {
                        if (kl[l].l_slptime > maxslp)
                            idle++;
                        else
                            sleeping++;
                    } else
                        blocked++;
                    break;
                case LSSTOP:
                    stopped++;
                    break;
                case LSIDL:
                    idle++;
                    break;
                }
            }
            break;
        case SZOMB:
        case SDYING:
        case SDEAD:
            zombies++;
            break;
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

    for (ps_ptr = list_head_g; ps_ptr != NULL; ps_ptr = ps_ptr->next)
        ps_metric_append_proc_list(ps_ptr);

    want_init = false;

    plugin_dispatch_metric_family_array(fams, FAM_PROC_MAX, 0);
    return 0;
}

int ps_init(void)
{
    int mib[2];
    mib[0] = CTL_VM;
    mib[1] = VM_MAXSLP;

    size_t size = sizeof(maxslp);
    if (sysctl(mib, 2, &maxslp, &size, NULL, 0) == -1)
        maxslp = 20; /* reasonable default? */

    pagesize = getpagesize();

    return 0;
}

