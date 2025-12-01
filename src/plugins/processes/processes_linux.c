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
#include "libutils/complain.h"

#include "processes.h"

#ifdef HAVE_LINUX_CONFIG_H
#include <linux/config.h>
#endif
#ifndef CONFIG_HZ
#define CONFIG_HZ 100
#endif

#include <regex.h>

#ifdef HAVE_SYS_CAPABILITY_H
#include <sys/capability.h>
#endif

static ts_t *taskstats_handle;

static long pagesize_g;

static char *path_proc;
static char *path_proc_stat;

void ps_fill_details(procstat_t *ps, process_entry_t *entry);

static int ps_read_schedstat(process_entry_t *ps)
{
    char filename[PATH_MAX];
    snprintf(filename, sizeof(filename), "%s/%lu/schedstat", path_proc, ps->id);

    char buffer[128];
    ssize_t status = read_text_file_contents(filename, buffer, sizeof(buffer) - 1);
    if (status <= 0)
        return -1;

    char *fields[4];
    int numfields = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));

    if (numfields < 3)
     return -1;

    ps->sched_running = strtoul(fields[0], NULL, /* base = */ 10);
    ps->sched_waiting = strtoul(fields[1], NULL, /* base = */ 10);
    ps->sched_timeslices = strtoul(fields[2], NULL, /* base = */ 10);

    return 0;
}

/* Read data from /proc/pid/status */
static int ps_read_status(process_entry_t *ps)
{
    char filename[PATH_MAX];
    snprintf(filename, sizeof(filename), "%s/%lu/status", path_proc, ps->id);

    FILE *fh = fopen(filename, "r");
    if (fh == NULL)
        return -1;

    unsigned long lib = 0;
    unsigned long exe = 0;
    unsigned long data = 0;
    unsigned long threads = 0;
    unsigned long cswitch_vol= 0;
    unsigned long cswitch_invol= 0;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        unsigned long *tmp;

        if (strncmp(buffer, "VmData", 6) == 0) {
            tmp = &data;
        } else if (strncmp(buffer, "VmLib", 5) == 0) {
            tmp = &lib;
        } else if (strncmp(buffer, "VmExe", 5) == 0) {
            tmp = &exe;
        } else if (strncmp(buffer, "Threads", 7) == 0) {
            tmp = &threads;
        } else if (strncmp(buffer, "voluntary_ctxt_switches", 23) == 0) {
            tmp = &cswitch_vol;
        } else if (strncmp(buffer, "nonvoluntary_ctxt_switches", 26) == 0) {
            tmp = &cswitch_invol;
        } else {
            continue;
        }

        char *fields[8];
        int numfields = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (numfields < 2)
            continue;

        errno = 0;
        char *endptr = NULL;
        unsigned long value = strtoul(fields[1], &endptr, /* base = */ 10);
        if ((errno == 0) && (endptr != fields[1])) {
            *tmp = value;
        }
    }

    if (fclose(fh))
        PLUGIN_WARNING("fclose: %s", STRERRNO);

    ps->vmem_data = data * 1024;
    ps->vmem_code = (exe + lib) * 1024;
    ps->cswitch_vol = cswitch_vol;
    ps->cswitch_invol = cswitch_invol;
    if (threads != 0)
        ps->num_lwp = threads;

    return 0;
}

static int ps_read_io(process_entry_t *ps)
{
    char filename[PATH_MAX];
    ssnprintf(filename, sizeof(filename), "%s/%lu/io", path_proc, ps->id);

    FILE *fh = fopen(filename, "r");
    if (fh == NULL) {
        PLUGIN_DEBUG("Failed to open file `%s'", filename);
        return -1;
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        int64_t *val = NULL;

        if (strncasecmp(buffer, "rchar:", 6) == 0)
            val = &(ps->io_rchar);
        else if (strncasecmp(buffer, "wchar:", 6) == 0)
            val = &(ps->io_wchar);
        else if (strncasecmp(buffer, "syscr:", 6) == 0)
            val = &(ps->io_syscr);
        else if (strncasecmp(buffer, "syscw:", 6) == 0)
            val = &(ps->io_syscw);
        else if (strncasecmp(buffer, "read_bytes:", 11) == 0)
            val = &(ps->io_diskr);
        else if (strncasecmp(buffer, "write_bytes:", 12) == 0)
            val = &(ps->io_diskw);
        else if (strncasecmp(buffer, "cancelled_write_bytes:", 22) == 0)
            val = &(ps->io_cancelled_diskw);
        else
            continue;

        char *fields[8];
        int numfields = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (numfields < 2)
            continue;

        errno = 0;
        char *endptr = NULL;
        long long tmp = strtoll(fields[1], &endptr, /* base = */ 10);
        if ((errno != 0) || (endptr == fields[1]))
            *val = -1;
        else
            *val = tmp;
    }

    if (fclose(fh))
        PLUGIN_WARNING("fclose: %s", STRERRNO);

    return 0;
}

