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

#ifdef KERNEL_SOLARIS
#    ifndef MAXCOMLEN
#        define MAXCOMLEN 16
#    endif
#endif

#include "processes.h"

#ifndef CMDLINE_BUFFER_SIZE
#    if defined(ARG_MAX) && (ARG_MAX < 4096)
#        define CMDLINE_BUFFER_SIZE ARG_MAX
#    else
#        define CMDLINE_BUFFER_SIZE 4096
#    endif
#endif

static metric_family_t fams_processes[FAM_PROCESSES_MAX] = {
    [FAM_PROCESSES_CTX] = {
          .name = "system_processes_contextswitch",
          .type = METRIC_TYPE_COUNTER,
          .help = "Total number of context switches across all CPUs."
    },
    [FAM_PROCESSES_FORKS] = {
          .name = "system_processes_forks",
          .type = METRIC_TYPE_COUNTER,
          .help = "Total number of processes and threads created in the system."
    },
    [FAM_PROCESSES_STATE] = {
         .name = "system_processes_state",
         .type = METRIC_TYPE_GAUGE,
         .help = "Summary of processes state.",
    },
};

static metric_family_t fams_proc[FAM_PROC_MAX] = {
    [FAM_PROC_VMEM_SIZE] = {
        .name = "system_process_vmem_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Virtual memory size in bytes.",
    },
    [FAM_PROC_VMEM_RSS] = {
        .name = "system_process_vmem_rss_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Resident Set Size: number of bytes the process hasin real memory."
    },
    [FAM_PROC_VMEM_DATA] = {
        .name = "system_process_vmem_data_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Size in bytes of data segments for these processes.",
    },
    [FAM_PROC_VMEM_CODE] = {
        .name = "system_process_vmem_code_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Size in bytes of code segments for these processes.",
    },
    [FAM_PROC_VMEM_STACK] = {
        .name = "system_process_vmem_stack_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Size in bytes of stack segments for these processes.",
    },
    [FAM_PROC_CPU_USER] = {
        .name = "system_process_cpu_user_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of time that this processes have been scheduled in user mode.",
    },
    [FAM_PROC_CPU_SYSTEM] = {
        .name = "system_process_cpu_system_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of time that this processes have been scheduled in kernel mode.",
    },
    [FAM_PROC_NUM_PROCESSS] = {
        .name = "system_process_num_process",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of processes."
    },
    [FAM_PROC_NUM_THREADS] = {
        .name = "system_process_num_threads",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of threads in this processes.",
    },
    [FAM_PROC_VMEM_MINFLT] = {
        .name = "system_process_vmem_minflt",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of minor faults the processes have made "
                "which have not required loading a memory page from disk.",
    },
    [FAM_PROC_VMEM_MAJFLT] = {
        .name = "system_process_vmem_majflt",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of major faults the processes have made "
                "which have required loading a memory page from disk.",
    },
    [FAM_PROC_IO_RCHAR] = {
        .name = "system_process_io_rchar_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of bytes returned by successful read(2) and similar system calls.",
    },
    [FAM_PROC_IO_WCHAR] = {
        .name = "system_process_io_wchar_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of bytes returned by successful write(2) and similar system calls.",
    },
    [FAM_PROC_IO_SYSCR] = {
        .name = "system_process_io_syscr",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of \"file read\" system calls—those from "
                "the read(2) family, sendfile(2), copy_file_range(2), etc.",
    },
    [FAM_PROC_IO_SYSCW] = {
        .name = "system_process_io_syscw",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of \"file write\" system calls—those from "
                "the write(2) family, sendfile(2), copy_file_range(2), etc.",
    },
    [FAM_PROC_IO_DISKR] = {
        .name = "system_process_io_diskr_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of bytes really fetched from the storage layer.",
    },
    [FAM_PROC_IO_DISKW] = {
        .name = "system_process_io_diskw_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of bytes really sent to the storage layer.",
    },
    [FAM_PROC_IO_CANCELLED_DISKW] = {
        .name = "system_process_io_cancelled_diskw_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of bytes \"saved\" from I/O writeback.",
    },
    [FAM_PROC_FILE_HANDLES] = {
        .name = "system_process_file_handles",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of the currently open file handles.",
    },
    [FAM_PROC_MEMORY_MAPPED_REGIONS] = {
        .name = "system_process_memory_mapped_regions",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of the currently mapped memory regions.",
    },
    [FAM_PROC_CTX_VOLUNTARY] = {
        .name = "system_process_contextswitch_voluntary",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of voluntary context switches.",
    },
    [FAM_PROC_CTX_INVOLUNTARY] = {
        .name = "system_process_contextswitch_involuntary",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of involuntary context switches.",
    },
    [FAM_PROC_DELAY_CPU] = {
        .name = "system_process_delay_cpu_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Delay waiting for cpu in seconds, while runnable count.",
    },
    [FAM_PROC_DELAY_BLKIO] = {
        .name = "system_process_delay_blkio_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Delay waiting for synchronous block I/O to complete in seconds "
                "does not account for delays in I/O submission.",
    },
    [FAM_PROC_DELAY_SWAPIN] = {
        .name = "system_process_delay_swapin_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Delay waiting for page fault I/O in seconds (swap in only).",
    },
    [FAM_PROC_DELAY_FREEPAGES] = {
        .name = "system_process_delay_freepages_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Delay waiting for memory reclaim in seconds.",
    },
    [FAM_PROC_DELAY_IRQ] = {
        .name = "system_process_delay_irq_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Delay waiting for IRQ/SOFTIRQ.",
    },
    [FAM_PROC_DELAY_THRASHING] = {
        .name = "system_process_delay_thrashing_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Delay waiting for thrashing page.",
    },
    [FAM_PROC_DELAY_COMPACT] = {
        .name = "system_process_delay_compact_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Delay waiting for memory compact.",
    },
    [FAM_PROC_DELAY_WPCOPY] = {
        .name = "system_process_delay_wpcopy_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Delay waiting for write-protect copy.",
    },
    [FAM_PROC_SCHED_RUNNING] = {
        .name = "system_process_sched_running_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Time spent on the cpu in seconds.",
    },
    [FAM_PROC_SCHED_WAITING] = {
        .name = "system_process_sched_waiting_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Time spent waiting on a runqueue in seconds.",
    },
    [FAM_PROC_SCHED_TIMESLICES] = {
        .name = "system_process_sched_timeslices",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of timeslices run on the cpu.",
    },
};

