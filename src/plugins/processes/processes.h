/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include <regex.h>

#ifdef HAVE_TASKSTATS
#include "taskstats.h"
#endif

#ifndef CMDLINE_BUFFER_SIZE
#if defined(ARG_MAX) && (ARG_MAX < 4096)
#define CMDLINE_BUFFER_SIZE ARG_MAX
#else
#define CMDLINE_BUFFER_SIZE 4096
#endif
#endif

enum {
    COLLECT_FILE_DESCRIPTORS = (1 <<  0),
    COLLECT_MEMORY_MAPS      = (1 <<  1),
    COLLECT_DELAY_ACCOUNTING = (1 <<  2),
};

enum {
    FAM_PROCESSES_CTX,
    FAM_PROCESSES_FORKS,
    FAM_PROCESSES_STATE,
    FAM_PROC_VMEM_SIZE,
    FAM_PROC_VMEM_RSS,
    FAM_PROC_VMEM_DATA,
    FAM_PROC_VMEM_CODE,
    FAM_PROC_VMEM_STACK,
    FAM_PROC_CPU_USER,
    FAM_PROC_CPU_SYSTEM,
    FAM_PROC_NUM_PROCESSS,
    FAM_PROC_NUM_THREADS,
    FAM_PROC_VMEM_MINFLT,
    FAM_PROC_VMEM_MAJFLT,
    FAM_PROC_IO_RCHAR,
    FAM_PROC_IO_WCHAR,
    FAM_PROC_IO_SYSCR,
    FAM_PROC_IO_SYSCW,
    FAM_PROC_IO_DISKR,
    FAM_PROC_IO_DISKW,
    FAM_PROC_FILE_HANDLES,
    FAM_PROC_MEMORY_MAPPED_REGIONS,
    FAM_PROC_CTX_VOLUNTARY,
    FAM_PROC_CTX_INVOLUNTARY,
    FAM_PROC_DELAY_CPU,
    FAM_PROC_DELAY_BLKIO,
    FAM_PROC_DELAY_SWAPIN,
    FAM_PROC_DELAY_FREEPAGES,
    FAM_PROC_DELAY_IRQ,
    FAM_PROC_SCHED_RUNNING,
    FAM_PROC_SCHED_WAITING,
    FAM_PROC_SCHED_TIMESLICES,
    FAM_PROC_MAX
};

enum {
    PROC_STATE_BLOCKED,
    PROC_STATE_DAEMON,
    PROC_STATE_DEAD,
    PROC_STATE_DETACHED,
    PROC_STATE_IDLE,
    PROC_STATE_ONPROC,
    PROC_STATE_ORPHAN,
    PROC_STATE_PAGING,
    PROC_STATE_RUNNING,
    PROC_STATE_SLEEPING,
    PROC_STATE_STOPPED,
    PROC_STATE_SYSTEM,
    PROC_STATE_WAIT,
    PROC_STATE_ZOMBIES,
    PROC_STATE_MAX
};

#define PROCSTAT_NAME_LEN 256

typedef struct process_entry_s {
    unsigned long id;
    char name[PROCSTAT_NAME_LEN];
    unsigned long long starttime;

    unsigned long num_proc;
    unsigned long num_lwp;
    unsigned long num_fd;
    unsigned long num_maps;
    unsigned long vmem_size;
    unsigned long vmem_rss;
    unsigned long vmem_data;
    unsigned long vmem_code;
    unsigned long stack_size;

    int64_t vmem_minflt_counter;
    int64_t vmem_majflt_counter;

    int64_t cpu_user_counter;
    int64_t cpu_system_counter;

    /* io data */
    int64_t io_rchar;
    int64_t io_wchar;
    int64_t io_syscr;
    int64_t io_syscw;
    int64_t io_diskr;
    int64_t io_diskw;
    bool has_io;

    int64_t cswitch_vol;
    int64_t cswitch_invol;

    int64_t sched_running;
    int64_t sched_waiting;
    int64_t sched_timeslices;
    bool has_sched;

#ifdef HAVE_TASKSTATS
    ts_delay_t delay;
#endif
    bool has_delay;

    bool has_fd;

    bool has_maps;
} process_entry_t;

typedef struct procstat_entry_s {
    unsigned long id;
    unsigned char age;
    unsigned long long starttime;

    int64_t vmem_minflt_counter;
    int64_t vmem_majflt_counter;

    int64_t cpu_user_counter;
    int64_t cpu_system_counter;

    /* io data */
    int64_t io_rchar;
    int64_t io_wchar;
    int64_t io_syscr;
    int64_t io_syscw;
    int64_t io_diskr;
    int64_t io_diskw;

    int64_t cswitch_vol;
    int64_t cswitch_invol;

    int64_t sched_running;
    int64_t sched_waiting;
    int64_t sched_timeslices;

#ifdef HAVE_TASKSTATS
    counter_to_rate_state_t delay_cpu;
    counter_to_rate_state_t delay_blkio;
    counter_to_rate_state_t delay_swapin;
    counter_to_rate_state_t delay_freepages;
#endif

    struct procstat_entry_s *next;
} procstat_entry_t;

typedef struct procstat_s {
    char name[PROCSTAT_NAME_LEN];
    regex_t *re;

    unsigned long num_proc;
    unsigned long num_lwp;
    unsigned long num_fd;
    unsigned long num_maps;
    unsigned long vmem_size;
    unsigned long vmem_rss;
    unsigned long vmem_data;
    unsigned long vmem_code;
    unsigned long stack_size;

    int64_t vmem_minflt_counter;
    int64_t vmem_majflt_counter;

    int64_t cpu_user_counter;
    int64_t cpu_system_counter;

    /* io data */
    int64_t io_rchar;
    int64_t io_wchar;
    int64_t io_syscr;
    int64_t io_syscw;
    int64_t io_diskr;
    int64_t io_diskw;

    int64_t cswitch_vol;
    int64_t cswitch_invol;

    int64_t sched_running;
    int64_t sched_waiting;
    int64_t sched_timeslices;

    /* Linux Delay Accounting. */
    double delay_cpu;
    double delay_blkio;
    double delay_swapin;
    double delay_freepages;
    double delay_irq;

    uint64_t flags;

    struct procstat_s *next;
    struct procstat_entry_s *instances;
} procstat_t;

void ps_fill_details(procstat_t *ps, process_entry_t *entry);

void ps_list_reset(void);

void ps_list_free(void);

void ps_list_add(const char *name, const char *cmdline, process_entry_t *entry);

void ps_submit_state(double *proc_state);

void ps_submit_forks(uint64_t value);
void ps_submit_ctxt(uint64_t value);

void ps_metric_append_proc_list(procstat_t *ps);
