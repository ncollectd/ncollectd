// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2016 Slava Polyakov
// SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín
// SPDX-FileContributor: Slava Polyakov <sigsegv0x0b at gmail.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

typedef struct {
    char socket[32];
    char hwmon[32];
} coretemp_t;

static char *path_sys_devices;

enum {
    FAM_CORETEMP_CORE_TEMPERATURE_CELSIUS,
    FAM_CORETEMP_CORE_MAX_TEMPERATURE_CELSIUS,
    FAM_CORETEMP_CORE_CRITICAL_TEMPERATURE_CELSIUS,
    FAM_CORETEMP_PACKAGE_TEMPERATURE_CELSIUS,
    FAM_CORETEMP_PACKAGE_MAX_TEMPERATURE_CELSIUS,
    FAM_CORETEMP_PACKAGE_CRITICAL_TEMPERATURE_CELSIUS,
    FAM_CORETEMP_MAX
};

static metric_family_t fams[FAM_CORETEMP_MAX] = {
    [FAM_CORETEMP_CORE_TEMPERATURE_CELSIUS] = {
        .name = "system_coretemp_core_temperature_celsius",
        .type = METRIC_TYPE_GAUGE,
        .help = "Core temperature in celsius."
    },
    [FAM_CORETEMP_CORE_MAX_TEMPERATURE_CELSIUS] = {
        .name = "system_coretemp_core_max_temperature_celsius",
        .type = METRIC_TYPE_GAUGE,
        .help = "Temperature at all cooling devices should be turned on in celsius."
    },
    [FAM_CORETEMP_CORE_CRITICAL_TEMPERATURE_CELSIUS] = {
        .name = "system_coretemp_core_critical_temperature_celsius",
        .type = METRIC_TYPE_GAUGE,
        .help = "Maximum junction temperature in celsius."
    },
    [FAM_CORETEMP_PACKAGE_TEMPERATURE_CELSIUS] = {
        .name = "system_coretemp_package_temperature_celsius",
        .type = METRIC_TYPE_GAUGE,
        .help = "Package temperature in celsius."
    },
    [FAM_CORETEMP_PACKAGE_MAX_TEMPERATURE_CELSIUS] = {
        .name = "system_coretemp_package_max_temperature_celsius",
        .type = METRIC_TYPE_GAUGE,
        .help = "Temperature at all cooling devices should be turned on in celsius."
    },
    [FAM_CORETEMP_PACKAGE_CRITICAL_TEMPERATURE_CELSIUS] = {
        .name = "system_coretemp_package_critical_temperature_celsius",
        .type = METRIC_TYPE_GAUGE,
        .help = "Maximum junction temperature in celsius."
    }
};