static cf_flags_t processes_flags[] = {
    { "file_descriptors",  COLLECT_FILE_DESCRIPTORS  },
    { "memory_maps",       COLLECT_MEMORY_MAPS       },
    { "delay_accounting",  COLLECT_DELAY_ACCOUNTING  }
};

static size_t processes_flags_size = STATIC_ARRAY_SIZE(processes_flags);

static procstat_t *list_head_g = NULL;
static bool want_init = true;

static uint64_t flags;

static char const *proc_state_name[PROC_STATE_MAX] = {
    [PROC_STATE_BLOCKED]  = "blocked",
    [PROC_STATE_DAEMON]   = "daemon",
    [PROC_STATE_DEAD]     = "dead",
    [PROC_STATE_DETACHED] = "detached",
    [PROC_STATE_IDLE]     = "idle",
    [PROC_STATE_ONPROC]   = "onproc",
    [PROC_STATE_ORPHAN]   = "orphan",
    [PROC_STATE_PAGING]   = "paging",
    [PROC_STATE_RUNNING]  = "running",
    [PROC_STATE_SLEEPING] = "sleeping",
    [PROC_STATE_STOPPED]  = "stopped",
    [PROC_STATE_SYSTEM]   = "system",
    [PROC_STATE_WAIT]     = "wait",
    [PROC_STATE_ZOMBIES]  = "zombies",
    [PROC_STATE_TRACED]   = "traced"
};

/* put name of process from config to list_head_g tree
 * list_head_g is a list of 'procstat_t' structs with
 * processes names we want to watch */
