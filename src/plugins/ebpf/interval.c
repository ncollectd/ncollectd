// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) Namhyung Kim
// SPDX-FileContributor: Namhyung Kim <namhyung at gmail.com>

// From https://github.com/wkz/ply

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "ply.h"
#include "internal.h"


struct interval_data {
    unsigned long long interval;
    int fd;
};

static int interval_sym_alloc(__attribute__((unused)) struct ply_probe *pb,
                              __attribute__((unused)) struct node *n)
{
    return -ENOENT;
}

static int interval_probe(struct ply_probe *pb)
{
    char *unit;
    size_t i;
    struct {
        const char *str;
        unsigned long long factor;
    } interval_units[] = {
        { "m"  , 60 * 1e9 },
        { "s"  , 1e9 },
        { "ms" , 1e6 },
        { "us" , 1e3 },
        { "ns" , 1 },
    };

    char *num = strchr(pb->probe, ':');
    if (num == NULL) {
        PLUGIN_ERROR("Interval doesn't have unit: %s.", pb->probe);
        return -1;
    }
    num = strdup(num + 1);
    if (num == NULL) {
        PLUGIN_ERROR("Memory allocation failure.");
        return -1;
    }
    unsigned long long intvl = strtoull(num, &unit, 0);

    if (unit == NULL || *unit == '\0')
        unit = "s";

    for (i = 0; i < ARRAY_SIZE(interval_units); i++) {
        if (strcmp(unit, interval_units[i].str))
            continue;

        intvl *= interval_units[i].factor;
        break;
    }
    free(num);

    if (i == ARRAY_SIZE(interval_units)) {
        PLUGIN_ERROR("Invalid time unit: %s.", pb->probe);
        return -1;
    }

    struct interval_data *data = xcalloc(1, sizeof(*data));
    data->interval = intvl;
    data->fd = -1;

    pb->provider_data = data;
    return 0;
}

static int interval_attach(struct ply_probe *pb)
{
    struct interval_data *data = pb->provider_data;

    data->fd = perf_event_attach_raw(pb, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_CLOCK,
                                     data->interval, 0);
    if (data->fd < 0) {
        PLUGIN_ERROR("Interval attach failed.");
        return data->fd;
    }
    return 0;
}

static int interval_detach(struct ply_probe *pb)
{
    struct interval_data *data = pb->provider_data;
    close(data->fd);
    data->fd = -1;
    free(data);
    return 0;
}

struct provider interval = {
    .name      = "interval",
    .prog_type = BPF_PROG_TYPE_PERF_EVENT,
    .sym_alloc = interval_sym_alloc,
    .probe     = interval_probe,
    .attach    = interval_attach,
    .detach    = interval_detach,
};
