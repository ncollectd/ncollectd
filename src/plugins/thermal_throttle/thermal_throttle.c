// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/itoa.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

typedef struct {
    int core_id;
    int package_id;
} cpu_map_t;

typedef struct {
    bool found;
    uint64_t count;
    uint64_t time;
} thermal_throttle_t;

static int core_max_id;
static int package_max_id;

static size_t cpus_num;
static cpu_map_t *cpus;
static size_t cpu_max_found;

static size_t thermal_throttle_cores_num;
static thermal_throttle_t *thermal_throttle_cores;

static size_t thermal_throttle_packages_num;
static thermal_throttle_t *thermal_throttle_packages;

static char *path_sys_devices;

enum {
    FAM_THERMAL_THROTTLE_CORE_COUNT,
    FAM_THERMAL_THROTTLE_CORE_TIME_SECONDS,
    FAM_THERMAL_THROTTLE_PACKAGE_COUNT,
    FAM_THERMAL_THROTTLE_PACKAGE_TIME_SECONDS,
    FAM_THERMAL_THROTTLE_MAX
};

static metric_family_t fams[FAM_THERMAL_THROTTLE_MAX] = {
    [FAM_THERMAL_THROTTLE_CORE_COUNT] = {
        .name = "system_thermal_throttle_core_count",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times \"Thermal Status flag\" changed from 0 to 1 for this core."
    },
    [FAM_THERMAL_THROTTLE_CORE_TIME_SECONDS] = {
        .name = "system_thermal_throttle_core_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total time in seconds for which \"Thermal Status flag\" has been "
                "set to 1 for this core."
    },
    [FAM_THERMAL_THROTTLE_PACKAGE_COUNT] = {
        .name = "system_thermal_throttle_package_count",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times \"Thermal Status flag\" changed from 0 to 1 for "
                "this package."
    },
    [FAM_THERMAL_THROTTLE_PACKAGE_TIME_SECONDS] = {
        .name = "system_thermal_throttle_package_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total time in seconds for which \"Thermal Status flag\" has been set "
                "to 1 this package."
    }
};

static int thermal_throttle_cpu(int dir_fd, __attribute__((unused)) const char *dirname,
                                const char *filename, __attribute__((unused)) void *user_data)
{
    if (strncmp("cpu", filename, strlen("cpu")) != 0)
        return 0;

    const char *cpu_num = filename + strlen("cpu");
    if (!isdigit(*cpu_num))
        return 0;

    size_t cpu = atoi(cpu_num);
    if (cpu > cpus_num)
        return 0;

    if (cpu > cpu_max_found)
        cpu_max_found = cpu;

    int core_id = cpus[cpu].core_id;
    if ((core_id >= 0) && ((size_t)core_id < thermal_throttle_cores_num)) {
        if (!thermal_throttle_cores[core_id].found) {
            char path[PATH_MAX];

            ssnprintf(path, sizeof(path), "%s/thermal_throttle/core_throttle_count", filename);
            uint64_t core_throttle_count = 0;
            int status = filetouint_at(dir_fd, path, &core_throttle_count);
            if (status != 0)
                return 0;

            ssnprintf(path, sizeof(path), "%s/thermal_throttle/core_throttle_total_time_ms", filename);
            uint64_t core_throttle_time = 0;
            status = filetouint_at(dir_fd, path, &core_throttle_time);
            if (status != 0)
                return 0;

            thermal_throttle_cores[core_id].found = true;
            thermal_throttle_cores[core_id].count = core_throttle_count;
            thermal_throttle_cores[core_id].time = core_throttle_time;
        }
    }

    int package_id = cpus[cpu].package_id;
    if ((package_id >= 0 ) && ((size_t)package_id < thermal_throttle_packages_num)) {
        if (!thermal_throttle_packages[package_id].found) {
            char path[PATH_MAX];

            ssnprintf(path, sizeof(path), "%s/thermal_throttle/package_throttle_count", filename);
            uint64_t package_throttle_count = 0;
            int status = filetouint_at(dir_fd, path, &package_throttle_count);
            if (status != 0)
                return 0;

            ssnprintf(path, sizeof(path), "%s/thermal_throttle/package_throttle_total_time_ms", filename);
            uint64_t package_throttle_time = 0;
            status = filetouint_at(dir_fd, path, &package_throttle_time);
            if (status != 0)
                return 0;

            thermal_throttle_packages[package_id].found = true;
            thermal_throttle_packages[package_id].count = package_throttle_count;
            thermal_throttle_packages[package_id].time = package_throttle_time;
        }
    }

    return 0;
}