static procstat_t *ps_list_register(const char *name, const char *regexp, const char *pid_file)
{

    procstat_t *new = calloc(1, sizeof(*new));
    if (new == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return NULL;
    }
    sstrncpy(new->name, name, sizeof(new->name));

    new->io_rchar = -1;
    new->io_wchar = -1;
    new->io_syscr = -1;
    new->io_syscw = -1;
    new->io_diskr = -1;
    new->io_diskw = -1;
    new->io_cancelled_diskw = -1;
    new->cswitch_vol = -1;
    new->cswitch_invol = -1;
    new->sched_running = -1;
    new->sched_waiting = -1;
    new->sched_timeslices = -1;

    new->flags = flags;
    memcpy(new->fams, fams_proc, sizeof(new->fams[0])*FAM_PROC_MAX);

    if (regexp != NULL) {
        PLUGIN_DEBUG("process-match: adding \"%s\" as criteria to process %s.", regexp, name);
        new->re = malloc(sizeof(*new->re));
        if (new->re == NULL) {
            PLUGIN_ERROR("ps_list_register: malloc failed.");
            free(new);
            return NULL;
        }

        int status = regcomp(new->re, regexp, REG_EXTENDED | REG_NOSUB);
        if (status != 0) {
            PLUGIN_ERROR("process-match: compiling the regular expression \"%s\" failed.", regexp);
            free(new->re);
            free(new);
            return NULL;
        }
    } else if (pid_file != NULL) {
        new->pid_file = strdup(pid_file);
        if (new->pid_file == NULL) {
            PLUGIN_ERROR("strdup failed.");
            free(new->re);
            free(new);
            return NULL;
        }
    }

    procstat_t *ptr;
    for (ptr = list_head_g; ptr != NULL; ptr = ptr->next) {
        if (strcmp(ptr->name, name) == 0) {
            PLUGIN_WARNING("You have configured more than one `Process' or 'ProcessMatch' "
                           "with the same name. All but the first setting will be ignored.");
            free(new->re);
            free(new->pid_file);
            free(new);
            return NULL;
        }

        if (ptr->next == NULL)
            break;
    }

    if (ptr == NULL)
        list_head_g = new;
    else
        ptr->next = new;

    return new;
}

/* try to match name against entry, returns 1 if success */
static int ps_list_match(const char *name, const char *cmdline, unsigned long pid, procstat_t *ps)
{
    if (ps->re != NULL) {
        const char *str = cmdline;
        if ((str == NULL) || (str[0] == 0))
            str = name;

        assert(str != NULL);

        int status = regexec(ps->re, str, 0, NULL, 0);
        if (status == 0)
            return 1;
    } else if (ps->pid_file != NULL) {
        if (ps->pid == pid)
            return 1;
    } else if (strcmp(ps->name, name) == 0) {
        return 1;
    }

    return 0;
}

static void ps_update_counter(int64_t *group_counter, int64_t *curr_counter, int64_t new_counter)
{
    if (want_init) {
        *curr_counter = new_counter;
        *group_counter = new_counter;
        return;
    }

    unsigned long curr_value;
    if (new_counter < *curr_counter)
        curr_value = new_counter + (ULONG_MAX - *curr_counter);
    else
        curr_value = new_counter - *curr_counter;

    if (*group_counter == -1)
        *group_counter = 0;

    *curr_counter = new_counter;
    *group_counter += curr_value;
}

__attribute__(( weak ))
void ps_fill_details(__attribute__((unused)) procstat_t *ps,
                     __attribute__((unused)) process_entry_t *entry)
{
    return;
}

