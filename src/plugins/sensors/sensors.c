// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2008 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2006 Luboš Staněk
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Lubos Stanek <lubek at users.sourceforge.net> Wed Oct 27, 2006
// SPDX-FileContributor: Henrique de Moraes Holschuh <hmh at debian.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include "sensors.h"

char *conffile = NULL;
bool use_labels = false;
plugin_filter_t *filter = NULL;

metric_family_t fams[FAM_SENSOR_MAX] = {
    [FAM_SENSOR_VOLTAGE_VOLTS] = {
        .name = "system_sensor_voltage_volts",
        .type = METRIC_TYPE_GAUGE,
        .help = "Voltage in volts.",
    },
    [FAM_SENSOR_FAN_SPEED_RPM] = {
        .name = "system_sensor_fan_speed_rpm",
        .type = METRIC_TYPE_GAUGE,
        .help = "Fan speed (rotations per minute).",
    },
    [FAM_SENSOR_TEMPERATURE_CELSIUS] = {
        .name = "system_sensor_temperature_celsius",
        .type = METRIC_TYPE_GAUGE,
        .help = "Temperature in celsius.",
    },
    [FAM_SENSOR_POWER_WATTS] = {
        .name = "system_sensor_power_watts",
        .type = METRIC_TYPE_GAUGE,
        .help = "Power in watts.",
    },
    [FAM_SENSOR_POWER_AVERAGE_WATTS] = {
        .name = "system_sensor_power_average_watts",
        .type = METRIC_TYPE_GAUGE,
        .help = "Power average in watts.",
    },
    [FAM_SENSOR_CURRENT_AMPS] = {
        .name = "system_sensor_current_amps",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current in amps.",
    },
    [FAM_SENSOR_HUMIDITY] = {
        .name = "system_sensor_humidity_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "Ambient relative humidity (percent).",
    },
    [FAM_SENSOR_INTRUSION_ALARM] = {
        .name = "system_sensor_intrusion_alarm",
        .type = METRIC_TYPE_GAUGE,
        .help = "Intrusion detection alarm.",
    },
};

int ncsensors_read(void);

static int ncsensors_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "sensor-config-file") == 0) {
#ifdef KERNEL_LINUX
            status = cf_util_get_string(child, &conffile);
#else
            PLUGIN_WARNING("The 'sensor-config-file' is only supported in Linux.");
#endif
        } else if (strcasecmp(child->key, "use-labels") == 0) {
#ifdef KERNEL_LINUX
            status = cf_util_get_boolean(child, &use_labels);
#else
            PLUGIN_WARNING("The 'use_labels' is only supported in Linux.");
#endif
        } else if (strcasecmp(child->key, "filter") == 0) {
            status = plugin_filter_configure(child, &filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0)
        return -1;

    return 0;
}

__attribute__(( weak ))
int ncsensors_init(void)
{
    return 0;
}

__attribute__(( weak ))
int ncsensors_shutdown(void)
{
    return 0;
}

void module_register(void)
{
    plugin_register_config("sensors", ncsensors_config);
    plugin_register_init("sensors", ncsensors_init);
    plugin_register_read("sensors", ncsensors_read);
    plugin_register_shutdown("sensors", ncsensors_shutdown);
}