static int ps_count_maps(pid_t pid)
{
    char filename[PATH_MAX];
    ssnprintf(filename, sizeof(filename), "%s/%d/maps", path_proc, pid);

    FILE *fh = fopen(filename, "r");
    if (fh == NULL) {
        PLUGIN_DEBUG("Failed to open file `%s'", filename);
        return -1;
    }

    int count = 0;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        if (strchr(buffer, '\n')) {
            count++;
        }
    }

    if (fclose(fh))
        PLUGIN_WARNING("fclose: %s", STRERRNO);

    return count;
}

static int ps_count_fd(int pid)
{
    char dirname[PATH_MAX];
    ssnprintf(dirname, sizeof(dirname), "%s/%i/fd", path_proc, pid);

    DIR *dh = opendir(dirname);
    if (dh == NULL) {
        DEBUG("Failed to open directory `%s'", dirname);
        return -1;
    }

    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(dh)) != NULL) {
        if (!isdigit((int)ent->d_name[0]))
            continue;
        else
            count++;
    }
    closedir(dh);

    return count;
}

#ifdef HAVE_TASKSTATS
static int ps_delay(process_entry_t *ps)
{
    if (taskstats_handle == NULL)
        return ENOTCONN;

    int status = ts_delay_by_tgid(taskstats_handle, (uint32_t)ps->id, &ps->delay);
    if (status == EPERM) {
        static c_complain_t c;
#if defined(HAVE_SYS_CAPABILITY_H) && defined(CAP_NET_ADMIN)
        if (plugin_check_capability(CAP_NET_ADMIN) != 0) {
            if (getuid() == 0) {
                c_complain(LOG_ERR, &c,
                           "processes plugin: Reading Delay Accounting metric failed: %s. "
                           "ncollectd is running as root, but missing the CAP_NET_ADMIN "
                           "capability. The most common cause for this is that the init "
                           "system is dropping capabilities.",
                           STRERROR(status));
            } else {
                c_complain(LOG_ERR, &c,
                           "processes plugin: Reading Delay Accounting metric failed: %s. "
                           "ncollectd is not running as root and missing the CAP_NET_ADMIN "
                           "capability. Either run ncollectd as root or grant it the "
                           "CAP_NET_ADMIN capability using \"setcap cap_net_admin=ep " PREFIX
                           "/sbin/ncollectd\".",
                           STRERROR(status));
            }
        } else {
            PLUGIN_ERROR("ts_delay_by_tgid failed: %s. The CAP_NET_ADMIN capability "
                         "is available (I checked), so this error is utterly unexpected.",
                          STRERROR(status));
        }
#else
        c_complain(LOG_ERR, &c, "processes plugin: Reading Delay Accounting metric failed: %s. "
                                "Reading Delay Accounting metrics requires root privileges.",
                                STRERROR(status));
#endif
        return status;
    } else if (status != 0) {
        PLUGIN_ERROR("ts_delay_by_tgid failed: %s", STRERROR(status));
        return status;
    }

    return 0;
}
#endif