/* add process entry to 'instances' of process 'name' (or refresh it) */
void ps_list_add(const char *name, const char *cmdline, unsigned long pid, process_entry_t *entry)
{
    procstat_entry_t *pse;

    if (entry->id == 0)
        return;

    for (procstat_t *ps = list_head_g; ps != NULL; ps = ps->next) {
        if ((ps_list_match(name, cmdline, pid, ps)) == 0)
            continue;

        ps_fill_details(ps, entry);

        for (pse = ps->instances; pse != NULL; pse = pse->next)
            if ((pse->id == entry->id) || (pse->next == NULL))
                break;

        if ((pse == NULL) || (pse->id != entry->id) || (pse->starttime != entry->starttime)) {
            if ((pse != NULL) && (pse->id == entry->id)) {
                PLUGIN_WARNING("pid %lu reused between two reads, ignoring existing "
                               "procstat_entry for %s", pse->id, name);
            }

            procstat_entry_t *new = calloc(1, sizeof(*new));
            if (new == NULL)
                return;

            new->id = entry->id;
            new->starttime = entry->starttime;

            if (pse == NULL)
                ps->instances = new;
            else
                pse->next = new;

            pse = new;
        }

        pse->age = 0;

        ps->num_proc += entry->num_proc;
        ps->num_lwp += entry->num_lwp;
        ps->num_fd += entry->num_fd;
        ps->num_maps += entry->num_maps;
        ps->vmem_size += entry->vmem_size;
        ps->vmem_rss += entry->vmem_rss;
        ps->vmem_data += entry->vmem_data;
        ps->vmem_code += entry->vmem_code;
        ps->stack_size += entry->stack_size;

        if ((entry->io_rchar != -1) && (entry->io_wchar != -1)) {
            ps_update_counter(&ps->io_rchar, &pse->io_rchar, entry->io_rchar);
            ps_update_counter(&ps->io_wchar, &pse->io_wchar, entry->io_wchar);
        }

        if ((entry->io_syscr != -1) && (entry->io_syscw != -1)) {
            ps_update_counter(&ps->io_syscr, &pse->io_syscr, entry->io_syscr);
            ps_update_counter(&ps->io_syscw, &pse->io_syscw, entry->io_syscw);
        }

        if ((entry->io_diskr != -1) && (entry->io_diskw != -1)) {
            ps_update_counter(&ps->io_diskr, &pse->io_diskr, entry->io_diskr);
            ps_update_counter(&ps->io_diskw, &pse->io_diskw, entry->io_diskw);
        }

        if (entry->io_cancelled_diskw != -1)
            ps_update_counter(&ps->io_cancelled_diskw, &pse->io_cancelled_diskw,
                                                       entry->io_cancelled_diskw);

        if ((entry->cswitch_vol != -1) && (entry->cswitch_invol != -1)) {
            ps_update_counter(&ps->cswitch_vol, &pse->cswitch_vol, entry->cswitch_vol);
            ps_update_counter(&ps->cswitch_invol, &pse->cswitch_invol, entry->cswitch_invol);
        }

        if ((entry->sched_running != -1) && (entry->sched_waiting != -1) &&
                (entry->sched_timeslices != -1)) {
            ps_update_counter(&ps->sched_running, &pse->sched_running, entry->sched_running);
            ps_update_counter(&ps->sched_waiting, &pse->sched_waiting, entry->sched_waiting);
            ps_update_counter(&ps->sched_timeslices, &pse->sched_timeslices, entry->sched_timeslices);
        }

        ps_update_counter(&ps->vmem_minflt_counter, &pse->vmem_minflt_counter, entry->vmem_minflt_counter);
        ps_update_counter(&ps->vmem_majflt_counter, &pse->vmem_majflt_counter, entry->vmem_majflt_counter);

        ps_update_counter(&ps->cpu_user_counter, &pse->cpu_user_counter, entry->cpu_user_counter);
        ps_update_counter(&ps->cpu_system_counter, &pse->cpu_system_counter, entry->cpu_system_counter);

#ifdef HAVE_TASKSTATS
        if (entry->has_delay) {
            if (isnan(ps->delay_cpu))
                ps->delay_cpu = entry->delay.cpu_ns;
            else
                ps->delay_cpu += entry->delay.cpu_ns;

            if (isnan(ps->delay_blkio))
                ps->delay_blkio = entry->delay.blkio_ns;
            else
                ps->delay_blkio += entry->delay.blkio_ns;

            if (isnan(ps->delay_swapin))
                ps->delay_swapin = entry->delay.swapin_ns;
            else
                ps->delay_swapin += entry->delay.swapin_ns;

            if (isnan(ps->delay_freepages))
                ps->delay_freepages = entry->delay.freepages_ns;
            else
                ps->delay_freepages += entry->delay.freepages_ns;

            if (isnan(ps->delay_irq))
                ps->delay_irq = entry->delay.irq_ns;
            else
                ps->delay_irq += entry->delay.irq_ns;

            if (isnan(ps->delay_thrashing))
                ps->delay_thrashing = entry->delay.thrashing_ns;
            else
                ps->delay_thrashing += entry->delay.thrashing_ns;

            if (isnan(ps->delay_compact))
                ps->delay_compact = entry->delay.compact_ns;
            else
                ps->delay_compact += entry->delay.compact_ns;

            if (isnan(ps->delay_wpcopy))
                ps->delay_wpcopy = entry->delay.wpcopy_ns;
            else
                ps->delay_wpcopy += entry->delay.wpcopy_ns;
        }
#endif
    }
}

