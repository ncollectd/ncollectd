// SPDX-License-Identifier: GPL-2.0-only OR MIT
// Copyright (C) 2016 rinigus
// Authors:
//	 rinigus <http://github.com/rinigus>

/*
 * CPU sleep is reported in milliseconds of sleep per second of wall
 * time. For that, the time difference between BOOT and MONOTONIC clocks
 * is reported using derive type.
 */

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"
#include <time.h>

static void cpusleep_submit(uint64_t cpu_sleep)
{
  metric_family_t fam = {
    .name = "host_cpusleep_milliseconds_total",
    .type = METRIC_TYPE_COUNTER,
  };

  metric_family_metric_append(&fam, (metric_t){ .value.counter.uint64 = cpu_sleep, });

  int status = plugin_dispatch_metric_family(&fam);
  if (status != 0)
    ERROR("cpusleep plugin: plugin_dispatch_metric_family failed: %s", STRERROR(status));

  metric_family_metric_reset(&fam);
}

static int cpusleep_read(void)
{
  struct timespec b, m;
  if (clock_gettime(CLOCK_BOOTTIME, &b) < 0) {
    ERROR("cpusleep plugin: clock_boottime failed");
    return -1;
  }

  if (clock_gettime(CLOCK_MONOTONIC, &m) < 0) {
    ERROR("cpusleep plugin: clock_monotonic failed");
    return -1;
  }

  // to avoid false positives in counter overflow due to reboot,
  // derive is used. Sleep is calculated in milliseconds
  int64_t diffsec = b.tv_sec - m.tv_sec;
  int64_t diffnsec = b.tv_nsec - m.tv_nsec;
  uint64_t sleep = diffsec * 1000 + diffnsec / 1000000;

  cpusleep_submit(sleep);

  return 0;
}

void module_register(void)
{
  plugin_register_read("cpusleep", cpusleep_read);
}