static int coretemp_read_temp(int dir_fd, __attribute__((unused)) const char *dirname,
                              const char *filename, void *user_data)
{
    if (strncmp("temp", filename, strlen("temp")) != 0)
        return 0;

    const char *suffix = filename + strlen("temp");
    while(isdigit(*suffix)) suffix++;

    if (*suffix != '_')
        return 0;

    suffix++;

    if (strcmp("label", suffix) != 0)
        return 0;

    char label[256];
    ssize_t len = read_file_at(dir_fd, filename, label, sizeof(label));
    if (len < 0)
        return 0;
    strnrtrim(label, len);

    coretemp_t *coretemp = (coretemp_t *)user_data;

    char prefix[256];
    size_t prefix_len = suffix - filename;
    if (prefix_len > (sizeof(prefix) - 1))
        return 0;
    sstrncpy(prefix, filename, prefix_len);

    char path_temp[PATH_MAX];

    ssnprintf(path_temp, sizeof(path_temp), "%s_input", prefix);
    double input = 0;
    int status = filetodouble_at(dir_fd, path_temp, &input);
    if (status != 0)
        return 0;

    ssnprintf(path_temp, sizeof(path_temp), "%s_max", prefix);
    double max = 0;
    status = filetodouble_at(dir_fd, path_temp, &max);
    if (status != 0)
        return 0;

    ssnprintf(path_temp, sizeof(path_temp), "%s_crit", prefix);
    double crit = 0;
    status = filetodouble_at(dir_fd, path_temp, &crit);
    if (status != 0)
        return 0;

    if (strncmp(label, "Package id ", strlen("Package id ")) == 0) {
        char *package_id = label + strlen("Package id ");
        metric_family_append(&fams[FAM_CORETEMP_PACKAGE_TEMPERATURE_CELSIUS],
                             VALUE_GAUGE(input/1000.0), NULL,
                             &(label_pair_const_t){.name="socket", .value=coretemp->socket},
                             &(label_pair_const_t){.name="hwmon", .value=coretemp->hwmon},
                             &(label_pair_const_t){.name="package", .value=package_id},
                             NULL);
        metric_family_append(&fams[FAM_CORETEMP_PACKAGE_MAX_TEMPERATURE_CELSIUS],
                             VALUE_GAUGE(max/1000.0), NULL,
                             &(label_pair_const_t){.name="socket", .value=coretemp->socket},
                             &(label_pair_const_t){.name="hwmon", .value=coretemp->hwmon},
                             &(label_pair_const_t){.name="package", .value=package_id},
                             NULL);
        metric_family_append(&fams[FAM_CORETEMP_PACKAGE_CRITICAL_TEMPERATURE_CELSIUS],
                             VALUE_GAUGE(crit/1000.0), NULL,
                             &(label_pair_const_t){.name="socket", .value=coretemp->socket},
                             &(label_pair_const_t){.name="hwmon", .value=coretemp->hwmon},
                             &(label_pair_const_t){.name="package", .value=package_id},
                             NULL);
    } else if (strncmp(label, "Core ", strlen("Core ")) == 0)  {
        char *core_id = label + strlen("Core ");
        metric_family_append(&fams[FAM_CORETEMP_CORE_TEMPERATURE_CELSIUS],
                             VALUE_GAUGE(input/1000.0), NULL,
                             &(label_pair_const_t){.name="socket", .value=coretemp->socket},
                             &(label_pair_const_t){.name="hwmon", .value=coretemp->hwmon},
                             &(label_pair_const_t){.name="core", .value=core_id},
                             NULL);
        metric_family_append(&fams[FAM_CORETEMP_CORE_MAX_TEMPERATURE_CELSIUS],
                             VALUE_GAUGE(max/1000.0), NULL,
                             &(label_pair_const_t){.name="socket", .value=coretemp->socket},
                             &(label_pair_const_t){.name="hwmon", .value=coretemp->hwmon},
                             &(label_pair_const_t){.name="core", .value=core_id},
                             NULL);
        metric_family_append(&fams[FAM_CORETEMP_CORE_CRITICAL_TEMPERATURE_CELSIUS],
                             VALUE_GAUGE(crit/1000.0), NULL,
                             &(label_pair_const_t){.name="socket", .value=coretemp->socket},
                             &(label_pair_const_t){.name="hwmon", .value=coretemp->hwmon},
                             &(label_pair_const_t){.name="core", .value=core_id},
                             NULL);
    }

    return 0;
}

static int coretemp_read_hwmon(int dir_fd, __attribute__((unused)) const char *dirname,
                               const char *filename, void *user_data)
{
    if (strncmp("hwmon", filename, strlen("hwmon")) != 0)
        return 0;

    coretemp_t *coretemp = (coretemp_t *)user_data;
    sstrncpy(coretemp->hwmon, filename + strlen("hwmon"), sizeof(coretemp->hwmon));

    walk_directory_at(dir_fd, filename, coretemp_read_temp, coretemp, 0);

    return 0;
}

static int coretemp_read_coretemp(int dir_fd, __attribute__((unused)) const char *dirname,
                                  const char *filename, __attribute__((unused)) void *user_data)
{
    if (strncmp("coretemp.", filename, strlen("coretemp.")) != 0)
        return 0;

    int core_fd = openat(dir_fd, filename, O_RDONLY | O_DIRECTORY);
    if (core_fd < 0)
        return 0;

    coretemp_t coretemp = {0};
    sstrncpy(coretemp.socket, filename + strlen("coretemp."), sizeof(coretemp.socket));

    walk_directory_at(core_fd, "hwmon", coretemp_read_hwmon, &coretemp, 0);

    close(core_fd);

    return 0;
}

static int coretemp_read (void)
{
    walk_directory(path_sys_devices, coretemp_read_coretemp, NULL, 0);

    plugin_dispatch_metric_family_array(fams, FAM_CORETEMP_MAX, 0);

    return 0;
}

static int coretemp_init(void)
{
    path_sys_devices = plugin_syspath("devices/platform");
    if (path_sys_devices == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int coretemp_shutdown(void)
{
    free(path_sys_devices);
    return 0;
}

void module_register(void)
{
    plugin_register_init("coretemp", coretemp_init);
    plugin_register_read("coretemp", coretemp_read);
    plugin_register_shutdown("coretemp", coretemp_shutdown);
}