/* remove old entries from instances of processes in list_head_g */
void ps_list_reset(void)
{
    for (procstat_t *ps = list_head_g; ps != NULL; ps = ps->next) {
        ps->pid = 0;

        ps->num_proc = 0;
        ps->num_lwp = 0;
        ps->num_fd = 0;
        ps->num_maps = 0;
        ps->vmem_size = 0;
        ps->vmem_rss = 0;
        ps->vmem_data = 0;
        ps->vmem_code = 0;
        ps->stack_size = 0;

        ps->delay_cpu = NAN;
        ps->delay_blkio = NAN;
        ps->delay_swapin = NAN;
        ps->delay_freepages = NAN;
        ps->delay_irq = NAN;
        ps->delay_thrashing = NAN;
        ps->delay_compact = NAN;
        ps->delay_wpcopy = NAN;

        procstat_entry_t *pse_prev = NULL;
        procstat_entry_t *pse = ps->instances;
        while (pse != NULL) {
            if (pse->age > 0) {
                PLUGIN_DEBUG("Removing this procstat entry cause it's too old: "
                             "id = %lu; name = %s;", pse->id, ps->name);
                if (pse_prev == NULL) {
                    ps->instances = pse->next;
                    free(pse);
                    pse = ps->instances;
                } else {
                    pse_prev->next = pse->next;
                    free(pse);
                    pse = pse_prev->next;
                }
            } else {
                pse->age = 1;
                pse_prev = pse;
                pse = pse->next;
            }
        }

        if (ps->pid_file != NULL) {
            uint64_t pid = 0;
            int status = filetouint(ps->pid_file, &pid);
            if (status == 0)
                ps->pid = pid;
        }
    }
}

void ps_list_free(void)
{
    procstat_t *ps = list_head_g;
    while (ps != NULL) {
        if (ps->re != NULL) {
            regfree(ps->re);
            free(ps->re);
        }

        free(ps->pid_file);

        procstat_entry_t *pse = ps->instances;
        while(pse != NULL) {
            procstat_entry_t *pse_next = pse->next;
            free(pse);
            pse = pse_next;
        }

        plugin_filter_free(ps->filter);

        procstat_t *ps_next = ps->next;
        free(ps);
        ps = ps_next;
    }
    list_head_g = NULL;
}

static int ps_tune_instance(config_item_t *ci, procstat_t *ps)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *c = ci->children + i;

        if (strcasecmp("collect", c->key) == 0) {
            status = cf_util_get_flags(c, processes_flags, processes_flags_size, &ps->flags);
        } else if (strcasecmp("filter", c->key) == 0) {
            status = plugin_filter_configure(c, &ps->filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          c->key, cf_get_file(c), cf_get_lineno(c));
            status = -1;
        }

        if (status != 0)
            break;
    }

    return status;
}

/* put all pre-defined 'Process' names from config to list_head_g tree */
static int ps_config(config_item_t *ci)
{
#ifdef KERNEL_LINUX
    const size_t max_procname_len = 15;
#elif defined(KERNEL_SOLARIS) || defined(KERNEL_FREEBSD)
    const size_t max_procname_len = MAXCOMLEN - 1;
#endif

    procstat_t *ps;
    int status = 0;

    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *c = ci->children + i;

        if (strcasecmp(c->key, "process") == 0) {
            if ((c->values_num != 1) || (c->values[0].type != CONFIG_TYPE_STRING)) {
                PLUGIN_ERROR("'process' in %s:%d expects exactly one string argument (got %i).",
                             cf_get_file(c), cf_get_lineno(c), c->values_num);
                status = -1;
                break;
            }

#if defined(KERNEL_LINUX) || defined(KERNEL_SOLARIS) || defined(KERNEL_FREEBSD)
            if (strlen(c->values[0].value.string) > max_procname_len) {
                PLUGIN_WARNING("this platform has a %"PRIsz" character limit to process names. "
                                "The 'process \"%s\"' option will not work as expected.",
                                max_procname_len, c->values[0].value.string);
            }
#endif

            ps = ps_list_register(c->values[0].value.string, NULL, NULL);
            if (ps == NULL) {
                status = -1;
                break;
            }

            if (c->children_num != 0)
                status = ps_tune_instance(c, ps);
        } else if (strcasecmp(c->key, "process-match") == 0) {
            if ((c->values_num != 2) || (c->values[0].type != CONFIG_TYPE_STRING) ||
                ((c->values[1].type != CONFIG_TYPE_REGEX) &&
                 (c->values[1].type != CONFIG_TYPE_STRING))) {
                PLUGIN_ERROR("'process-match' in %s:%d needs exactly a string and a regex (got %i).",
                             cf_get_file(c), cf_get_lineno(c), c->values_num);
                status = -1;
                break;
            }

            ps = ps_list_register(c->values[0].value.string, c->values[1].value.string, NULL);
            if (ps == NULL) {
                status = -1;
                break;
            }

            if (c->children_num != 0)
                status = ps_tune_instance(c, ps);
        } else if (strcasecmp(c->key, "process-pidfile") == 0) {
            if ((c->values_num != 2) || (c->values[0].type != CONFIG_TYPE_STRING) ||
                (c->values[1].type != CONFIG_TYPE_STRING)) {
                PLUGIN_ERROR("'process-pidfile' in %s:%d needs exactly two string arguments (got %i).",
                             cf_get_file(c), cf_get_lineno(c), c->values_num);
                status = -1;
                break;
            }
            ps = ps_list_register(c->values[0].value.string, NULL, c->values[1].value.string);
            if (ps == NULL) {
                status = -1;
                break;
            }

            if (c->children_num != 0)
                status = ps_tune_instance(c, ps);
        } else if (strcasecmp("collect", c->key) == 0) {
            status = cf_util_get_flags(c, processes_flags, processes_flags_size, &flags);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                         c->key, cf_get_file(c), cf_get_lineno(c));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        ps_list_free();
        return -1;
    }

    return 0;
}

