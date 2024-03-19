// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

static char *path_proc_schedstat;

enum {
    FAM_SCHEDSTAT_RUNNING,
    FAM_SCHEDSTAT_WAITING,
    FAM_SCHEDSTAT_TIMESLICES,
    FAM_SCHEDSTAT_MAX,
};

static metric_family_t fams[FAM_SCHEDSTAT_MAX]= {
    [FAM_SCHEDSTAT_RUNNING] = {
        .name = "system_schedstat_running",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of jiffies spent running a process.",
    },
    [FAM_SCHEDSTAT_WAITING] = {
        .name = "system_schedstat_waiting",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of jiffies waiting for this CPU.",
    },
    [FAM_SCHEDSTAT_TIMESLICES] = {
        .name = "system_schedstat_timeslices",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of timeslices executed by CPU.",
    },
};

static int schedstat_read(void)
{
    FILE *fh = fopen(path_proc_schedstat, "r");
    if (fh == NULL) {
        PLUGIN_WARNING("Unable to open '%s': %s", path_proc_schedstat, STRERRNO);
        return EINVAL;
    }

    char buffer[4096];
    char *fields[16];
    while(fgets(buffer, sizeof(buffer), fh) != NULL) {
        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (unlikely(fields_num < 10))
            continue;

        if (strncmp(fields[0], "cpu", 3) != 0)
            continue;

        char *ncpu = fields[0] + 3;
        value_t value = {0};

        value = VALUE_COUNTER(strtol(fields[7], NULL, 10));
        metric_family_append(&fams[FAM_SCHEDSTAT_RUNNING], value, NULL,
                             &(label_pair_const_t){.name="cpu", .value=ncpu}, NULL);

        value = VALUE_COUNTER(strtol(fields[8], NULL, 10));
        metric_family_append(&fams[FAM_SCHEDSTAT_WAITING], value, NULL,
                             &(label_pair_const_t){.name="cpu", .value=ncpu}, NULL);


        value = VALUE_COUNTER(strtol(fields[9], NULL, 10));
        metric_family_append(&fams[FAM_SCHEDSTAT_TIMESLICES], value, NULL,
                             &(label_pair_const_t){.name="cpu", .value=ncpu}, NULL);
    }
    fclose(fh);

    plugin_dispatch_metric_family_array(fams, FAM_SCHEDSTAT_MAX, 0);
    return 0;
}

static int schedstat_init(void)
{
    path_proc_schedstat = plugin_procpath("schedstat");
    if (path_proc_schedstat == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int schedstat_shutdown(void)
{
    free(path_proc_schedstat);
    return 0;
}

void module_register(void)
{
    plugin_register_init("schedstat", schedstat_init);
    plugin_register_read("schedstat", schedstat_read);
    plugin_register_shutdown("schedstat", schedstat_shutdown);
}
