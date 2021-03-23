// SPDX-License-Identifier: GPL-2.0
#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

#if KERNEL_LINUX
#include <sys/timex.h>
/* #endif KERNEL_LINUX */
#else
#error "No applicable input method."
#endif

enum {
  FAM_TIMEX_SYNC_STATUS,
  FAM_TIMEX_OFFSET_SECONDS,
  FAM_TIMEX_FREQUENCY_ADJUSTMENT_RATIO,
  FAM_TIMEX_MAXERROR_SECONDS,
  FAM_TIMEX_ESTIMATED_ERROR_SECONDS,
  FAM_TIMEX_STATUS,
  FAM_TIMEX_LOOP_TIME_CONSTANT,
  FAM_TIMEX_TICK_SECONDS,
  FAM_TIMEX_PPS_FREQUENCY_HERTZ,
  FAM_TIMEX_PPS_JITTER_SECONDS,
  FAM_TIMEX_PPS_SHIFT_SECONDS,
  FAM_TIMEX_PPS_STABILITY_HERTZ,
  FAM_TIMEX_PPS_JITTER_TOTAL,
  FAM_TIMEX_PPS_CALIBRATION_TOTAL,
  FAM_TIMEX_PPS_ERROR_TOTAL,
  FAM_TIMEX_PPS_STABILITY_EXCEEDED_TOTAL,
  FAM_TIMEX_TAI_OFFSET_SECONDS,
  FAM_TIMEX_MAX,
};