void ps_submit_ctxt(uint64_t value)
{
    metric_family_append(&fams_processes[FAM_PROCESSES_CTX], VALUE_COUNTER(value), NULL, NULL);
}

void ps_submit_forks(uint64_t value)
{
    metric_family_append(&fams_processes[FAM_PROCESSES_FORKS], VALUE_COUNTER(value), NULL, NULL);
}

/* submit global state (e.g.: qty of zombies, running, etc..) */
void ps_submit_state(double *proc_state)
{
    for (size_t i = 0; i < PROC_STATE_MAX; i++) {
        if (!isnan(proc_state[i])) {
            metric_family_append(&fams_processes[FAM_PROCESSES_STATE],
                                 VALUE_GAUGE(proc_state[i]), NULL,
                                 &(label_pair_const_t){.name="state", .value=proc_state_name[i]},
                                 NULL);
        }
    }
}

/* submit info about specific process (e.g.: memory taken, cpu usage, etc..) */
void ps_metric_append_proc_list(procstat_t *ps)
{
    label_set_t labels = {.num=1, .ptr=(label_pair_t[]){{.name="name", .value=ps->name}}};

    metric_family_append(&ps->fams[FAM_PROC_VMEM_SIZE], VALUE_GAUGE(ps->vmem_size), &labels, NULL);
    metric_family_append(&ps->fams[FAM_PROC_VMEM_RSS], VALUE_GAUGE(ps->vmem_rss), &labels, NULL);
    metric_family_append(&ps->fams[FAM_PROC_VMEM_DATA], VALUE_GAUGE(ps->vmem_data), &labels, NULL);
    metric_family_append(&ps->fams[FAM_PROC_VMEM_CODE], VALUE_GAUGE(ps->vmem_code), &labels, NULL);
    metric_family_append(&ps->fams[FAM_PROC_VMEM_STACK], VALUE_GAUGE(ps->stack_size), &labels, NULL);
    metric_family_append(&ps->fams[FAM_PROC_CPU_USER],
                         VALUE_COUNTER_FLOAT64((double)ps->cpu_user_counter * 1e-6),
                         &labels, NULL);
    metric_family_append(&ps->fams[FAM_PROC_CPU_SYSTEM],
                         VALUE_COUNTER_FLOAT64((double)ps->cpu_system_counter * 1e-6),
                         &labels, NULL);
    metric_family_append(&ps->fams[FAM_PROC_NUM_PROCESSS],
                         VALUE_GAUGE(ps->num_proc), &labels, NULL);
    metric_family_append(&ps->fams[FAM_PROC_NUM_THREADS],
                         VALUE_GAUGE(ps->num_lwp), &labels, NULL);
    metric_family_append(&ps->fams[FAM_PROC_VMEM_MINFLT],
                         VALUE_COUNTER(ps->vmem_minflt_counter), &labels, NULL);
    metric_family_append(&ps->fams[FAM_PROC_VMEM_MAJFLT],
                         VALUE_COUNTER(ps->vmem_majflt_counter), &labels, NULL);

    if (ps->io_rchar != -1)
        metric_family_append(&ps->fams[FAM_PROC_IO_RCHAR], VALUE_COUNTER(ps->io_rchar), &labels, NULL);
    if (ps->io_wchar != -1)
        metric_family_append(&ps->fams[FAM_PROC_IO_WCHAR], VALUE_COUNTER(ps->io_wchar), &labels, NULL);
    if (ps->io_syscr != -1)
        metric_family_append(&ps->fams[FAM_PROC_IO_SYSCR], VALUE_COUNTER(ps->io_syscr), &labels, NULL);
    if (ps->io_syscw != -1)
        metric_family_append(&ps->fams[FAM_PROC_IO_SYSCW], VALUE_COUNTER(ps->io_syscw), &labels, NULL);
    if (ps->io_diskr != -1)
        metric_family_append(&ps->fams[FAM_PROC_IO_DISKR], VALUE_COUNTER(ps->io_diskr), &labels, NULL);
    if (ps->io_diskw != -1)
        metric_family_append(&ps->fams[FAM_PROC_IO_DISKW], VALUE_COUNTER(ps->io_diskw), &labels, NULL);
    if (ps->io_cancelled_diskw != -1)
        metric_family_append(&ps->fams[FAM_PROC_IO_CANCELLED_DISKW],
                             VALUE_COUNTER(ps->io_cancelled_diskw), &labels, NULL);

    if (ps->flags & COLLECT_FILE_DESCRIPTORS)
        metric_family_append(&ps->fams[FAM_PROC_FILE_HANDLES],
                             VALUE_GAUGE(ps->num_fd), &labels, NULL);
    if (ps->flags & COLLECT_MEMORY_MAPS)
        metric_family_append(&ps->fams[FAM_PROC_MEMORY_MAPPED_REGIONS],
                             VALUE_GAUGE(ps->num_maps), &labels, NULL);

    if (ps->cswitch_vol != -1)
        metric_family_append(&ps->fams[FAM_PROC_CTX_VOLUNTARY],
                             VALUE_COUNTER(ps->cswitch_vol), &labels, NULL);
    if (ps->cswitch_invol != -1)
        metric_family_append(&ps->fams[FAM_PROC_CTX_INVOLUNTARY],
                             VALUE_COUNTER(ps->cswitch_invol), &labels, NULL);
    if (ps->sched_running != -1)
        metric_family_append(&ps->fams[FAM_PROC_SCHED_RUNNING],
                             VALUE_COUNTER_FLOAT64((double)ps->sched_running * 1e-9),
                             &labels, NULL);
    if (ps->sched_waiting!= -1)
        metric_family_append(&ps->fams[FAM_PROC_SCHED_WAITING],
                             VALUE_COUNTER_FLOAT64((double)ps->sched_waiting * 1e-9),
                             &labels, NULL);
    if (ps->sched_timeslices!= -1)
        metric_family_append(&ps->fams[FAM_PROC_SCHED_TIMESLICES],
                             VALUE_COUNTER(ps->sched_timeslices), &labels, NULL);

    if (ps->flags & COLLECT_DELAY_ACCOUNTING) {
        /* The ps->delay_* metrics are in nanoseconds. Convert to seconds. */
        double const delay_factor = 1000000000.0;

        if (!isnan(ps->delay_cpu))
            metric_family_append(&ps->fams[FAM_PROC_DELAY_CPU],
                                 VALUE_COUNTER_FLOAT64(ps->delay_cpu / delay_factor),
                                 &labels, NULL);
        if (!isnan(ps->delay_blkio))
            metric_family_append(&ps->fams[FAM_PROC_DELAY_BLKIO],
                                 VALUE_COUNTER_FLOAT64(ps->delay_blkio / delay_factor),
                                 &labels, NULL);
        if (!isnan(ps->delay_swapin))
            metric_family_append(&ps->fams[FAM_PROC_DELAY_SWAPIN],
                                 VALUE_COUNTER_FLOAT64(ps->delay_swapin / delay_factor),
                                 &labels, NULL);
        if (!isnan(ps->delay_freepages))
            metric_family_append(&ps->fams[FAM_PROC_DELAY_FREEPAGES],
                                 VALUE_COUNTER_FLOAT64(ps->delay_freepages / delay_factor),
                                 &labels, NULL);
        if (!isnan(ps->delay_irq))
            metric_family_append(&ps->fams[FAM_PROC_DELAY_IRQ],
                                 VALUE_COUNTER_FLOAT64(ps->delay_irq / delay_factor),
                                 &labels, NULL);
        if (!isnan(ps->delay_thrashing))
            metric_family_append(&ps->fams[FAM_PROC_DELAY_THRASHING],
                                 VALUE_COUNTER_FLOAT64(ps->delay_thrashing / delay_factor),
                                 &labels, NULL);
        if (!isnan(ps->delay_compact))
            metric_family_append(&ps->fams[FAM_PROC_DELAY_COMPACT],
                                 VALUE_COUNTER_FLOAT64(ps->delay_compact / delay_factor),
                                 &labels, NULL);
        if (!isnan(ps->delay_wpcopy))
            metric_family_append(&ps->fams[FAM_PROC_DELAY_WPCOPY],
                                 VALUE_COUNTER_FLOAT64(ps->delay_wpcopy / delay_factor),
                                 &labels, NULL);
    }

    PLUGIN_DEBUG("name = %s; num_proc = %lu; num_lwp = %lu; num_fd = %lu; num_maps = %lu; "
                 "vmem_size = %lu; vmem_rss = %lu; vmem_data = %lu; vmem_code = %lu; "
                 "vmem_minflt_counter = %" PRIi64 "; vmem_majflt_counter = %" PRIi64 "; "
                 "cpu_user_counter = %" PRIi64 "; cpu_system_counter = %" PRIi64 "; "
                 "io_rchar = %" PRIi64 "; io_wchar = %" PRIi64 "; "
                 "io_syscr = %" PRIi64 "; io_syscw = %" PRIi64 "; "
                 "io_diskr = %" PRIi64 "; io_diskw = %" PRIi64 "; "
                 "io_cancelled_diskw = %" PRIi64 "; "
                 "cswitch_vol = %" PRIi64 "; cswitch_invol = %" PRIi64 "; "
                 "sched_running = %" PRIi64 "; sched_waiting = %" PRIi64 "; "
                 "sched_timeslices= %" PRIi64 "; delay_cpu = %g; delay_blkio = %g; "
                 "delay_swapin = %g; delay_freepages = %g; delay_irq = %g; "
                 "delay_thrashing = %g; delay_compact = %g; delay_wpcopy = %g;",
                 ps->name, ps->num_proc, ps->num_lwp, ps->num_fd, ps->num_maps,
                 ps->vmem_size, ps->vmem_rss, ps->vmem_data, ps->vmem_code,
                 ps->vmem_minflt_counter, ps->vmem_majflt_counter,
                 ps->cpu_user_counter, ps->cpu_system_counter,
                 ps->io_rchar, ps->io_wchar, ps->io_syscr, ps->io_syscw,
                 ps->io_diskr, ps->io_diskw, ps->io_cancelled_diskw,
                 ps->cswitch_vol, ps->cswitch_invol,
                 ps->sched_running, ps->sched_waiting, ps->sched_timeslices,
                 ps->delay_cpu, ps->delay_blkio, ps->delay_swapin, ps->delay_freepages,
                 ps->delay_irq, ps->delay_thrashing, ps->delay_compact, ps->delay_wpcopy);

}

void ps_dispatch(void)
{
    cdtime_t ts = cdtime();

    plugin_dispatch_metric_family_array(fams_processes, FAM_PROCESSES_MAX, ts);

    for (procstat_t *ps = list_head_g; ps != NULL; ps = ps->next) {
        ps_metric_append_proc_list(ps);
        plugin_dispatch_metric_family_array_filtered(ps->fams, FAM_PROC_MAX, ps->filter, ts);
    }

    want_init = false;
}

__attribute__(( weak ))
int ps_read(void)
{
    return 0;
}

__attribute__(( weak ))
int ps_init(void)
{
    return 0;
}

__attribute__(( weak ))
int ps_shutdown(void)
{
    ps_list_free();
    return 0;
}

void module_register(void)
{
    plugin_register_config("processes", ps_config);
    plugin_register_init("processes", ps_init);
    plugin_register_read("processes", ps_read);
    plugin_register_shutdown("processes", ps_shutdown);
}
