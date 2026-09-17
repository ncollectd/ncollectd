// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2016 rinigus
// SPDX-FileContributor: rinigus <http://github.com/rinigus>

#include "plugin.h"
#include "libutils/time.h"

static metric_family_t fam = {
    .name = "system_cpusleep_seconds",
    .type = METRIC_TYPE_COUNTER,
    .help = "The relative amount of time in seconds the device has spent in suspend state.",
};

static int cpusleep_read(void)
{
    struct timespec ts_boot;
    if (clock_gettime(CLOCK_BOOTTIME, &ts_boot) < 0) {
        PLUGIN_ERROR("clock_boottime failed");
        return -1;
    }

    struct timespec ts_monotonic;
    if (clock_gettime(CLOCK_MONOTONIC, &ts_monotonic) < 0) {
        PLUGIN_ERROR("clock_monotonic failed");
        return -1;
    }

    double sleep = TIMESPEC_TO_DOUBLE(&ts_boot) - TIMESPEC_TO_DOUBLE(&ts_monotonic);

    metric_family_append(&fam, VALUE_COUNTER_FLOAT64(sleep), NULL, NULL);

    plugin_dispatch_metric_family(&fam, 0);

    return 0;
}

void module_register(void)
{
    plugin_register_read("cpusleep", cpusleep_read);
}