void ps_fill_details(procstat_t *ps, process_entry_t *entry)
{
    if (entry->state != PROC_STATE_ZOMBIES) {
        if (entry->has_status == false) {
            ps_read_status(entry);
            entry->has_status = true;
        }
    }

    if (entry->has_io == false) {
        ps_read_io(entry);
        entry->has_io = true;
    }

    if (entry->has_sched == false) {
        ps_read_schedstat(entry);
        entry->has_sched = true;
    }

    if (ps->flags & COLLECT_MEMORY_MAPS) {
        if (entry->has_maps == false) {
            int num_maps = ps_count_maps(entry->id);
            if (num_maps >= 0)
                entry->num_maps = num_maps;
            entry->has_maps = true;
        }
    }

    if (ps->flags & COLLECT_FILE_DESCRIPTORS) {
        if (entry->has_fd == false) {
            int num_fd = ps_count_fd(entry->id);
            if (num_fd >= 0)
                entry->num_fd = num_fd;
            entry->has_fd = true;
        }
    }

#ifdef HAVE_TASKSTATS
    if (ps->flags & COLLECT_DELAY_ACCOUNTING) {
        if (entry->has_delay == false) {
            ps_delay(entry);
            entry->has_delay = true;
        }
    }
#endif
}

/* ps_read_process reads process counters on Linux. */
static int ps_read_process(long pid, process_entry_t *ps, char *state)
{
    char filename[PATH_MAX];
    ssnprintf(filename, sizeof(filename), "%s/%li/stat", path_proc, pid);

    char buffer[1024];
    ssize_t status = read_text_file_contents(filename, buffer, sizeof(buffer));
    if (status <= 0)
        return -1;
    size_t buffer_len = (size_t)status;

    /* The name of the process is enclosed in parens. Since the name can
     * contain parens itself, spaces, numbers and pretty much everything
     * else, use these to determine the process name. We don't use
     * strchr(3) and strrchr(3) to avoid pointer arithmetic which would
     * otherwise be required to determine name_len. */
    size_t name_start_pos = 0;
    while (name_start_pos < buffer_len && buffer[name_start_pos] != '(')
        name_start_pos++;

    size_t name_end_pos = buffer_len;
    while (name_end_pos > 0 && buffer[name_end_pos] != ')')
        name_end_pos--;

    /* Either '(' or ')' is not found or they are in the wrong order.
     * Anyway, something weird that shouldn't happen ever. */
    if (name_start_pos >= name_end_pos) {
        PLUGIN_ERROR("name_start_pos = %"PRIsz" >= name_end_pos = %" PRIsz,
                      name_start_pos, name_end_pos);
        return -1;
    }

    size_t name_len = (name_end_pos - name_start_pos) - 1;
    if (name_len >= sizeof(ps->name))
        name_len = sizeof(ps->name) - 1;

    sstrncpy(ps->name, &buffer[name_start_pos + 1], name_len + 1);

    if ((buffer_len - name_end_pos) < 2)
        return -1;
    char *buffer_ptr = &buffer[name_end_pos + 2];

    char *fields[64];
    int fields_len = strsplit(buffer_ptr, fields, STATIC_ARRAY_SIZE(fields));
    if (fields_len < 26) {
        PLUGIN_DEBUG("processes plugin: ps_read_process (pid = %li): '%s' has only %i fields..",
                     pid, filename, fields_len);
        return -1;
    }

    *state = fields[0][0];

    if (*state == 'Z') {
        ps->num_lwp = 0;
        ps->num_proc = 0;
    } else {
        ps->num_lwp = strtoul(fields[17], /* endptr = */ NULL, /* base = */ 10);
        if (ps->num_lwp == 0)
            ps->num_lwp = 1;
        ps->num_proc = 1;
    }

    /* Leave the rest at zero if this is only a zombi */
    if (ps->num_proc == 0) {
        PLUGIN_DEBUG("This is only a zombie: pid = %li; name = %s;", pid, ps->name);
        return 0;
    }

    int64_t cpu_user_counter = atoll(fields[11]);
    int64_t cpu_system_counter = atoll(fields[12]);
    long long unsigned vmem_size = atoll(fields[20]);
    long long unsigned vmem_rss = atoll(fields[21]);
    ps->vmem_minflt_counter = atol(fields[7]);
    ps->vmem_majflt_counter = atol(fields[9]);

    ps->starttime = strtoull(fields[19], NULL, 10);

    unsigned long long stack_start = atoll(fields[25]);
    unsigned long long stack_ptr = atoll(fields[26]);
    long long unsigned stack_size = (stack_start > stack_ptr) ? stack_start - stack_ptr
                                                              : stack_ptr - stack_start;

    /* Convert jiffies to useconds */
    cpu_user_counter = cpu_user_counter * 1000000 / CONFIG_HZ;
    cpu_system_counter = cpu_system_counter * 1000000 / CONFIG_HZ;
    vmem_rss = vmem_rss * pagesize_g;

    ps->cpu_user_counter = cpu_user_counter;
    ps->cpu_system_counter = cpu_system_counter;
    ps->vmem_size = (unsigned long)vmem_size;
    ps->vmem_rss = (unsigned long)vmem_rss;
    ps->stack_size = (unsigned long)stack_size;

    /* no data by default. May be filled by ps_fill_details () */
    ps->io_rchar = -1;
    ps->io_wchar = -1;
    ps->io_syscr = -1;
    ps->io_syscw = -1;
    ps->io_diskr = -1;
    ps->io_diskw = -1;
    ps->io_cancelled_diskw = -1;

    ps->vmem_data = -1;
    ps->vmem_code = -1;
    ps->cswitch_vol = -1;
    ps->cswitch_invol = -1;

    ps->sched_running = -1;
    ps->sched_waiting = -1;
    ps->sched_timeslices = -1;
    /* success */
    return 0;
}

