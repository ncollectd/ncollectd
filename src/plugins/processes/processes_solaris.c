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
#include "libutils/kstat.h"

/* Hack: Avoid #error when building a 32-bit binary with
 * _FILE_OFFSET_BITS=64. There is a reason for this #error, as one
 * of the structures in <sys/procfs.h> uses an off_t, but that
 * isn't relevant to our usage of procfs. */
#if !defined(_LP64) && _FILE_OFFSET_BITS == 64
#define SAVE_FOB_64
#undef _FILE_OFFSET_BITS
#endif

#include <procfs.h>

#ifdef SAVE_FOB_64
#define _FILE_OFFSET_BITS 64
#undef SAVE_FOB_64
#endif

#include <dirent.h>
#include <sys/user.h>

#ifndef MAXCOMLEN
#define MAXCOMLEN 16
#endif

#include "processes.h"

static kstat_ctl_t *kc;

static char *ps_get_cmdline(long pid, char *name __attribute__((unused)),
                            char *buffer, size_t buffer_size)
{
    char path[PATH_MAX];
    psinfo_t info;
    ssize_t status;

    snprintf(path, sizeof(path), "/proc/%li/psinfo", pid);

    status = read_file_contents(path, (void *)&info, sizeof(info));
    if ((status < 0) || (((size_t)status) != sizeof(info))) {
        PLUGIN_ERROR("Unexpected return value while reading \"%s\": "
                     "Returned %zd but expected %" PRIsz ".", path, status, buffer_size);
        return NULL;
    }

    sstrncpy(buffer, info.pr_psargs, buffer_size);

    return buffer;
}

/*
 * Reads process information on the Solaris OS. The information comes mainly
 * from
 * /proc/PID/status, /proc/PID/psinfo and /proc/PID/usage
 * The values for input and output chars are calculated "by hand"
 * Added a few "solaris" specific process states as well
 */
static int ps_read_process(long pid, process_entry_t *ps, char *state)
{
    char f_status[PATH_MAX];
    char f_psinfo[PATH_MAX];
    char f_usage[PATH_MAX];

    snprintf(f_status, sizeof(f_status), "/proc/%li/status", pid);
    snprintf(f_psinfo, sizeof(f_psinfo), "/proc/%li/psinfo", pid);
    snprintf(f_usage, sizeof(f_usage), "/proc/%li/usage", pid);

    pstatus_t *myStatus = calloc(1, sizeof(*myStatus));
    psinfo_t *myInfo = calloc(1, sizeof(*myInfo));
    prusage_t *myUsage = calloc(1, sizeof(*myUsage));
    if ((myStatus == NULL) || (myInfo == NULL) || (myUsage == NULL)) {
        PLUGIN_ERROR("calloc failed.");
        free(myStatus);
        free(myInfo);
        free(myUsage);
        return -1;
    }

    if (read_file_contents(f_psinfo, myInfo, sizeof(*myInfo)) != (ssize_t)sizeof(*myInfo)) {
        free(myStatus);
        free(myInfo);
        free(myUsage);
        return -1;
    }

    sstrncpy(ps->name, myInfo->pr_fname, sizeof(myInfo->pr_fname));
    ps->starttime = (unsigned long long)myInfo->pr_start.tv_sec * 1000000000 +
                    myInfo->pr_start.tv_nsec;
    /* The status file of a zombie cannot be read, only its psinfo. */
    if ((myInfo->pr_wstat != 0) || (myInfo->pr_lwp.pr_sname == 'Z')) {
        ps->num_proc = 0;
        ps->num_lwp = 0;
        *state = (char)'Z';

        free(myStatus);
        free(myInfo);
        free(myUsage);
        return 0;
    }

    ssize_t status_len = read_file_contents(f_status, myStatus, sizeof(*myStatus));
    ssize_t usage_len = read_file_contents(f_usage, myUsage, sizeof(*myUsage));
    if ((status_len != (ssize_t)sizeof(*myStatus)) || (usage_len != (ssize_t)sizeof(*myUsage))) {
        free(myStatus);
        free(myInfo);
        free(myUsage);
        return -1;
    }

    ps->num_proc = 1;
    ps->num_lwp = myInfo->pr_nlwp;


    /*
     * Convert system time and user time from nanoseconds to microseconds
     * for compatibility with the linux module
     */
    ps->cpu_system_counter = (int64_t)myStatus->pr_stime.tv_sec * 1000000 +
                             myStatus->pr_stime.tv_nsec / 1000;
    ps->cpu_user_counter = (int64_t)myStatus->pr_utime.tv_sec * 1000000 +
                           myStatus->pr_utime.tv_nsec / 1000;

    /*
     * Convert rssize from KB to bytes to be consistent w/ the linux module
     */
    ps->vmem_rss = myInfo->pr_rssize * 1024;
    ps->vmem_size = myInfo->pr_size * 1024;
    ps->vmem_minflt_counter = myUsage->pr_minf;
    ps->vmem_majflt_counter = myUsage->pr_majf;

    /*
     * TODO: Data and code segment calculations for Solaris
     */

    ps->vmem_data = 0;
    ps->vmem_code = 0;
    ps->stack_size = myStatus->pr_stksize;

    /*
     * TODO: File descriptor count for Solaris
     */
    ps->num_fd = 0;

    /* Number of memory mappings */
    ps->num_maps = 0;

    /*
     * Calculating input/output chars
     * Formula used is total chars / total blocks => chars/block
     * then convert input/output blocks to chars
     */
    ulong_t tot_chars = myUsage->pr_ioch;
    ulong_t tot_blocks = myUsage->pr_inblk + myUsage->pr_oublk;
    ulong_t chars_per_block = 1;
    if (tot_blocks != 0)
        chars_per_block = tot_chars / tot_blocks;
    ps->io_rchar = myUsage->pr_inblk * chars_per_block;
    ps->io_wchar = myUsage->pr_oublk * chars_per_block;
    ps->io_syscr = myUsage->pr_sysc;
    ps->io_syscw = myUsage->pr_sysc;
    ps->io_diskr = -1;
    ps->io_diskw = -1;
    ps->io_cancelled_diskw = -1;

    /*
     * TODO: context switch counters for Solaris
     */
    ps->cswitch_vol = -1;
    ps->cswitch_invol = -1;

    ps->sched_running = -1;
    ps->sched_waiting = -1;
    ps->sched_timeslices = -1;

    /*
     * TODO: Find way of setting BLOCKED and PAGING status
     */

    *state = (char)'R';
    if (myStatus->pr_flags & PR_ASLEEP)
        *state = (char)'S';
    else if (myStatus->pr_flags & PR_STOPPED)
        *state = (char)'T';
    else if (myStatus->pr_flags & PR_DETACH)
        *state = (char)'E';
    else if (myStatus->pr_flags & PR_DAEMON)
        *state = (char)'A';
    else if (myStatus->pr_flags & PR_ISSYS)
        *state = (char)'Y';
    else if (myStatus->pr_flags & PR_ORPHAN)
        *state = (char)'O';

    free(myStatus);
    free(myInfo);
    free(myUsage);

    return 0;
}

