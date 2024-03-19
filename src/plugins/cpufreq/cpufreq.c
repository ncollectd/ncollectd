// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2005-2007  Peter Holik
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Peter Holik <peter at holik.at>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#ifdef KERNEL_FREEBSD
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

enum {
    FAM_CPU_FREQUENCY_HZ,
    FAM_CPU_FREQUENCY_STATE_RATIO,
    FAM_CPU_FREQUENCY_TRANSITIONS,
    FAM_CPU_FREQUENCY_MAX,
};

static metric_family_t fams[FAM_CPU_FREQUENCY_MAX] = {
    [FAM_CPU_FREQUENCY_HZ] = {
        .name = "system_cpu_frequency_hz",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current frequency of this CPU.",
    },
    [FAM_CPU_FREQUENCY_STATE_RATIO] = {
        .name = "system_cpu_frequency_state_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "The amount of time spent in each of the frequencies supported by this CPU",
    },
    [FAM_CPU_FREQUENCY_TRANSITIONS] = {
        .name = "system_cpu_frequency_transitions",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of frequency transitions on this CPU.",
    },
};

#ifdef KERNEL_LINUX

static char *path_sys;

#define MAX_AVAIL_FREQS 20

static int num_cpu;

struct cpu_data_t {
    counter_to_rate_state_t time_state[MAX_AVAIL_FREQS];
} * cpu_data;

/* Flags denoting capability of reporting CPU frequency statistics. */
static bool report_p_stats = false;

static void cpufreq_stats_init(void)
{
    cpu_data = calloc(num_cpu, sizeof(*cpu_data));
    if (cpu_data == NULL)
        return;

    report_p_stats = true;

    /* Check for stats module and disable if not present. */
    for (int i = 0; i < num_cpu; i++) {
        char filename[PATH_MAX];

        snprintf(filename, sizeof(filename),
                           "%s/devices/system/cpu/cpu%d/cpufreq/stats/time_in_state", path_sys, i);
        if (access(filename, R_OK)) {
            PLUGIN_NOTICE("File %s not exists or no access. P-State "
                          "statistics will not be reported. Check if `cpufreq-stats' kernel "
                          "module is loaded.", filename);
            report_p_stats = false;
            break;
        }

        snprintf(filename, sizeof(filename),
                           "%s/devices/system/cpu/cpu%d/cpufreq/stats/total_trans", path_sys, i);
        if (access(filename, R_OK)) {
            PLUGIN_NOTICE("File %s not exists or no access. P-State "
                          "statistics will not be reported. Check if `cpufreq-stats' kernel "
                          "module is loaded.", filename);
            report_p_stats = false;
            break;
        }
    }
    return;
}
#endif

#ifdef KERNEL_LINUX
static void cpufreq_read_stats(int cpu, char *cpunum)
{
    char filename[PATH_MAX];
    /* Read total transitions for cpu frequency */
    snprintf(filename, sizeof(filename),
                       "%s/devices/system/cpu/cpu%d/cpufreq/stats/total_trans", path_sys, cpu);

    uint64_t v;
    if (filetouint(filename, &v) != 0) {
        PLUGIN_ERROR("Reading '%s' failed.", filename);
        return;
    }

    metric_family_append(&fams[FAM_CPU_FREQUENCY_TRANSITIONS], VALUE_COUNTER(v), NULL,
                         &(label_pair_const_t){.name="cpu", .value=cpunum}, NULL);

    /* Determine percentage time in each state for cpu during previous
     * interval. */
    snprintf(filename, sizeof(filename),
                       "%s/devices/system/cpu/cpu%d/cpufreq/stats/time_in_state", path_sys, cpu);

    FILE *fh = fopen(filename, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Reading '%s' failed.", filename);
        return;
    }

    int state_index = 0;
    cdtime_t now = cdtime();
    char buffer[DATA_MAX_NAME_LEN];

    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        unsigned int frequency;
        unsigned long long time;

        /*
         * State time units is 10ms. To get rate of seconds per second
         * we have to divide by 100. To get percents we have to multiply it
         * by 100 back. So, just use parsed value directly.
         */
        if (!sscanf(buffer, "%u%llu", &frequency, &time)) {
            PLUGIN_ERROR("Reading \"%s\" failed.", filename);
            break;
        }

        char state[DATA_MAX_NAME_LEN];
        snprintf(state, sizeof(state), "%u", frequency);

        if (state_index >= MAX_AVAIL_FREQS) {
            PLUGIN_NOTICE("Found too many frequency states (%d > %d). "
                           "Plugin needs to be recompiled. Please open a bug report for "
                           "this.", (state_index + 1), MAX_AVAIL_FREQS);
            break;
        }

        double g;
        if (counter_to_rate(&g, time, now, &(cpu_data[cpu].time_state[state_index])) == 0) {
            /*
             * Due to some inaccuracy reported value can be a bit greatrer than 100.1.
             * That produces gaps on charts.
             */
            if (g > 100.1)
                g = 100.1;

            metric_family_append(&fams[FAM_CPU_FREQUENCY_STATE_RATIO], VALUE_GAUGE(g), NULL,
                                 &(label_pair_const_t){.name="cpu", .value=cpunum},
                                 &(label_pair_const_t){.name="state", .value=state}, NULL);
        }
        state_index++;
    }
    fclose(fh);
}
#endif

