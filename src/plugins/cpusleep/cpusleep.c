// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2016 rinigus
// SPDX-FileContributor: rinigus <http://github.com/rinigus>

/*
 * CPU sleep is reported in milliseconds of sleep per second of wall
 * time. For that, the time difference between BOOT and MONOTONIC clocks
 * is reported using derive type.
 */

#include "plugin.h"

#include <time.h>

static metric_family_t fam = {
    .name = "system_cpusleep_seconds",
    .type = METRIC_TYPE_COUNTER,
    .help = "The relative amount of time in seconds the device has spent in suspend state.",
};

static int cpusleep_read(void)
{
    struct timespec b;
    if (clock_gettime(CLOCK_BOOTTIME, &b) < 0) {
        PLUGIN_ERROR("clock_boottime failed");
        return -1;
    }

    struct timespec m;
    if (clock_gettime(CLOCK_MONOTONIC, &m) < 0) {
        PLUGIN_ERROR("clock_monotonic failed");
        return -1;
    }

    // to avoid false positives in counter overflow due to reboot,
    // derive is used. Sleep is calculated in milliseconds
    double diffsec = b.tv_sec - m.tv_sec;
    double diffnsec = b.tv_nsec - m.tv_nsec;
    double sleep = diffsec + diffnsec / 100000000.0;

    metric_family_append(&fam, VALUE_COUNTER_FLOAT64(sleep), NULL, NULL);

    plugin_dispatch_metric_family(&fam, 0);

    return 0;
}

void module_register(void)
{
    plugin_register_read("cpusleep", cpusleep_read);
}
