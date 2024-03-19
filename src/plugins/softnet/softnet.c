// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/itoa.h"

static char *path_proc_softnet;

enum {
    FAM_SOFTNET_PROCESSED,
    FAM_SOFTNET_DROPPED,
    FAM_SOFTNET_TIMES_SQUEEZED,
    FAM_SOFTNET_RECEIVED_RPS,
    FAM_SOFTNET_FLOW_LIMIT,
    FAM_SOFTNET_BACKLOG_LENGTH,
    FAM_SOFTNET_MAX,
};

static metric_family_t fams[FAM_SOFTNET_MAX]= {
    [FAM_SOFTNET_PROCESSED] = {
        .name = "system_softnet_processed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of processed packets.",
    },
    [FAM_SOFTNET_DROPPED] = {
        .name = "system_softnet_dropped",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of dropped packets.",
    },
    [FAM_SOFTNET_TIMES_SQUEEZED] = {
        .name = "system_softnet_times_squeezed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times processing packets ran out of quota.",
    },
    [FAM_SOFTNET_RECEIVED_RPS] = {
        .name = "system_softnet_received_rps",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of steering packets received.",
    },
    [FAM_SOFTNET_FLOW_LIMIT] = {
        .name = "system_softnet_flow_limit",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times processing packets hit flow limit.",
    },
    [FAM_SOFTNET_BACKLOG_LENGTH] = {
        .name = "system_softnet_backlog_length",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of packets in backlog queue, sum of input queue and process queue.",
    },
};


static int softnet_read(void)
{
    FILE *fh = fopen(path_proc_softnet, "r");
    if (fh == NULL) {
        PLUGIN_WARNING("Unable to open '%s': %s", path_proc_softnet, STRERRNO);
        return EINVAL;
    }

    char buffer[256];
    char *fields[16];
    for (int ncpu = 0; fgets(buffer, sizeof(buffer), fh) != NULL ; ncpu++) {
        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));

        if (fields_num < 6)
            continue;

        value_t value = {0};
        char cpu[ITOA_MAX];

#if 0
    0             1             2             3  4  5  6  7                           8                         9                 10                 11                       12
sd->processed, sd->dropped, sd->time_squeeze, 0, 0, 0, 0, 0, /* was fastroute */ sd->cpu_collision        , sd->received_rps
sd->processed, sd->dropped, sd->time_squeeze, 0, 0, 0, 0, 0, /* was fastroute */ 0, /* was cpu_collision */ sd->received_rps, flow_limit_count
sd->processed, sd->dropped, sd->time_squeeze, 0, 0, 0, 0, 0, /* was fastroute */ 0, /* was cpu_collision */ sd->received_rps, flow_limit_count, softnet_backlog_len(sd), (int)seq->index
#endif

        if (fields_num >= 14) {
            int fcpu =  (int)strtol(fields[12], NULL, 16);
            uitoa(fcpu, cpu);
        } else {
            uitoa(ncpu, cpu);
        }

        if (fields_num >= 3) {
            value = VALUE_COUNTER(strtol(fields[0], NULL, 16));
            metric_family_append(&fams[FAM_SOFTNET_PROCESSED], value, NULL,
                                 &(label_pair_const_t){.name="cpu", .value=cpu}, NULL);

            value = VALUE_COUNTER(strtol(fields[1], NULL, 16));
            metric_family_append(&fams[FAM_SOFTNET_DROPPED], value, NULL,
                                 &(label_pair_const_t){.name="cpu", .value=cpu}, NULL);

            value = VALUE_COUNTER(strtol(fields[2], NULL, 16));
            metric_family_append(&fams[FAM_SOFTNET_TIMES_SQUEEZED], value, NULL,
                                 &(label_pair_const_t){.name="cpu", .value=cpu}, NULL);

            if (fields_num >= 10) {
                value = VALUE_COUNTER(strtol(fields[9], NULL, 16));
                metric_family_append(&fams[FAM_SOFTNET_RECEIVED_RPS], value, NULL,
                                     &(label_pair_const_t){.name="cpu", .value=cpu}, NULL);

                if (fields_num >= 12) {
                    value = VALUE_GAUGE(strtol(fields[11], NULL, 16));
                    metric_family_append(&fams[FAM_SOFTNET_BACKLOG_LENGTH], value, NULL,
                                         &(label_pair_const_t){.name="cpu", .value=cpu}, NULL);
                }
            }
        }
    }
    fclose(fh);

    plugin_dispatch_metric_family_array(fams, FAM_SOFTNET_MAX, 0);
    return 0;
}

static int softnet_init(void)
{
    path_proc_softnet = plugin_procpath("net/softnet_stat");
    if (path_proc_softnet == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int softnet_shutdown(void)
{
    free(path_proc_softnet);
    return 0;
}

void module_register(void)
{
    plugin_register_init("softnet", softnet_init);
    plugin_register_read("softnet", softnet_read);
    plugin_register_shutdown("softnet", softnet_shutdown);
}
