// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2014 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2008 Oleg King
// SPDX-FileCopyrightText: Copyright (C) 2009 Simon Kuhnle
// SPDX-FileCopyrightText: Copyright (C) 2009 Manuel Sanmartin
// SPDX-FileCopyrightText: Copyright (C) 2009-2024 Manuel Sanmartín
// SPDX-FileCopyrightText: Copyright (C) 2013-2014 Pierre-Yves Ritschard
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Oleg King <king2 at kaluga.ru>
// SPDX-FileContributor: Simon Kuhnle <simon at blarzwurst.de>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>
// SPDX-FileContributor: Pierre-Yves Ritschard <pyr at spootnik.org>

#include "plugin.h"
#include "libutils/common.h"

#include "cpu.h"

metric_family_t fams[FAM_CPU_MAX] = {
    [FAM_CPU_ALL_USAGE] = {
        .name = "system_cpu_all_usage_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The amount of time, in seconds, that the system spent in various states.",
    },
    [FAM_CPU_USAGE] = {
        .name = "system_cpu_usage_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The amount of time, in seconds, that the specific CPU spent in various states.",
    },
    [FAM_CPU_COUNT] = {
        .name = "system_cpu_count",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of cpus in the system.",
    },
};

bool cpu_report_guest = false;
bool cpu_subtract_guest = true;
bool cpu_report_topology = false;

static int cpu_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "report-guest-state") == 0) {
            status = cf_util_get_boolean(child, &cpu_report_guest);
        } else if (strcasecmp(child->key, "subtract-guest-state") == 0) {
            status = cf_util_get_boolean(child, &cpu_subtract_guest);
        } else if (strcasecmp(child->key, "report-topology") == 0) {
            status = cf_util_get_boolean(child, &cpu_report_topology);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

int cpu_read(void);

__attribute__(( weak ))
int cpu_init(void)
{
    return 0;
}

__attribute__(( weak ))
int cpu_shutdown(void)
{
    return 0;
}

void module_register(void)
{
    plugin_register_init("cpu", cpu_init);
    plugin_register_config("cpu", cpu_config);
    plugin_register_read("cpu", cpu_read);
    plugin_register_shutdown("cpu", cpu_shutdown);
}