static int thermal_throttle_topology(int dir_fd, __attribute__((unused)) const char *dirname,
                                     const char *filename, __attribute__((unused)) void *user_data)
{
    if (strncmp("cpu", filename, strlen("cpu")) != 0)
        return 0;

    const char *cpu_num = filename + strlen("cpu");
    if (!isdigit(*cpu_num))
        return 0;

    size_t cpu = atoi(cpu_num);

    char path[PATH_MAX];

    ssnprintf(path, sizeof(path), "%s/topology/core_id", filename);
    uint64_t core_id = 0;
    int status = filetouint_at(dir_fd, path, &core_id);
    if (status != 0)
        return 0;

    ssnprintf(path, sizeof(path), "%s/topology/physical_package_id", filename);
    uint64_t package_id = 0;
    status = filetouint_at(dir_fd, path, &package_id);
    if (status != 0)
        return 0;

    if (cpu >= cpus_num) {
        cpu_map_t *tmp_cpus = realloc(cpus, sizeof(cpu_map_t) * (cpu + 1));
        if (tmp_cpus == NULL)
            return 0;

        cpus = tmp_cpus;

        for (size_t i = cpus_num; i < (cpu + 1); i++) {
            cpus[i].core_id = -1;
            cpus[i].package_id = -1;
        }

        cpus_num = cpu + 1;
    }

    cpus[cpu].core_id = core_id;
    cpus[cpu].package_id = package_id;

    if ((int)core_id > core_max_id)
        core_max_id = (int)core_id;

    if ((int)package_id > package_max_id)
        package_max_id = (int)package_id;

    return 0;
}

static void thermal_throttle_free(void)
{
    if (cpus != NULL) {
        free(cpus);
        cpus = NULL;
    }
    cpus_num = 0;

    core_max_id = 0;
    package_max_id = 0;

    if (thermal_throttle_cores != NULL) {
        free(thermal_throttle_cores);
        thermal_throttle_cores = NULL;
    }
    thermal_throttle_cores_num = 0;

    if (thermal_throttle_packages != NULL) {
        free(thermal_throttle_packages);
        thermal_throttle_packages = NULL;
    }
    thermal_throttle_packages_num = 0;
}

static int thermal_throttle_read(void)
{
    if ((cpus_num == 0) || ((cpu_max_found + 1) != cpus_num)) {
        core_max_id = -1;
        package_max_id = -1;

        thermal_throttle_free();

        walk_directory(path_sys_devices, thermal_throttle_topology, NULL, 0);

        if ((core_max_id == -1) || (package_max_id == -1)) {
            thermal_throttle_free();
            return 0;
        }

        thermal_throttle_cores_num = core_max_id + 1;
        thermal_throttle_cores = calloc(thermal_throttle_cores_num,
                                        sizeof(*thermal_throttle_cores));
        if (thermal_throttle_cores == NULL) {
            thermal_throttle_free();
            return 0;
        }

        thermal_throttle_packages_num = package_max_id + 1;
        thermal_throttle_packages = calloc(thermal_throttle_packages_num,
                                           sizeof(*thermal_throttle_packages));
        if (thermal_throttle_packages == NULL) {
            thermal_throttle_free();
            return 0;
        }
    }

    cpu_max_found = 0;

    memset(thermal_throttle_cores, 0,
           sizeof(*thermal_throttle_cores) * thermal_throttle_cores_num);
    memset(thermal_throttle_packages, 0,
           sizeof(*thermal_throttle_packages) * thermal_throttle_packages_num);

    walk_directory(path_sys_devices, thermal_throttle_cpu, NULL, 0);

    for (size_t i = 0; i < thermal_throttle_cores_num; i++) {
        if (thermal_throttle_cores[i].found) {
            char core_id_str[ITOA_MAX];
            uitoa(i, core_id_str);

            metric_family_append(&fams[FAM_THERMAL_THROTTLE_CORE_COUNT],
                                 VALUE_COUNTER(thermal_throttle_cores[i].count), NULL,
                                 &(label_pair_const_t){.name="core", .value=core_id_str},
                                 NULL);
            metric_family_append(&fams[FAM_THERMAL_THROTTLE_CORE_TIME_SECONDS],
                                 VALUE_COUNTER_FLOAT64(thermal_throttle_cores[i].time/1000.0), NULL,
                                 &(label_pair_const_t){.name="core", .value=core_id_str},
                                 NULL);
        }
    }

    for (size_t i = 0; i < thermal_throttle_packages_num; i++) {
        if (thermal_throttle_packages[i].found) {
            char package_id_str[ITOA_MAX];
            uitoa(i, package_id_str);

            metric_family_append(&fams[FAM_THERMAL_THROTTLE_PACKAGE_COUNT],
                                 VALUE_COUNTER(thermal_throttle_packages[i].count), NULL,
                                 &(label_pair_const_t){.name="package", .value=package_id_str},
                                 NULL);
            metric_family_append(&fams[FAM_THERMAL_THROTTLE_PACKAGE_TIME_SECONDS],
                                 VALUE_COUNTER_FLOAT64(thermal_throttle_packages[i].time/1000.0), NULL,
                                 &(label_pair_const_t){.name="package", .value=package_id_str},
                                 NULL);
        }
    }

    plugin_dispatch_metric_family_array(fams, FAM_THERMAL_THROTTLE_MAX, 0);

    return 0;
}

static int thermal_throttle_init(void)
{
    path_sys_devices = plugin_syspath("devices/system/cpu");
    if (path_sys_devices == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    return 0;
}

static int thermal_throttle_shutdown(void)
{
    free(path_sys_devices);
    thermal_throttle_free();
    return 0;
}

void module_register(void)
{
    plugin_register_init("thermal_throttle", thermal_throttle_init);
    plugin_register_read("thermal_throttle", thermal_throttle_read);
    plugin_register_shutdown("thermal_throttle", thermal_throttle_shutdown);
}
