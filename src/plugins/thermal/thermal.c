// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2008 Michał Mirosław
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Michał Mirosław <mirq-linux at rere.qmqm.pl>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#ifndef KERNEL_LINUX
#error "This module is for Linux only."
#endif

enum {
    FAM_COOLING_DEVICE_MAX_STATE = 0,
    FAM_COOLING_DEVICE_CUR_STATE,
    FAM_THERMAL_ZONE_CELSIUS,
    FAM_THERMAL_MAX,
};

static metric_family_t fams[FAM_THERMAL_MAX] = {
    [FAM_COOLING_DEVICE_MAX_STATE] = {
        .name = "system_cooling_device_max_state",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_COOLING_DEVICE_CUR_STATE] = {
        .name = "system_cooling_device_cur_state",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_THERMAL_ZONE_CELSIUS] = {
        .name = "system_thermal_zone_celsius",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
};

static char *path_sys_thermal;
static char *path_proc_thermal_zone;

static bool force_procfs;
static exclist_t excl_device;

enum thermal_fs_e {
    THERMAL_NONE = 0,
    THERMAL_PROCFS,
    THERMAL_SYSFS,
};

static enum thermal_fs_e thermal_fs;

enum dev_type { TEMP = 0, COOLING_DEV };

static int thermal_sysfs_device_read(__attribute__((unused)) int dirfd,
                                     const char __attribute__((unused)) * dir,
                                     const char *name,
                                     __attribute__((unused)) void *user_data)
{
    char filename[PATH_MAX];
    bool success = false;

    if (!exclist_match(&excl_device, name))
        return -1;

    snprintf(filename, sizeof(filename), "%s/%s/temp", path_sys_thermal, name);
    double raw = 0;
    if (parse_double_file(filename, &raw) == 0) {
        metric_family_append(&fams[FAM_THERMAL_ZONE_CELSIUS], VALUE_GAUGE(raw/1000.0), NULL,
                             &(label_pair_const_t){.name="zone", .value=name}, NULL);
        success = true;
    }

    snprintf(filename, sizeof(filename), "%s/%s/cur_state", path_sys_thermal, name);
    if (parse_double_file(filename, &raw) == 0) {
        metric_family_append(&fams[FAM_COOLING_DEVICE_CUR_STATE], VALUE_GAUGE(raw), NULL,
                             &(label_pair_const_t){.name="device", .value=name}, NULL);
        success = true;
    }

    snprintf(filename, sizeof(filename), "%s/%s/max_state", path_sys_thermal, name);
    if (parse_double_file(filename, &raw) == 0) {
        metric_family_append(&fams[FAM_COOLING_DEVICE_MAX_STATE], VALUE_GAUGE(raw), NULL,
                             &(label_pair_const_t){.name="device", .value=name}, NULL);
        success = true;
    }

    return success ? 0 : -1;
}

static int thermal_procfs_device_read(__attribute__((unused)) int dirfd,
                                      const char __attribute__((unused)) * dir,
                                      const char *name,
                                      __attribute__((unused)) void *user_data)
{
    const char str_temp[] = "temperature:";
    char filename[PATH_MAX];
    char data[1024];

    if (!exclist_match(&excl_device, name))
        return -1;

    /**
     * rechot ~ # cat /proc/acpi/thermal_zone/THRM/temperature
     * temperature:                         55 C
     */

    int len = snprintf(filename, sizeof(filename), "%s/%s/temperature",
                                                   path_proc_thermal_zone, name);
    if ((len < 0) || ((size_t)len >= sizeof(filename)))
        return -1;

    len = (ssize_t)read_text_file_contents(filename, data, sizeof(data));
    if ((len > 0) && ((size_t)len > sizeof(str_temp)) &&
        (data[--len] == '\n') && (!strncmp(data, str_temp, sizeof(str_temp) - 1))) {
        char *endptr = NULL;
        double temp;
        double factor, add;

        if (data[--len] == 'C') {
            add = 0;
            factor = 1.0;
        } else if (data[len] == 'F') {
            add = -32;
            factor = 5.0 / 9.0;
        } else if (data[len] == 'K') {
            add = -273.15;
            factor = 1.0;
        } else
            return -1;

        while (len > 0 && data[--len] == ' ');
        data[len + 1] = 0;

        while (len > 0 && data[--len] != ' ');
        ++len;

        errno = 0;
        temp = (strtod(data + len, &endptr) + add) * factor;

        if (endptr != data + len && errno == 0) {
            metric_family_append(&fams[FAM_THERMAL_ZONE_CELSIUS], VALUE_GAUGE(temp), NULL,
                                 &(label_pair_const_t){.name="zone", .value=name}, NULL);
            return 0;
        }
    }

    return -1;
}

static int thermal_read(void)
{
    if (thermal_fs == THERMAL_PROCFS) {
        walk_directory(path_proc_thermal_zone, thermal_procfs_device_read, NULL, 0);
    } else {
        walk_directory(path_sys_thermal, thermal_sysfs_device_read, NULL, 0);
    }

    plugin_dispatch_metric_family_array(fams, FAM_THERMAL_MAX, 0);
    return 0;
}

static int thermal_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "device") == 0) {
            status = cf_util_exclist(child, &excl_device);
        } else if (strcasecmp(child->key, "force-use-procfs") == 0) {
            status = cf_util_get_boolean(child, &force_procfs);
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

static int thermal_init(void)
{
    path_sys_thermal = plugin_syspath("class/thermal");
    if (path_sys_thermal == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    path_proc_thermal_zone = plugin_procpath("acpi/thermal_zone");
    if (path_proc_thermal_zone == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    int ret = -1;

    if (!force_procfs && access(path_sys_thermal, R_OK | X_OK) == 0) {
        thermal_fs = THERMAL_SYSFS;
        ret = plugin_register_read("thermal", thermal_read);
    } else if (access(path_proc_thermal_zone, R_OK | X_OK) == 0) {
        thermal_fs = THERMAL_PROCFS;
        ret = plugin_register_read("thermal", thermal_read);
    }

    return ret;
}

static int thermal_shutdown(void)
{
    free(path_sys_thermal);
    free(path_proc_thermal_zone);
    exclist_reset(&excl_device);
    return 0;
}

void module_register(void)
{
    plugin_register_config("thermal", thermal_config);
    plugin_register_init("thermal", thermal_init);
    plugin_register_shutdown("thermal", thermal_shutdown);
}