/*
 * Reads the number of threads created since the last reboot. On Solaris these
 * are retrieved from kstat (module cpu, name sys, class misc, stat nthreads).
 * The result is the sum for all the threads created on each cpu
 */
static int read_fork_rate(void)
{
    uint64_t result = 0;

    if (kc == NULL)
        return -1;

    for (kstat_t *ksp_chain = kc->kc_chain; ksp_chain != NULL; ksp_chain = ksp_chain->ks_next) {
        if ((strcmp(ksp_chain->ks_module, "cpu") != 0) ||
            (strcmp(ksp_chain->ks_name, "sys") != 0) ||
            (strcmp(ksp_chain->ks_class, "misc") != 0))
            continue;

        kstat_read(kc, ksp_chain, NULL);

        long long tmp = get_kstat_value(ksp_chain, "nthreads");
        if (tmp != -1LL)
            result += tmp;
    }

    ps_submit_forks(result);
    return 0;
}

int ps_read(void)
{

    double proc_state[PROC_STATE_MAX];

    for (size_t i = 0; i < PROC_STATE_MAX; i++) {
        proc_state[i] = NAN;
    }

    /*
     * The Solaris section adds a few more process states and removes some
     * process states compared to linux. Most notably there is no "PAGING"
     * and "BLOCKED" state for a process.  The rest is similar to the linux
     * code.
     */
    int running = 0;
    int sleeping = 0;
    int zombies = 0;
    int stopped = 0;
    int detached = 0;
    int daemon = 0;
    int system = 0;
    int orphan = 0;

    struct dirent *ent;
    DIR *proc;

    int status;
    char state;

    char cmdline[PRARGSZ];

    if (kc == NULL)
        return -1;

    if (kstat_chain_update(kc) < 0) {
        PLUGIN_ERROR("kstat_chain_update failed.");
        return -1;
    }

    ps_list_reset();

    proc = opendir("/proc");
    if (proc == NULL) {
        PLUGIN_ERROR("Cannot open '/proc': %s", STRERRNO);
         return -1;
    }

    while ((ent = readdir(proc)) != NULL) {
        long pid;
        process_entry_t pse;
        char *endptr;

        if (!isdigit((unsigned char)ent->d_name[0]))
            continue;

        pid = strtol(ent->d_name, &endptr, 10);
        if (*endptr != 0) /* value didn't completely parse as a number */
            continue;

        memset(&pse, 0, sizeof(pse));
        pse.id = pid;

        status = ps_read_process(pid, &pse, &state);
        if (status != 0) {
            DEBUG("ps_read_process failed: %i", status);
            continue;
        }

        switch (state) {
        case 'R':
            running++;
            break;
        case 'S':
            sleeping++;
            break;
        case 'E':
            detached++;
            break;
        case 'Z':
            zombies++;
            break;
        case 'T':
            stopped++;
            break;
        case 'A':
            daemon++;
            break;
        case 'Y':
            system++;
            break;
        case 'O':
            orphan++;
            break;
        }

        ps_list_add(pse.name, ps_get_cmdline(pid, pse.name, cmdline, sizeof(cmdline)), pid, &pse);
    }
    closedir(proc);

    proc_state[PROC_STATE_RUNNING] = running;
    proc_state[PROC_STATE_SLEEPING] = sleeping;
    proc_state[PROC_STATE_ZOMBIES] = zombies;
    proc_state[PROC_STATE_STOPPED] = stopped;
    proc_state[PROC_STATE_DETACHED] = detached;
    proc_state[PROC_STATE_DAEMON] = daemon;
    proc_state[PROC_STATE_SYSTEM] = system;
    proc_state[PROC_STATE_ORPHAN] = orphan;
    ps_submit_state(proc_state);

    read_fork_rate();

    ps_dispatch();

    return 0;
}

int ps_init(void)
{
    if (kc == NULL)
        kc = kstat_open();

    if (kc == NULL) {
        PLUGIN_ERROR("kstat_open failed.");
        return -1;
    }

    return 0;
}

int ps_shutdown(void)
{
    if (kc != NULL) {
        kstat_close(kc);
        kc = NULL;
    }

    ps_list_free();
    return 0;
}
