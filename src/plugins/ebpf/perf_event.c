// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) Tobias Waldekranz
// SPDX-FileContributor: Tobias Waldekranz <tobias at waldekranz.com>

// From https://github.com/wkz/ply

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include "ply.h"
#include "internal.h"

static char tracepath_buf[PATH_MAX] = "/sys/kernel/debug/tracing/";
static pthread_once_t tracepath_once = PTHREAD_ONCE_INIT;

static void tracepath_init(void)
{
    /* tracefs is mounted on /sys/kernel/tracing since Linux 4.1, older
     * systems only provide it below debugfs. */
    char *sys = plugin_syspath(NULL);
    if (sys == NULL)
        return;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/kernel/tracing/events", sys);
    if ((access(path, F_OK) == 0) || (errno != ENOENT))
        snprintf(tracepath_buf, sizeof(tracepath_buf), "%s/kernel/tracing/", sys);
    else
        snprintf(tracepath_buf, sizeof(tracepath_buf), "%s/kernel/debug/tracing/", sys);

    free(sys);
}

/* Path of the tracefs mount point, with a trailing slash. */
const char *tracepath(void)
{
    pthread_once(&tracepath_once, tracepath_init);
    return tracepath_buf;
}

static int perf_event_id(__attribute__((unused)) struct ply_probe *pb, const char *path)
{
    FILE *fp;
    int id;

    fp = fopenf("r", "%s/id", path);
    if (!fp)
        return -errno;

    errno = 0;
    if (fscanf(fp, "%d", &id) != 1) {
        int err = errno ? -errno : -EINVAL;
        fclose(fp);
        return err;
    }

    fclose(fp);
    return id;
}

int perf_event_attach(struct ply_probe *pb, const char *path,
              int task_mode)
{
    struct perf_event_attr attr = {0};
    int fd, id;
    int pid = task_mode ? 0 : -1;
    int cpu = task_mode ? -1 : 0;
    int group_fd = task_mode ? -1 : pb->ply->group_fd;

    id = perf_event_id(pb, path);
    if (id < 0)
        return id;

    attr.type = PERF_TYPE_TRACEPOINT;
    attr.sample_type = PERF_SAMPLE_RAW;
    attr.sample_period = 1;
    attr.wakeup_events = 1;
    attr.config = id;
    attr.disabled = 1;

    fd = perf_event_open(&attr, pid, cpu, group_fd, 0);
    if (fd < 0)
        return -errno;

    if (ioctl(fd, PERF_EVENT_IOC_SET_BPF, pb->bpf_fd)) {
        close(fd);
        return -errno;
    }

    if (pb->ply->group_fd == -1 && !task_mode)
        pb->ply->group_fd = fd;

    return fd;
}

int perf_event_attach_raw(struct ply_probe *pb, int type, unsigned long config,
              unsigned long long period, int task_mode)
{
    struct perf_event_attr attr = {0};
    int fd;
    int pid = task_mode ? 0 : -1;
    int cpu = task_mode ? -1 : 0;
    int group_fd = task_mode ? -1 : pb->ply->group_fd;

    attr.type = type;
    attr.config = config;
    attr.sample_type = PERF_SAMPLE_RAW;
    attr.sample_period = period;
    attr.wakeup_events = 1;
    attr.disabled = 1;

    fd = perf_event_open(&attr, pid, cpu, group_fd, 0);
    if (fd < 0)
        return -errno;

    if (ioctl(fd, PERF_EVENT_IOC_SET_BPF, pb->bpf_fd)) {
        close(fd);
        return -errno;
    }

    if (pb->ply->group_fd == -1 && !task_mode)
        pb->ply->group_fd = fd;

    return fd;
}

int perf_event_attach_profile(struct ply_probe *pb, int cpu,
              unsigned long long freq)
{
    struct perf_event_attr attr = {0};
    int fd;

    attr.type = PERF_TYPE_SOFTWARE;
    attr.config = PERF_COUNT_SW_CPU_CLOCK;
    attr.sample_freq = freq;
    attr.freq = 1;

    fd = perf_event_open(&attr, -1, cpu, -1, 0);
    if (fd < 0)
        return -errno;

    if (ioctl(fd, PERF_EVENT_IOC_SET_BPF, pb->bpf_fd)) {
        close(fd);
        return -errno;
    }

    return fd;
}

int perf_event_enable(int group_fd)
{
    if (ioctl(group_fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP))
        return -errno;

    return 0;
}

int perf_event_disable(int group_fd)
{
    if (ioctl(group_fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP))
        return -errno;

    return 0;
}
