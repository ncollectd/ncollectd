// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#ifdef KERNEL_LINUX
#include <sys/timex.h>
#else
#error "No applicable input method."
#endif

enum {
    FAM_TIMEX_SYNC_STATUS,
    FAM_TIMEX_PLL_OFFSET_SECONDS,
    FAM_TIMEX_PLL_FREQUENCY_PPM,
    FAM_TIMEX_PLL_MAXIMUM_ERROR_SECONDS,
    FAM_TIMEX_PLL_ESTIMATED_ERROR_SECONDS,
    FAM_TIMEX_STATUS,
    FAM_TIMEX_LOOP_TIME_CONSTANT,
    FAM_TIMEX_TICK_SECONDS,
    FAM_TIMEX_PPS_FREQUENCY_PPM,
    FAM_TIMEX_PPS_JITTER_SECONDS,
    FAM_TIMEX_PPS_CALIBRATION_INTERVAL,
    FAM_TIMEX_PPS_STABILITY_PPM,
    FAM_TIMEX_PPS_JITTER_LIMIT,
    FAM_TIMEX_PPS_CALIBRATION_CICLES,
    FAM_TIMEX_PPS_CALIBRATION_ERROR,
    FAM_TIMEX_PPS_STABILITY_EXCEEDED,
    FAM_TIMEX_TAI_OFFSET_SECONDS,
    FAM_TIMEX_MAX,
};

static metric_family_t fams[FAM_TIMEX_MAX] = {
    [FAM_TIMEX_SYNC_STATUS] = {
        .name = "system_timex_sync_status",
        .type = METRIC_TYPE_GAUGE,
        .help = "Is clock synchronized to a reliable server (1 = yes, 0 = no).",
    },
    [FAM_TIMEX_PLL_OFFSET_SECONDS] = {
        .name = "system_timex_pll_offset_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Kernel phase-locked loop offset  between local system "
                "and reference clock in seconds.",
    },
    [FAM_TIMEX_PLL_FREQUENCY_PPM] = {
        .name = "system_timex_pll_frequency_ppm",
        .type = METRIC_TYPE_GAUGE,
        .help = "Kernel phase-locked loop frequency in parts per million.",
    },
    [FAM_TIMEX_PLL_MAXIMUM_ERROR_SECONDS] = {
        .name = "system_timex_pll_maximum_error_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Maximum error for the kernel phase-locked loop in seconds.",
    },
    [FAM_TIMEX_PLL_ESTIMATED_ERROR_SECONDS] = {
        .name = "system_timex_pll_estimated_error_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Estimated error for the kernel phase-locked loop in seconds."
    },
    [FAM_TIMEX_STATUS] = {
        .name = "system_timex_status",
        .type = METRIC_TYPE_GAUGE,
        .help = "Value of the status array bits.",
    },
    [FAM_TIMEX_LOOP_TIME_CONSTANT] = {
        .name = "system_timex_loop_time_constant",
        .type = METRIC_TYPE_GAUGE,
        .help = "Phase-locked loop time constant.",
    },
    [FAM_TIMEX_TICK_SECONDS] = {
        .name = "system_timex_tick_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Seconds between clock ticks.",
    },
    [FAM_TIMEX_PPS_FREQUENCY_PPM] = {
        .name = "system_timex_pps_frequency_ppm",
        .type = METRIC_TYPE_GAUGE,
        .help = "Pulse per second frequency in Parts Per Million.",
    },
    [FAM_TIMEX_PPS_JITTER_SECONDS] = {
        .name = "system_timex_pps_jitter_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Pulse per second jitter in seconds.",
    },
    [FAM_TIMEX_PPS_CALIBRATION_INTERVAL] = {
        .name = "system_timex_pps_calibration_interval_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Pulse per second interval duration.",
    },
    [FAM_TIMEX_PPS_STABILITY_PPM] = {
        .name = "system_timex_pps_stability_ppm",
        .type = METRIC_TYPE_GAUGE,
        .help = "Pulse per second stability in Parts Per Million."
    },
    [FAM_TIMEX_PPS_JITTER_LIMIT] = {
        .name = "system_timex_pps_jitter_limit",
        .type = METRIC_TYPE_COUNTER,
        .help = "Pulse per second count of jitter limit exceeded events.",
    },
    [FAM_TIMEX_PPS_CALIBRATION_CICLES] = {
        .name = "system_timex_pps_calibration_clicles",
        .type = METRIC_TYPE_COUNTER,
        .help = "Pulse per second count of calibration intervals.",
    },
    [FAM_TIMEX_PPS_CALIBRATION_ERROR] = {
        .name = "system_timex_pps_calibration_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Pulse per second count of calibration errors.",
    },
    [FAM_TIMEX_PPS_STABILITY_EXCEEDED] = {
        .name = "system_timex_pps_stability_exceeded",
        .type = METRIC_TYPE_COUNTER,
        .help = "Pulse per second count of stability limit exceeded events.",
    },
    [FAM_TIMEX_TAI_OFFSET_SECONDS] = {
        .name = "system_timex_tai_offset_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "International Atomic Time (TAI) offset.",
    },
};