static char *ps_get_cmdline(long pid, char *name, char *buf, size_t buf_len)
{
    if ((pid < 1) || (NULL == buf) || (buf_len < 2))
        return NULL;

    char file[PATH_MAX];
    ssnprintf(file, sizeof(file), "%s/%li/cmdline", path_proc, pid);

    errno = 0;
    int fd = open(file, O_RDONLY);
    if (fd < 0) {
        /* ENOENT means the process exited while we were handling it.
         * Don't complain about this, it only fills the logs. */
        if (errno != ENOENT)
            WARNING("processes plugin: Failed to open `%s': %s.", file, STRERRNO);
        return NULL;
    }

    char *buf_ptr = buf;
    size_t len = buf_len;

    size_t n = 0;

    while (true) {
        ssize_t status = read(fd, (void *)buf_ptr, len);
        if (status < 0) {
            if ((EAGAIN == errno) || (EINTR == errno))
                continue;
            PLUGIN_WARNING("Failed to read from `%s': %s.", file, STRERRNO);
            close(fd);
            return NULL;
        }

        n += status;

        if (status == 0)
            break;

        buf_ptr += status;
        len -= status;

        if (len == 0)
            break;
    }

    close(fd);

    if (n == 0) {
        /* cmdline not available; e.g. kernel thread, zombie */
        if (NULL == name)
            return NULL;

        ssnprintf(buf, buf_len, "[%s]", name);
        return buf;
    }

    assert(n <= buf_len);

    if (n == buf_len)
        --n;
    buf[n] = '\0';

    --n;
    /* remove trailing whitespace */
    while ((n > 0) && (isspace(buf[n]) || ('\0' == buf[n]))) {
        buf[n] = '\0';
        --n;
    }

    /* arguments are separated by '\0' in /proc/<pid>/cmdline */
    while (n > 0) {
        if ('\0' == buf[n])
            buf[n] = ' ';
        --n;
    }
    return buf;
}

static int proc_stat_read(uint64_t *forks, uint64_t *ctx, uint64_t *running)
{
    FILE *fh = fopen(path_proc_stat, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Cannot open '%s': %s", path_proc_stat, STRERRNO);
        return -1;
    }

    char buffer[65536];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *fields[3];
        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (fields_num != 2)
            continue;

        if (strcmp("processes", fields[0]) == 0) {
            uint64_t value = 0;
            int status = parse_uinteger(fields[1], &value);
            if (status == 0)
                *forks = value;
        } else if (strcmp("ctxt", fields[0]) == 0) {
            uint64_t value = 0;
            int status = parse_uinteger(fields[1], &value);
            if (status == 0)
                *ctx = value;
        } else if (strcmp("procs_running", fields[0]) == 0) {
            uint64_t value = 0;
            int status = parse_uinteger(fields[1], &value);
            if (status == 0)
                *running = value;
        }
    }

    fclose(fh);
    return 0;
}

