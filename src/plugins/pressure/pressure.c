// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

static char *proc_pressure_cpu;
static char *proc_pressure_io;
static char *proc_pressure_memory;
static char *proc_pressure_irq;

enum {
    FAM_PRESSURE_CPU_WAITING_SECONDS,
    FAM_PRESSURE_CPU_STALLED_SECONDS,
    FAM_PRESSURE_IO_WAITING_SECONDS,
    FAM_PRESSURE_IO_STALLED_SECONDS,
    FAM_PRESSURE_MEMORY_WAITING_SECONDS,
    FAM_PRESSURE_MEMORY_STALLED_SECONDS,
    FAM_PRESSURE_IRQ_STALLED_SECONDS,
    FAM_PRESSURE_MAX,
};

static metric_family_t fams[FAM_PRESSURE_MAX] = {
    [FAM_PRESSURE_CPU_WAITING_SECONDS] = {
        .name = "system_pressure_cpu_waiting_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The share of time in which at least some tasks are stalled on the cpu.",
    },
    [FAM_PRESSURE_CPU_STALLED_SECONDS] = {
        .name = "system_pressure_cpu_stalled_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The share of time in which all non-idle tasks are stalled "
                "on the cpu simultaneously.",
    },
    [FAM_PRESSURE_IO_WAITING_SECONDS] = {
        .name = "system_pressure_io_waiting_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The share of time in which at least some tasks are stalled on the io.",
    },
    [FAM_PRESSURE_IO_STALLED_SECONDS] = {
        .name = "system_pressure_io_stalled_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The share of time in which all non-idle tasks are stalled "
                "on the io simultaneously."
    },
    [FAM_PRESSURE_MEMORY_WAITING_SECONDS] = {
        .name = "system_pressure_memory_waiting_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The share of time in which at least some tasks are stalled on the memory.",
    },
    [FAM_PRESSURE_MEMORY_STALLED_SECONDS] = {
        .name = "system_pressure_memory_stalled_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The share of time in which all non-idle tasks are stalled "
                "on the memory simultaneously.",
    },
    [FAM_PRESSURE_IRQ_STALLED_SECONDS] = {
        .name = "system_pressure_irq_stalled_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The share of time in which IRQ/SOFTIRQ are stalled.",
    },
};

static int pressure_read_file(const char *filename,
                              metric_family_t *fam_waiting, metric_family_t *fam_stalled)
{
    FILE *fh = fopen(filename, "r");
    if (unlikely(fh == NULL)) {
        PLUGIN_ERROR("Open \"%s\"  failed: %s", filename, STRERRNO);
        return -1;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *fields[5] = {0};
        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (fields_num != 5)
            continue;

        if (strncmp(fields[4], "total=", strlen("total=")) != 0)
            continue;

        char *total_value = fields[4] + strlen("total=");
        if (*total_value == '\0')
            continue;

        value_t value = VALUE_COUNTER_FLOAT64((double)atoll(total_value) / 1000000.0);

        if ((strcmp(fields[0], "some") == 0) && (fam_waiting != NULL)) {
            metric_family_append(fam_waiting, value, NULL, NULL);
        } else if ((strcmp(fields[0], "full") == 0) && (fam_stalled != NULL)) {
            metric_family_append(fam_stalled, value, NULL, NULL);
        }
    }

    fclose(fh);

    return 0;
}

static int pressure_read(void)
{
    pressure_read_file(proc_pressure_cpu,
                       &fams[FAM_PRESSURE_CPU_WAITING_SECONDS],
                       &fams[FAM_PRESSURE_CPU_STALLED_SECONDS]);
    pressure_read_file(proc_pressure_io,
                       &fams[FAM_PRESSURE_IO_WAITING_SECONDS],
                       &fams[FAM_PRESSURE_IO_STALLED_SECONDS]);
    pressure_read_file(proc_pressure_memory,
                       &fams[FAM_PRESSURE_MEMORY_WAITING_SECONDS],
                       &fams[FAM_PRESSURE_MEMORY_STALLED_SECONDS]);
    pressure_read_file(proc_pressure_irq, NULL,
                       &fams[FAM_PRESSURE_IRQ_STALLED_SECONDS]);

    plugin_dispatch_metric_family_array(fams, FAM_PRESSURE_MAX, 0);

    return 0;
}

static int pressure_init(void)
{
    proc_pressure_cpu = plugin_procpath("pressure/cpu");
    if (proc_pressure_cpu == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    proc_pressure_io = plugin_procpath("pressure/io");
    if (proc_pressure_io == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    proc_pressure_memory = plugin_procpath("pressure/memory");
    if (proc_pressure_memory == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    proc_pressure_irq = plugin_procpath("pressure/irq");
    if (proc_pressure_irq == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int pressure_shutdown(void)
{
    free(proc_pressure_cpu);
    free(proc_pressure_io);
    free(proc_pressure_memory);
    free(proc_pressure_irq);
    return 0;
}

void module_register(void)
{
    plugin_register_init("pressure", pressure_init);
    plugin_register_read("pressure", pressure_read);
    plugin_register_shutdown("pressure", pressure_shutdown);
}
