// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright 2018 Evgeny Naumov
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <stdint.h>
#include <stdio.h>

#pragma GCC diagnostic ignored "-Wpedantic"
#include <nvml.h>

enum {
    FAM_GPU_NVIDIA_MEMORY_FREE_BYTES,
    FAM_GPU_NVIDIA_MEMORY_USED_BYTES,
    FAM_GPU_NVIDIA_GPU_UTILIZATION_RATIO,
    FAM_GPU_NVIDIA_FAN_SPEED_RATIO,
    FAM_GPU_NVIDIA_TEMPERATURE_CELSIUS,
    FAM_GPU_NVIDIA_MULTIPROCESSOR_FREQUENCY_HZ,
    FAM_GPU_NVIDIA_MEMORY_FREQUENCY_HZ,
    FAM_GPU_NVIDIA_POWER_WATTS,
    FAM_GPU_NVIDIA_MAX,
};

static metric_family_t fams[FAM_GPU_NVIDIA_MAX] = {
    [FAM_GPU_NVIDIA_MEMORY_FREE_BYTES] = {
        .name = "gpu_nvidia_memory_free_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Unallocated FB memory in bytes",
    },
    [FAM_GPU_NVIDIA_MEMORY_USED_BYTES] = {
        .name = "gpu_nvidia_memory_used_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Allocated FB memory in bytes."
    },
    [FAM_GPU_NVIDIA_GPU_UTILIZATION_RATIO] = {
        .name = "gpu_nvidia_gpu_utilization_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "Percent of time over the past sample period "
                "during which one or more kernels was executing on the GPU."
    },
    [FAM_GPU_NVIDIA_FAN_SPEED_RATIO] = {
        .name = "gpu_nvidia_fan_speed_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "The fan speed is expressed as a percentage "
                "of the product's maximum noise tolerance fan speed."
    },
    [FAM_GPU_NVIDIA_TEMPERATURE_CELSIUS] = {
        .name = "gpu_nvidia_temperature_celsius",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current temperature readings for the device in celsius degrees."
    },
    [FAM_GPU_NVIDIA_MULTIPROCESSOR_FREQUENCY_HZ] = {
        .name = "gpu_nvidia_multiprocessor_frequency_hz",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current clock speed for the multiprocessor."
    },
    [FAM_GPU_NVIDIA_MEMORY_FREQUENCY_HZ] = {
        .name = "gpu_nvidia_memory_frequency_hz",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current clock speed for the memory."
    },
    [FAM_GPU_NVIDIA_POWER_WATTS] = {
        .name = "gpu_nvidia_power_watts",
        .type = METRIC_TYPE_GAUGE,
        .help = "Power usage for this GPU in watts and its associated circuitry."
    },
};

// This is a bitflag, necessitating the (extremely conservative) assumption
// that there are no more than 64 GPUs on this system.
static uint64_t conf_match_mask = 0;
static bool conf_mask_is_exclude = 0;