/* do actual readings from kernel */
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
    int traced = 0;
    int dead = 0;
    int idle = 0;

    struct dirent *ent;
    DIR *proc;
    long pid;

    char cmdline[CMDLINE_BUFFER_SIZE];

    process_entry_t pse;

    ps_list_reset();

    if ((proc = opendir(path_proc)) == NULL) {
        PLUGIN_ERROR("Cannot open '%s': %s", path_proc, STRERRNO);
        return -1;
    }

    while ((ent = readdir(proc)) != NULL) {
        if (!isdigit(ent->d_name[0]))
            continue;

        if ((pid = atol(ent->d_name)) < 1)
            continue;

        memset(&pse, 0, sizeof(pse));
        pse.id = pid;

        char state;

        int status = ps_read_process(pid, &pse, &state);
        if (status != 0) {
            DEBUG("ps_read_process failed: %i", status);
            continue;
        }

        switch (state) {
        case 'R':
            running++;
            pse.state = PROC_STATE_RUNNING;
            break;
        case 'S':
            sleeping++;
            pse.state = PROC_STATE_SLEEPING;
            break;
        case 'D':
            blocked++;
            pse.state = PROC_STATE_BLOCKED;
            break;
        case 'Z':
            zombies++;
            pse.state = PROC_STATE_ZOMBIES;
            break;
        case 'T':
            stopped++;
            pse.state = PROC_STATE_STOPPED;
            break;
        case 't':
            traced++;
            pse.state = PROC_STATE_TRACED;
            break;
        case 'X':
            dead++;
            pse.state = PROC_STATE_DEAD;
            break;
        case 'I':
            idle++;
            pse.state = PROC_STATE_IDLE;
            break;
        }

        ps_list_add(pse.name, ps_get_cmdline(pid, pse.name, cmdline, sizeof(cmdline)), pid, &pse);
    }

    closedir(proc);

    /* get procs_running from /proc/stat
     * scanning /proc/stat AND computing other process stats takes too much time.
     * Consequently, the number of running processes based on the occurrences
     * of 'R' as character indicating the running state is typically zero. Due
     * to processes are actually changing state during the evaluation of it's
     * stat(s).
     * The 'procs_running' number in /proc/stat on the other hand is more
     * accurate, and can be retrieved in a single 'read' call. */
    uint64_t stat_forks = 0;
    uint64_t stat_ctxt = 0;
    uint64_t stat_running = 0;

    proc_stat_read(&stat_forks, &stat_ctxt, &stat_running);

    proc_state[PROC_STATE_RUNNING] = stat_running;
    proc_state[PROC_STATE_SLEEPING] = sleeping;
    proc_state[PROC_STATE_ZOMBIES] = zombies;
    proc_state[PROC_STATE_STOPPED] = stopped;
    proc_state[PROC_STATE_BLOCKED] = blocked;
    proc_state[PROC_STATE_TRACED] = traced;
    proc_state[PROC_STATE_DEAD] = dead;
    proc_state[PROC_STATE_IDLE] = idle;
    ps_submit_state(proc_state);

    ps_submit_forks(stat_forks);
    ps_submit_ctxt(stat_ctxt);

    ps_dispatch();

    return 0;
}

int ps_init(void)
{
    path_proc = plugin_procpath(NULL);
    if (path_proc == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    path_proc_stat = plugin_procpath("stat");
    if (path_proc_stat == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    pagesize_g = sysconf(_SC_PAGESIZE);
    PLUGIN_DEBUG("pagesize_g = %li; CONFIG_HZ = %i;", pagesize_g, CONFIG_HZ);

#if HAVE_TASKSTATS
    if (taskstats_handle == NULL) {
        taskstats_handle = ts_create();
        if (taskstats_handle == NULL)
            PLUGIN_WARNING("Creating taskstats handle failed.");
    }
#endif

    return 0;
}

int ps_shutdown(void)
{
    free(path_proc);
    free(path_proc_stat);
    ps_list_free();
    ts_destroy(taskstats_handle);

    return 0;
}
