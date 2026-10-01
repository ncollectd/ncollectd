// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2016 rinigus
// SPDX-FileContributor: rinigus <http://github.com/rinigus>

#include "plugin.h"
#include "libutils/time.h"

#include <math.h>

static metric_family_t fam = {
    .name = "system_cpusleep_seconds",
    .type = METRIC_TYPE_COUNTER,
    .help = "Total time in seconds the device has spent in suspend state since boot."
};

static double last_sleep = 0;

static int cpusleep_read(void)
{
    struct timespec ts_monotonic;
    if (clock_gettime(CLOCK_MONOTONIC, &ts_monotonic) < 0) {
        PLUGIN_ERROR("clock_monotonic failed: %s.", STRERRNO);
        return -1;
    }

    struct timespec ts_boot;
    if (clock_gettime(CLOCK_BOOTTIME, &ts_boot) < 0) {
        PLUGIN_ERROR("clock_boottime failed: %s.", STRERRNO);
        return -1;
    }
        
    double sleep = TIMESPEC_TO_DOUBLE(&ts_boot) - TIMESPEC_TO_DOUBLE(&ts_monotonic);
    if (sleep > last_sleep) {
        last_sleep = sleep;   
    } else {
        sleep = last_sleep;
    }

    metric_family_append(&fam, VALUE_COUNTER_FLOAT64(((double)llround(sleep * 1000.0))/1000.0),
                         NULL, NULL);

    plugin_dispatch_metric_family(&fam, 0);

    return 0;
}

void module_register(void)
{
    plugin_register_read("cpusleep", cpusleep_read);
}