static int cpufreq_read(void)
{
#ifdef KERNEL_LINUX
    for (int cpu = 0; cpu < num_cpu; cpu++) {
        char filename[PATH_MAX];
        /* Read cpu frequency */
        snprintf(filename, sizeof(filename),
                           "%s/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", path_sys, cpu);

        double v;
        if (filetodouble(filename, &v) != 0) {
            PLUGIN_WARNING("Reading '%s' failed.", filename);
            continue;
        }

        /* convert kHz to Hz */
        v *= 1000.0;

        char cpunum[64];
        snprintf(cpunum, sizeof(cpunum), "%d", cpu);
        metric_family_append(&fams[FAM_CPU_FREQUENCY_HZ], VALUE_GAUGE(v), NULL,
                             &(label_pair_const_t){.name="cpu", .value=cpunum}, NULL);

        if (report_p_stats)
            cpufreq_read_stats(cpu, cpunum);
    }
#elif defined(KERNEL_FREEBSD)
    /* FreeBSD currently only has 1 freq setting.  See BUGS in cpufreq(4) */
    char mib[] = "dev.cpu.0.freq";
    int cpufreq;
    size_t cf_len = sizeof(cpufreq);

    if (sysctlbyname(mib, &cpufreq, &cf_len, NULL, 0) != 0) {
        PLUGIN_WARNING("sysctl \"%s\" failed.", mib);
        return 0;
    }

    /* convert Mhz to Hz */
    metric_family_append(&fams[FAM_CPU_FREQUENCY_HZ], VALUE_GAUGE(cpufreq * 1000000.0), NULL,
                         &(label_pair_const_t){.name="cpu", .value="0"}, NULL);
#endif


    plugin_dispatch_metric_family_array(fams, FAM_CPU_FREQUENCY_MAX, 0);

    return 0;
}

static int cpufreq_init(void)
{
#ifdef KERNEL_LINUX
    path_sys = plugin_syspath(NULL);
    if (path_sys == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    char filename[PATH_MAX];

    num_cpu = 0;

    while (1) {
        int status = snprintf(filename, sizeof(filename),
                              "%s/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq",
                              path_sys, num_cpu);
        if ((status < 1) || ((unsigned int)status >= sizeof(filename)))
            break;

        if (access(filename, R_OK))
            break;

        num_cpu++;
    }

    PLUGIN_INFO("Found %d CPU%s", num_cpu, (num_cpu == 1) ? "" : "s");
    cpufreq_stats_init();

    if (num_cpu == 0)
        plugin_unregister_read("cpufreq");
#elif defined(KERNEL_FREEBSD)
    char mib[] = "dev.cpu.0.freq";
    int cpufreq;
    size_t cf_len = sizeof(cpufreq);

    if (sysctlbyname(mib, &cpufreq, &cf_len, NULL, 0) != 0) {
        PLUGIN_WARNING("sysctl \"%s\" failed.", mib);
        plugin_unregister_read("cpufreq");
    }
#endif

    return 0;
}

#ifdef KERNEL_LINUX
static int cpufreq_shutdown(void)
{
    free(path_sys);
    free(cpu_data);

    return 0;
}
#endif

void module_register(void)
{
    plugin_register_init("cpufreq", cpufreq_init);
    plugin_register_read("cpufreq", cpufreq_read);
#ifdef KERNEL_LINUX
    plugin_register_shutdown("cpufreq", cpufreq_shutdown);
#endif
}