static int timex_read(void)
{
    struct timex timex = {0};

    int status = adjtimex(&timex);
    if (unlikely(status < 0)) {
        PLUGIN_ERROR("error calling adjtimex : %s", STRERRNO);
        return -1;
    }

    metric_family_append(&fams[FAM_TIMEX_SYNC_STATUS],
                         VALUE_GAUGE(status == TIME_ERROR ? 0 : 1), NULL, NULL);
    metric_family_append(&fams[FAM_TIMEX_PLL_OFFSET_SECONDS],
                        VALUE_GAUGE((double)timex.offset /
                                    (timex.status & ADJ_NANO ? 1000000000L : 1000000L)),
                        NULL, NULL);
    metric_family_append(&fams[FAM_TIMEX_PLL_FREQUENCY_PPM],
                         VALUE_GAUGE(ldexp((double)timex.freq, -16)), NULL, NULL);
    metric_family_append(&fams[FAM_TIMEX_PLL_MAXIMUM_ERROR_SECONDS],
                         VALUE_GAUGE((double)timex.maxerror / 1000000L), NULL, NULL);
    metric_family_append(&fams[FAM_TIMEX_PLL_ESTIMATED_ERROR_SECONDS],
                         VALUE_GAUGE((double)timex.esterror / 1000000L), NULL, NULL);
    metric_family_append(&fams[FAM_TIMEX_STATUS],
                         VALUE_GAUGE(timex.status), NULL, NULL);
    metric_family_append(&fams[FAM_TIMEX_LOOP_TIME_CONSTANT],
                         VALUE_GAUGE(timex.constant), NULL, NULL);
    metric_family_append(&fams[FAM_TIMEX_TICK_SECONDS],
                         VALUE_GAUGE((double)timex.tick / 1000000L), NULL, NULL);
    metric_family_append(&fams[FAM_TIMEX_PPS_FREQUENCY_PPM],
                         VALUE_GAUGE(ldexp((double)timex.ppsfreq, -16)), NULL, NULL);
    metric_family_append(&fams[FAM_TIMEX_PPS_JITTER_SECONDS],
                         VALUE_GAUGE((double)timex.jitter /
                                     (timex.status & ADJ_NANO ? 1000000000L : 1000000L)),
                         NULL, NULL);
    metric_family_append(&fams[FAM_TIMEX_PPS_CALIBRATION_INTERVAL],
                         VALUE_GAUGE(timex.shift), NULL, NULL);
    metric_family_append(&fams[FAM_TIMEX_PPS_STABILITY_PPM],
                         VALUE_GAUGE(ldexp((double)timex.stabil, -16)), NULL, NULL);
    metric_family_append(&fams[FAM_TIMEX_PPS_JITTER_LIMIT],
                         VALUE_COUNTER(timex.jitcnt), NULL, NULL);
    metric_family_append(&fams[FAM_TIMEX_PPS_CALIBRATION_CICLES],
                         VALUE_COUNTER(timex.calcnt), NULL, NULL);
    metric_family_append(&fams[FAM_TIMEX_PPS_CALIBRATION_ERROR],
                         VALUE_COUNTER(timex.errcnt), NULL, NULL);
    metric_family_append(&fams[FAM_TIMEX_PPS_STABILITY_EXCEEDED],
                         VALUE_COUNTER(timex.stbcnt), NULL, NULL);
    metric_family_append(&fams[FAM_TIMEX_TAI_OFFSET_SECONDS],
                         VALUE_GAUGE(timex.tai), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_TIMEX_MAX, 0);

    return 0;
}

void module_register(void)
{
    plugin_register_read("timex", timex_read);
}
