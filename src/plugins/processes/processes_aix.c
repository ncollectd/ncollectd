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

#include <procinfo.h>
#include <sys/types.h>

#define MAXPROCENTRY 32
#define MAXTHRDENTRY 16
#define MAXARGLN 1024

#include "processes.h"

extern metric_family_t fams[];
extern procstat_t *list_head_g;
extern bool want_init;

static struct procentry64 procentry[MAXPROCENTRY];
static struct thrdentry64 thrdentry[MAXTHRDENTRY];
static int pagesize;

#ifndef _AIXVERSION_610
int getprocs64(void *procsinfo, int sizproc, void *fdsinfo, int sizfd, pid_t *index, int count);
int getthrds64(pid_t, void *, int, tid64_t *, int);
#endif
int getargs(void *processBuffer, int bufferLen, char *argsBuffer, int argsLen);

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
    int paging = 0;
    int blocked = 0;

    pid_t pindex = 0;
    int nprocs;

    process_entry_t pse;

    ps_list_reset();
    while ((nprocs = getprocs64(procentry, sizeof(struct procentry64),
                                /* fdsinfo = */ NULL, sizeof(struct fdsinfo64),
                                &pindex, MAXPROCENTRY)) > 0) {
        for (int i = 0; i < nprocs; i++) {
            int nthreads;
            char arglist[MAXARGLN + 1];

            if (procentry[i].pi_state == SNONE)
                continue;
            /* if (procentry[i].pi_state == SZOMB)  FIXME */

            char *cmdline = procentry[i].pi_comm;
            char *cargs = procentry[i].pi_comm;
            if (procentry[i].pi_flags & SKPROC) {
                if (procentry[i].pi_pid == 0)
                    cmdline = "swapper";
                cargs = cmdline;
            } else {
                if (getargs(&procentry[i], sizeof(struct procentry64), arglist, MAXARGLN) >= 0) {
                    int n = -1;
                    while (++n < MAXARGLN) {
                        if (arglist[n] == '\0') {
                            if (arglist[n + 1] == '\0')
                                break;
                            arglist[n] = ' ';
                        }
                    }
                    cargs = arglist;
                }
            }

            memset(&pse, 0, sizeof(pse));

            pse.id = procentry[i].pi_pid;
            pse.num_lwp = procentry[i].pi_thcount;
            pse.num_proc = 1;

            tid64_t thindex = 0;
            while ((nthreads = getthrds64(procentry[i].pi_pid, thrdentry,
                                          sizeof(struct thrdentry64), &thindex,
                                          MAXTHRDENTRY)) > 0) {
                int j;

                for (j = 0; j < nthreads; j++) {
                    switch (thrdentry[j].ti_state) {
                    /* case TSNONE: break; */
                    case TSIDL:
                        blocked++;
                        break; /* FIXME is really blocked */
                    case TSRUN:
                        running++;
                        break;
                    case TSSLEEP:
                        sleeping++;
                        break;
                    case TSSWAP:
                        paging++;
                        break;
                    case TSSTOP:
                        stopped++;
                        break;
                    case TSZOMB:
                        zombies++;
                        break;
                    }
                }
                if (nthreads < MAXTHRDENTRY)
                    break;
            }

            /* tv_usec is nanosec ??? */
            pse.cpu_user_counter = procentry[i].pi_ru.ru_utime.tv_sec * 1000000 +
                                   procentry[i].pi_ru.ru_utime.tv_usec / 1000;

            /* tv_usec is nanosec ??? */
            pse.cpu_system_counter = procentry[i].pi_ru.ru_stime.tv_sec * 1000000 +
                                     procentry[i].pi_ru.ru_stime.tv_usec / 1000;

            pse.vmem_minflt_counter = procentry[i].pi_minflt;
            pse.vmem_majflt_counter = procentry[i].pi_majflt;

            pse.vmem_size = procentry[i].pi_tsize + procentry[i].pi_dvm * pagesize;
            pse.vmem_rss = (procentry[i].pi_drss + procentry[i].pi_trss) * pagesize;
            /* Not supported/implemented */
            pse.vmem_data = 0;
            pse.vmem_code = 0;
            pse.stack_size = 0;

            pse.io_rchar = -1;
            pse.io_wchar = -1;
            pse.io_syscr = -1;
            pse.io_syscw = -1;
            pse.io_diskr = -1;
            pse.io_diskw = -1;

            pse.num_fd = 0;
            pse.num_maps = 0;

            pse.cswitch_vol = -1;
            pse.cswitch_invol = -1;

            pse.sched_running = -1;
            pse.sched_waiting = -1;
            pse.sched_timeslices = -1;

            ps_list_add(cmdline, cargs, &pse);
        }

        if (nprocs < MAXPROCENTRY)
            break;
    }

    proc_state[PROC_STATE_RUNNING] = running;
    proc_state[PROC_STATE_SLEEPING] = sleeping;
    proc_state[PROC_STATE_ZOMBIES] = zombies;
    proc_state[PROC_STATE_STOPPED] = stopped;
    proc_state[PROC_STATE_PAGING] = paging;
    proc_state[PROC_STATE_BLOCKED] = blocked;
    ps_submit_state(proc_state);

    for (procstat_t *ps = list_head_g; ps != NULL; ps = ps->next)
        ps_metric_append_proc_list(ps);

    want_init = false;

    plugin_dispatch_metric_family_array(fams, FAM_PROC_MAX, 0);
    return 0;
}

int ps_init(void)
{
    pagesize = getpagesize();

    return 0;
}