static int timex_read(void)
{
  metric_family_t fams[FAM_TIMEX_MAX] = {
    [FAM_TIMEX_SYNC_STATUS] = {
      .name = "host_timex_sync_status",
      .help = "Is clock synchronized to a reliable server (1 = yes, 0 = no).",
      .type = METRIC_TYPE_GAUGE,
    },
    [FAM_TIMEX_OFFSET_SECONDS] = {
      .name = "host_timex_offset_seconds",
      .help = "Time offset in between local system and reference clock.",
      .type = METRIC_TYPE_GAUGE,
    },
    [FAM_TIMEX_FREQUENCY_ADJUSTMENT_RATIO] = {
      .name = "host_timex_frequency_adjustment_ratio",
      .help = "Local clock frequency adjustment.",
      .type = METRIC_TYPE_GAUGE,
    },
    [FAM_TIMEX_MAXERROR_SECONDS] = {
      .name = "host_timex_maxerror_seconds",
      .help = "Maximum error in seconds.",
      .type = METRIC_TYPE_GAUGE,
    },
    [FAM_TIMEX_ESTIMATED_ERROR_SECONDS] = {
      .name = "host_timex_estimated_error_seconds",
      .help = "Estimated error in seconds.",
      .type = METRIC_TYPE_GAUGE,
    },
    [FAM_TIMEX_STATUS] = {
      .name = "host_timex_status",
      .help = "Value of the status array bits.",
      .type = METRIC_TYPE_GAUGE,
    },
    [FAM_TIMEX_LOOP_TIME_CONSTANT] = {
      .name = "host_timex_loop_time_constant",
      .help = "Phase-locked loop time constant.",
      .type = METRIC_TYPE_GAUGE,
    },
    [FAM_TIMEX_TICK_SECONDS] = {
      .name = "host_timex_tick_seconds",
      .help = "Seconds between clock ticks.",
      .type = METRIC_TYPE_GAUGE,
    },
    [FAM_TIMEX_PPS_FREQUENCY_HERTZ] = {
      .name = "host_timex_pps_frequency_hertz",
      .help = "Pulse per second frequency.",
      .type = METRIC_TYPE_GAUGE,
    },
    [FAM_TIMEX_PPS_JITTER_SECONDS] = {
      .name = "host_timex_pps_jitter_seconds",
      .help = "Pulse per second jitter.",
      .type = METRIC_TYPE_GAUGE,
    },
    [FAM_TIMEX_PPS_SHIFT_SECONDS] = {
      .name = "host_timex_pps_shift_seconds",
      .help = "Pulse per second interval duration.",
      .type = METRIC_TYPE_GAUGE,
    },
    [FAM_TIMEX_PPS_STABILITY_HERTZ] = {
      .name = "host_timex_pps_stability_hertz",
      .help = "Pulse per second stability, average of recent frequency changes.",
      .type = METRIC_TYPE_GAUGE,
    },
    [FAM_TIMEX_PPS_JITTER_TOTAL] = {
      .name = "host_timex_pps_jitter_total",
      .help = "Pulse per second count of jitter limit exceeded events.",
      .type = METRIC_TYPE_COUNTER,
    },
    [FAM_TIMEX_PPS_CALIBRATION_TOTAL] = {
      .name = "host_timex_pps_calibration_total",
      .help = "Pulse per second count of calibration intervals.",
      .type = METRIC_TYPE_COUNTER,
    },
    [FAM_TIMEX_PPS_ERROR_TOTAL] = {
      .name = "host_timex_pps_error_total",
      .help = "Pulse per second count of calibration errors.",
      .type = METRIC_TYPE_COUNTER,
    },
    [FAM_TIMEX_PPS_STABILITY_EXCEEDED_TOTAL] = {
      .name = "host_timex_pps_stability_exceeded_total",
      .help = "Pulse per second count of stability limit exceeded events.",
      .type = METRIC_TYPE_COUNTER,
    },
    [FAM_TIMEX_TAI_OFFSET_SECONDS] = {
      .name = "host_timex_tai_offset_seconds",
      .help = "International Atomic Time (TAI) offset.",
      .type = METRIC_TYPE_GAUGE,
    },
  };

  struct timex timex = {0};

  int status = adjtimex(&timex);
  if (status < 0) {
    ERROR("timex plugin: error calling adjtimex : %s", STRERRNO);
    return -1;
  }

  metric_t m = {0};

  m.value.gauge = status == TIME_ERROR ? 0 : 1;
  metric_family_metric_append(&fams[FAM_TIMEX_SYNC_STATUS], m);

  m.value.gauge = (gauge_t)timex.offset / (timex.status & ADJ_NANO ? 1000000000L : 1000000L);
  metric_family_metric_append(&fams[FAM_TIMEX_OFFSET_SECONDS], m);

  m.value.gauge = (gauge_t)timex.freq / 65536000000L;
  metric_family_metric_append(&fams[FAM_TIMEX_FREQUENCY_ADJUSTMENT_RATIO], m);

  m.value.gauge = (gauge_t)timex.maxerror / 1000000L;
  metric_family_metric_append(&fams[FAM_TIMEX_MAXERROR_SECONDS], m);

  m.value.gauge = (gauge_t)timex.esterror / 1000000L;
  metric_family_metric_append(&fams[FAM_TIMEX_ESTIMATED_ERROR_SECONDS], m);

  m.value.gauge = timex.status;
  metric_family_metric_append(&fams[FAM_TIMEX_STATUS], m);
 
  m.value.gauge = timex.constant;
  metric_family_metric_append(&fams[FAM_TIMEX_LOOP_TIME_CONSTANT], m);

  m.value.gauge = (gauge_t)timex.tick / 1000000L;
  metric_family_metric_append(&fams[FAM_TIMEX_TICK_SECONDS], m);

  m.value.gauge = (gauge_t)timex.ppsfreq / 65536000000L;
  metric_family_metric_append(&fams[FAM_TIMEX_PPS_FREQUENCY_HERTZ], m);

  m.value.gauge = (gauge_t)timex.jitter / (timex.status & ADJ_NANO ? 1000000000L : 1000000L);
  metric_family_metric_append(&fams[FAM_TIMEX_PPS_JITTER_SECONDS], m);

  m.value.gauge = timex.shift;
  metric_family_metric_append(&fams[FAM_TIMEX_PPS_SHIFT_SECONDS], m);

  m.value.gauge = (gauge_t)timex.stabil / 65536000000L;
  metric_family_metric_append(&fams[FAM_TIMEX_PPS_STABILITY_HERTZ], m);

  m.value.counter = timex.jitcnt;
  metric_family_metric_append(&fams[FAM_TIMEX_PPS_JITTER_TOTAL], m);

  m.value.counter = timex.calcnt;
  metric_family_metric_append(&fams[FAM_TIMEX_PPS_CALIBRATION_TOTAL], m);

  m.value.counter = timex.errcnt;
  metric_family_metric_append(&fams[FAM_TIMEX_PPS_ERROR_TOTAL], m);

  m.value.counter = timex.stbcnt;
  metric_family_metric_append(&fams[FAM_TIMEX_PPS_STABILITY_EXCEEDED_TOTAL], m);

  m.value.gauge = timex.tai;
  metric_family_metric_append(&fams[FAM_TIMEX_TAI_OFFSET_SECONDS], m);

  for (size_t i = 0; i < FAM_TIMEX_MAX; i++) {
    if (fams[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&fams[i]);
      if (status != 0)
        ERROR("timex plugin: plugin_dispatch_metric_family failed: %s", STRERROR(status));
      metric_family_metric_reset(&fams[i]);
    }
  }

  return 0;
}

void module_register(void)
{
  plugin_register_read("timex", timex_read);
}
