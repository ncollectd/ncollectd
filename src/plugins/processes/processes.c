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

void ps_fill_details(const procstat_t *ps, process_entry_t *entry);

metric_family_t fams[FAM_PROC_MAX] = {
    [FAM_PROC_VMEM_SIZE] = {
        .name = "system_processes_vmem_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_PROC_VMEM_RSS] = {
        .name = "system_processes_vmem_rss_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_PROC_VMEM_DATA] = {
        .name = "system_processes_vmem_data_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_PROC_VMEM_CODE] = {
        .name = "system_processes_vmem_code_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_PROC_VMEM_STACK] = {
        .name = "system_processes_vmem_stack_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_PROC_CPU_USER] = {
        .name = "system_processes_cpu_user",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROC_CPU_SYSTEM] = {
        .name = "system_processes_cpu_system",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROC_NUM_PROCESSS] = {
        .name = "system_processes_num_processs",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_PROC_NUM_THREADS] = {
        .name = "system_processes_num_threads",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_PROC_VMEM_MINFLT] = {
        .name = "system_processes_vmem_minflt",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROC_VMEM_MAJFLT] = {
        .name = "system_processes_vmem_majflt",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROC_IO_RCHAR] = {
        .name = "system_processes_io_rchar_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROC_IO_WCHAR] = {
        .name = "system_processes_io_wchar_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROC_IO_SYSCR] = {
        .name = "system_processes_io_syscr",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROC_IO_SYSCW] = {
        .name = "system_processes_io_syscw",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROC_IO_DISKR] = {
        .name = "system_processes_io_diskr_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROC_IO_DISKW] = {
        .name = "system_processes_io_diskw_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROC_FILE_HANDLES] = {
        .name = "system_processes_file_handles",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_PROC_FILE_HANDLES_MAPPED] = {
        .name = "system_processes_file_handles_mapped",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_PROC_CTX_VOLUNTARY] = {
        .name = "system_processes_contextswitch_voluntary",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROC_CTX_INVOLUNTARY] = {
        .name = "system_processes_contextswitch_involuntary",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROC_DELAY_CPU] = {
        .name = "system_processes_delay_cpu_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_PROC_DELAY_BLKIO] = {
        .name = "system_processes_delay_blkio_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_PROC_DELAY_SWAPIN] = {
        .name = "system_processes_delay_swapin_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_PROC_DELAY_FREEPAGES] = {
        .name = "system_processes_delay_freepages_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_PROC_SCHED_RUNNING] = {
        .name = "system_processes_sched_running",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROC_SCHED_WAITING] = {
        .name = "system_processes_sched_waiting",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROC_SCHED_TIMESLICES] = {
        .name = "system_processes_sched_timeslices",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
};


procstat_t *list_head_g = NULL;

bool want_init = true;
bool report_ctx_switch = false;
bool report_fd_num = false;
bool report_maps_num = false;
bool report_delay = false;

static char const *proc_state_name[PROC_STATE_MAX] = {
    "blocked", "daemon",  "dead",     "detached", "idle",   "onproc", "orphan",
    "paging",  "running", "sleeping", "stopped",  "system", "wait",   "zombies"
};

/* put name of process from config to list_head_g tree
 * list_head_g is a list of 'procstat_t' structs with
 * processes names we want to watch */
static procstat_t *ps_list_register(const char *name, const char *regexp)
{
    procstat_t *new;
    procstat_t *ptr;
    int status;

    new = calloc(1, sizeof(*new));
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
    new->cswitch_vol = -1;
    new->cswitch_invol = -1;
    new->sched_running = -1;
    new->sched_waiting = -1;
    new->sched_timeslices = -1;

    new->report_fd_num = report_fd_num;
    new->report_maps_num = report_maps_num;
    new->report_ctx_switch = report_ctx_switch;
    new->report_delay = report_delay;

    if (regexp != NULL) {
        PLUGIN_DEBUG("process-match: adding \"%s\" as criteria to process %s.", regexp, name);
        new->re = malloc(sizeof(*new->re));
        if (new->re == NULL) {
            PLUGIN_ERROR("ps_list_register: malloc failed.");
            free(new);
            return NULL;
        }

        status = regcomp(new->re, regexp, REG_EXTENDED | REG_NOSUB);
        if (status != 0) {
            PLUGIN_ERROR("process-match: compiling the regular expression \"%s\" failed.", regexp);
            free(new->re);
            free(new);
            return NULL;
        }
    }

    for (ptr = list_head_g; ptr != NULL; ptr = ptr->next) {
        if (strcmp(ptr->name, name) == 0) {
            PLUGIN_WARNING("You have configured more than one `Process' or 'ProcessMatch' "
                           "with the same name. All but the first setting will be ignored.");
            free(new->re);
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
static int ps_list_match(const char *name, const char *cmdline, procstat_t *ps)
{
    if (ps->re != NULL) {
        const char *str = cmdline;
        if ((str == NULL) || (str[0] == 0))
            str = name;

        assert(str != NULL);

        int status = regexec(ps->re, str, /* nmatch = */ 0,
                                          /* pmatch = */ NULL,
                                          /* eflags = */ 0);
        if (status == 0)
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

#ifdef HAVE_TASKSTATS
static void ps_update_delay_one(double *out_rate_sum, counter_to_rate_state_t *state,
                                uint64_t cnt, cdtime_t t)
{
    double rate = NAN;
    int status = counter_to_rate(&rate, cnt, t, state);
    if ((status != 0) || isnan(rate)) {
        return;
    }

    if (isnan(*out_rate_sum)) {
        *out_rate_sum = rate;
    } else {
        *out_rate_sum += rate;
    }
}

static void ps_update_delay(procstat_t *out, procstat_entry_t *prev, process_entry_t *curr)
{
    cdtime_t now = cdtime();

    ps_update_delay_one(&out->delay_cpu, &prev->delay_cpu, curr->delay.cpu_ns, now);
    ps_update_delay_one(&out->delay_blkio, &prev->delay_blkio, curr->delay.blkio_ns, now);
    ps_update_delay_one(&out->delay_swapin, &prev->delay_swapin, curr->delay.swapin_ns, now);
    ps_update_delay_one(&out->delay_freepages, &prev->delay_freepages, curr->delay.freepages_ns, now);
}
#endif

/* add process entry to 'instances' of process 'name' (or refresh it) */
void ps_list_add(const char *name, const char *cmdline, process_entry_t *entry)
{
    procstat_entry_t *pse;

    if (entry->id == 0)
        return;

    for (procstat_t *ps = list_head_g; ps != NULL; ps = ps->next) {
        if ((ps_list_match(name, cmdline, ps)) == 0)
            continue;

#ifdef KERNEL_LINUX
        ps_fill_details(ps, entry);
#endif

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
        if (entry->has_delay)
            ps_update_delay(ps, pse, entry);
#endif
    }
}

/* remove old entries from instances of processes in list_head_g */
void ps_list_reset(void)
{

    for (procstat_t *ps = list_head_g; ps != NULL; ps = ps->next) {
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

        procstat_entry_t *pse = ps->instances;
        while(pse != NULL) {
            procstat_entry_t *pse_next = pse->next;
            free(pse);
            pse = pse_next;
        }

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

        if (strcasecmp(c->key, "collect-context-switch") == 0) {
            status = cf_util_get_boolean(c, &ps->report_ctx_switch);
        } else if (strcasecmp(c->key, "collect-file-descriptor") == 0) {
            status = cf_util_get_boolean(c, &ps->report_fd_num);
        } else if (strcasecmp(c->key, "collect-memory-maps") == 0) {
            status = cf_util_get_boolean(c, &ps->report_maps_num);
        } else if (strcasecmp(c->key, "collect-delay-accounting") == 0) {
#ifdef HAVE_TASKSTATS
            status = cf_util_get_boolean(c, &ps->report_delay);
#else
            PLUGIN_WARNING("The plugin has been compiled without support "
                            "for the 'collect-delay-accounting' option.");
#endif
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
            if ((c->values_num != 1) || (CONFIG_TYPE_STRING != c->values[0].type)) {
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

            ps = ps_list_register(c->values[0].value.string, NULL);
            if (ps == NULL) {
                status = -1;
                break;
            }

            if (c->children_num != 0)
                status = ps_tune_instance(c, ps);
        } else if (strcasecmp(c->key, "process-match") == 0) {
            if ((c->values_num != 2) || (CONFIG_TYPE_STRING != c->values[0].type) ||
                (CONFIG_TYPE_STRING != c->values[1].type)) {
                PLUGIN_ERROR("'process-match' in %s:%d needs exactly two string arguments (got %i).",
                             cf_get_file(c), cf_get_lineno(c), c->values_num);
                status = -1;
                break;
            }

            ps = ps_list_register(c->values[0].value.string, c->values[1].value.string);
            if (ps == NULL) {
                status = -1;
                break;
            }

            if (c->children_num != 0)
                status = ps_tune_instance(c, ps);
        } else if (strcasecmp(c->key, "collect-context-switch") == 0) {
            status = cf_util_get_boolean(c, &report_ctx_switch);
        } else if (strcasecmp(c->key, "collect-file-descriptor") == 0) {
            status = cf_util_get_boolean(c, &report_fd_num);
        } else if (strcasecmp(c->key, "collect-memory-maps") == 0) {
            status = cf_util_get_boolean(c, &report_maps_num);
        } else if (strcasecmp(c->key, "collect-delay-accounting") == 0) {
#ifdef HAVE_TASKSTATS
            status = cf_util_get_boolean(c, &report_delay);
#else
            PLUGIN_WARNING("The plugin has been compiled without support "
                           "for the 'collect-delay-accounting' option.");
#endif
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
    metric_family_t fam = {
            .name = "system_processes_contextswitch",
            .type = METRIC_TYPE_COUNTER,
            .help = "Total number of context switches across all CPUs."
    };
    metric_family_append(&fam, VALUE_COUNTER(value), NULL, NULL);
    plugin_dispatch_metric_family(&fam, 0);
}

void ps_submit_forks(uint64_t value)
{
    metric_family_t fam = {
            .name = "system_processes_forks",
            .type = METRIC_TYPE_COUNTER,
            .help = "Total number of processes and threads created."
    };
    metric_family_append(&fam, VALUE_COUNTER(value), NULL, NULL);
    plugin_dispatch_metric_family(&fam, 0);
}

/* submit global state (e.g.: qty of zombies, running, etc..) */
void ps_submit_state(double *proc_state)
{
    metric_family_t fam = {
            .name = "system_processes_state",
            .type = METRIC_TYPE_GAUGE,
    };

    for (size_t i = 0; i < PROC_STATE_MAX; i++) {
        if (!isnan(proc_state[i])) {
            metric_family_append(&fam, VALUE_GAUGE(proc_state[i]), NULL,
                            &(label_pair_const_t){.name="state", .value=proc_state_name[i]}, NULL);
        }
    }

    plugin_dispatch_metric_family(&fam, 0);
}

/* submit info about specific process (e.g.: memory taken, cpu usage, etc..) */
void ps_metric_append_proc_list(procstat_t *ps)
{
    label_set_t labels = {.num=1, .ptr=(label_pair_t[]){{.name="name", .value=ps->name}}};

    metric_family_append(&fams[FAM_PROC_VMEM_SIZE], VALUE_GAUGE(ps->vmem_size), &labels, NULL);
    metric_family_append(&fams[FAM_PROC_VMEM_RSS], VALUE_GAUGE(ps->vmem_rss), &labels, NULL);
    metric_family_append(&fams[FAM_PROC_VMEM_DATA], VALUE_GAUGE(ps->vmem_data), &labels, NULL);
    metric_family_append(&fams[FAM_PROC_VMEM_CODE], VALUE_GAUGE(ps->vmem_code), &labels, NULL);
    metric_family_append(&fams[FAM_PROC_VMEM_STACK], VALUE_GAUGE(ps->stack_size), &labels, NULL);
    metric_family_append(&fams[FAM_PROC_CPU_USER],
                         VALUE_COUNTER(ps->cpu_user_counter), &labels, NULL);
    metric_family_append(&fams[FAM_PROC_CPU_SYSTEM],
                         VALUE_COUNTER(ps->cpu_system_counter), &labels, NULL);
    metric_family_append(&fams[FAM_PROC_NUM_PROCESSS],
                         VALUE_GAUGE(ps->num_proc), &labels, NULL);
    metric_family_append(&fams[FAM_PROC_NUM_THREADS],
                         VALUE_GAUGE(ps->num_lwp), &labels, NULL);
    metric_family_append(&fams[FAM_PROC_VMEM_MINFLT],
                         VALUE_COUNTER(ps->vmem_minflt_counter), &labels, NULL);
    metric_family_append(&fams[FAM_PROC_VMEM_MAJFLT],
                         VALUE_COUNTER(ps->vmem_majflt_counter), &labels, NULL);

    if (ps->io_rchar != -1)
        metric_family_append(&fams[FAM_PROC_IO_RCHAR], VALUE_COUNTER(ps->io_rchar), &labels, NULL);
    if (ps->io_wchar != -1)
        metric_family_append(&fams[FAM_PROC_IO_WCHAR], VALUE_COUNTER(ps->io_wchar), &labels, NULL);
    if (ps->io_syscr != -1)
        metric_family_append(&fams[FAM_PROC_IO_SYSCR], VALUE_COUNTER(ps->io_syscr), &labels, NULL);
    if (ps->io_syscw != -1)
        metric_family_append(&fams[FAM_PROC_IO_SYSCW], VALUE_COUNTER(ps->io_syscw), &labels, NULL);
    if (ps->io_diskr != -1)
        metric_family_append(&fams[FAM_PROC_IO_DISKR], VALUE_COUNTER(ps->io_diskr), &labels, NULL);
    if (ps->io_diskw != -1)
        metric_family_append(&fams[FAM_PROC_IO_DISKW], VALUE_COUNTER(ps->io_diskw), &labels, NULL);
    if (ps->num_fd > 0)
        metric_family_append(&fams[FAM_PROC_FILE_HANDLES], VALUE_GAUGE(ps->num_fd), &labels, NULL);
    if (ps->num_maps > 0)
        metric_family_append(&fams[FAM_PROC_FILE_HANDLES_MAPPED],
                             VALUE_GAUGE(ps->num_maps), &labels, NULL);
    if (ps->cswitch_vol != -1)
        metric_family_append(&fams[FAM_PROC_CTX_VOLUNTARY],
                             VALUE_COUNTER(ps->cswitch_vol), &labels, NULL);
    if (ps->cswitch_invol != -1)
        metric_family_append(&fams[FAM_PROC_CTX_INVOLUNTARY],
                             VALUE_COUNTER(ps->cswitch_invol), &labels, NULL);
    if (ps->sched_running != -1)
        metric_family_append(&fams[FAM_PROC_SCHED_RUNNING],
                             VALUE_COUNTER(ps->sched_running), &labels, NULL);
    if (ps->sched_waiting!= -1)
        metric_family_append(&fams[FAM_PROC_SCHED_WAITING],
                             VALUE_COUNTER(ps->sched_waiting), &labels, NULL);
    if (ps->sched_timeslices!= -1)
        metric_family_append(&fams[FAM_PROC_SCHED_TIMESLICES],
                             VALUE_COUNTER(ps->sched_timeslices), &labels, NULL);

    /* The ps->delay_* metrics are in nanoseconds per second. Convert to seconds
     * per second. */
    double const delay_factor = 1000000000.0;

    if (!isnan(ps->delay_cpu))
        metric_family_append(&fams[FAM_PROC_DELAY_CPU],
                             VALUE_GAUGE(ps->delay_cpu / delay_factor), &labels, NULL);
    if (!isnan(ps->delay_blkio))
        metric_family_append(&fams[FAM_PROC_DELAY_BLKIO],
                             VALUE_GAUGE(ps->delay_blkio / delay_factor), &labels, NULL);
    if (!isnan(ps->delay_swapin))
        metric_family_append(&fams[FAM_PROC_DELAY_SWAPIN],
                             VALUE_GAUGE(ps->delay_swapin / delay_factor), &labels, NULL);
    if (!isnan(ps->delay_freepages))
        metric_family_append(&fams[FAM_PROC_DELAY_FREEPAGES],
                             VALUE_GAUGE(ps->delay_freepages / delay_factor), &labels, NULL);

    PLUGIN_DEBUG("name = %s; num_proc = %lu; num_lwp = %lu; num_fd = %lu; num_maps = %lu; "
                 "vmem_size = %lu; vmem_rss = %lu; vmem_data = %lu; vmem_code = %lu; "
                 "vmem_minflt_counter = %" PRIi64 "; vmem_majflt_counter = %" PRIi64 "; "
                 "cpu_user_counter = %" PRIi64 "; cpu_system_counter = %" PRIi64 "; "
                 "io_rchar = %" PRIi64 "; io_wchar = %" PRIi64 "; "
                 "io_syscr = %" PRIi64 "; io_syscw = %" PRIi64 "; "
                 "io_diskr = %" PRIi64 "; io_diskw = %" PRIi64 "; "
                 "cswitch_vol = %" PRIi64 "; cswitch_invol = %" PRIi64 "; "
                 "sched_running = %" PRIi64 "; sched_waiting = %" PRIi64 "; "
                 "sched_timeslices= %" PRIi64 "; delay_cpu = %g; delay_blkio = %g; "
                 "delay_swapin = %g; delay_freepages = %g;",
                 ps->name, ps->num_proc, ps->num_lwp, ps->num_fd, ps->num_maps,
                 ps->vmem_size, ps->vmem_rss, ps->vmem_data, ps->vmem_code,
                 ps->vmem_minflt_counter, ps->vmem_majflt_counter,
                 ps->cpu_user_counter, ps->cpu_system_counter,
                 ps->io_rchar, ps->io_wchar, ps->io_syscr, ps->io_syscw, ps->io_diskr, ps->io_diskw,
                 ps->cswitch_vol, ps->cswitch_invol,
                 ps->sched_running, ps->sched_waiting, ps->sched_timeslices,
                 ps->delay_cpu, ps->delay_blkio, ps->delay_swapin, ps->delay_freepages);

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