static int nvml_read(void)
{
    unsigned int device_count = 0;
    nvmlReturn_t nv_status = nvmlDeviceGetCount(&device_count);
    if (nv_status != NVML_SUCCESS) {
        PLUGIN_ERROR("Failed to enumerate NVIDIA GPUs (returned %u)", nv_status);
        return -1;
    }

    if (device_count > 64)
        device_count = 64;

    for (unsigned int idx = 0; idx < device_count; idx++) {
        unsigned int is_match = ((((uint64_t)1) << idx) & conf_match_mask)
                                || (conf_match_mask == 0);
        if (conf_mask_is_exclude == !!is_match)
            continue;

        nvmlDevice_t dev;
        nv_status = nvmlDeviceGetHandleByIndex(idx, &dev);
        if (nv_status != NVML_SUCCESS) {
            PLUGIN_WARNING("nvmlDeviceGetHandleByIndex failed (%u) at index %u", nv_status, idx);
            continue;
        }

        char dev_name[NVML_DEVICE_NAME_BUFFER_SIZE] = {0};
        nv_status = nvmlDeviceGetName(dev, dev_name, NVML_DEVICE_NAME_BUFFER_SIZE);
        if (nv_status != NVML_SUCCESS) {
            PLUGIN_WARNING("nvmlDeviceGetName failed (%u) at index %u", nv_status, idx);
            continue;
        }

        char device_index[64];
        snprintf(device_index, sizeof(device_index), "%u", idx);

        nvmlMemory_t meminfo;
        nv_status = nvmlDeviceGetMemoryInfo(dev, &meminfo);
        if (nv_status == NVML_SUCCESS) {
            metric_family_append(&fams[FAM_GPU_NVIDIA_MEMORY_FREE_BYTES],
                                 VALUE_GAUGE(meminfo.free), NULL,
                                 &(label_pair_const_t){.name="device_name", .value=dev_name},
                                 &(label_pair_const_t){.name="device_index", .value=device_index},
                                 NULL);
            metric_family_append(&fams[FAM_GPU_NVIDIA_MEMORY_USED_BYTES],
                                 VALUE_GAUGE(meminfo.used), NULL,
                                 &(label_pair_const_t){.name="device_name", .value=dev_name},
                                 &(label_pair_const_t){.name="device_index", .value=device_index},
                                 NULL);
        }

        nvmlUtilization_t utilization;
        nv_status = nvmlDeviceGetUtilizationRates(dev, &utilization);
        if (nv_status == NVML_SUCCESS)
            metric_family_append(&fams[FAM_GPU_NVIDIA_GPU_UTILIZATION_RATIO],
                                 VALUE_GAUGE((double)utilization.gpu / 100.0), NULL,
                                 &(label_pair_const_t){.name="device_name", .value=dev_name},
                                 &(label_pair_const_t){.name="device_index", .value=device_index},
                                 NULL);

        unsigned int fan_speed;
        nv_status = nvmlDeviceGetFanSpeed(dev, &fan_speed);
        if (nv_status == NVML_SUCCESS)
            metric_family_append(&fams[FAM_GPU_NVIDIA_FAN_SPEED_RATIO],
                                 VALUE_GAUGE((double)fan_speed / 100.0), NULL,
                                 &(label_pair_const_t){.name="device_name", .value=dev_name},
                                 &(label_pair_const_t){.name="device_index", .value=device_index},
                                 NULL);

        unsigned int core_temp;
        nv_status = nvmlDeviceGetTemperature(dev, NVML_TEMPERATURE_GPU, &core_temp);
        if (nv_status == NVML_SUCCESS)
            metric_family_append(&fams[FAM_GPU_NVIDIA_TEMPERATURE_CELSIUS],
                                 VALUE_GAUGE(core_temp), NULL,
                                 &(label_pair_const_t){.name="device_name", .value=dev_name},
                                 &(label_pair_const_t){.name="device_index", .value=device_index},
                                 NULL);

        unsigned int sm_clk_mhz;
        nv_status = nvmlDeviceGetClockInfo(dev, NVML_CLOCK_SM, &sm_clk_mhz);
        if (nv_status == NVML_SUCCESS)
            metric_family_append(&fams[FAM_GPU_NVIDIA_MULTIPROCESSOR_FREQUENCY_HZ],
                                 VALUE_GAUGE(1e6 * sm_clk_mhz), NULL,
                                 &(label_pair_const_t){.name="device_name", .value=dev_name},
                                 &(label_pair_const_t){.name="device_index", .value=device_index},
                                 NULL);

        unsigned int mem_clk_mhz;
        nv_status = nvmlDeviceGetClockInfo(dev, NVML_CLOCK_MEM, &mem_clk_mhz);
        if (nv_status == NVML_SUCCESS)
            metric_family_append(&fams[FAM_GPU_NVIDIA_MEMORY_FREQUENCY_HZ],
                                 VALUE_GAUGE(1e6 * mem_clk_mhz), NULL,
                                 &(label_pair_const_t){.name="device_name", .value=dev_name},
                                 &(label_pair_const_t){.name="device_index", .value=device_index},
                                 NULL);

        unsigned int power_mW;
        nv_status = nvmlDeviceGetPowerUsage(dev, &power_mW);
        if (nv_status == NVML_SUCCESS)
            metric_family_append(&fams[FAM_GPU_NVIDIA_POWER_WATTS],
                                 VALUE_GAUGE(1e-3 * power_mW), NULL,
                                 &(label_pair_const_t){.name="device_name", .value=dev_name},
                                 &(label_pair_const_t){.name="device_index", .value=device_index},
                                 NULL);

    }

    plugin_dispatch_metric_family_array(fams, FAM_GPU_NVIDIA_MAX, 0);
    return 0;
}

static int nvml_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "gpu-index") == 0) {
            unsigned int device_idx = 0;
            status = cf_util_get_unsigned_int(child, &device_idx);
            if (status == 0) {
                if (device_idx >= 64) {
                    PLUGIN_ERROR("At most 64 GPUs (0 <= GPUIndex < 64) are supported!");
                    return -1;
                }
                conf_match_mask |= (((uint64_t)1) << device_idx);
            }
        } else if (strcasecmp(child->key, "ignore-gpu-selected") == 0) {
            status =  cf_util_get_boolean(child, &conf_mask_is_exclude);
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

static int nvml_shutdown(void)
{
    nvmlReturn_t nv_status = nvmlShutdown();
    if (nv_status != NVML_SUCCESS) {
        PLUGIN_ERROR("nvmlShutdown failed with %u", nv_status);
        return -1;
    }
    return 0;
}

static int nvml_init(void)
{
    nvmlReturn_t nv_status = nvmlInit();
    if (nv_status != NVML_SUCCESS) {
        PLUGIN_ERROR("nvmlInit failed with %u", nv_status);
        return -1;
    }
    return 0;
}

void module_register(void)
{
    plugin_register_init("gpu_nvidia", nvml_init);
    plugin_register_config("gpu_nvidia", nvml_config);
    plugin_register_read("gpu_nvidia", nvml_read);
    plugin_register_shutdown("gpu_nvidia", nvml_shutdown);
}
