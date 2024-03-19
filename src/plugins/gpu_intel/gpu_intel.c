// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright(c) 2020-2023 Intel Corporation. All rights reserved.
// SPDX-FileContributor: Eero Tamminen <eero.t.tamminen at intel.com>

/*
 * See:
 * - https://spec.oneapi.com/level-zero/latest/sysman/PROG.html
 * - https://spec.oneapi.io/level-zero/latest/sysman/api.html
 *
 * Error handling:
 * - All Sysman API call errors are logged
 * - Sysman errors cause plugin initialization failure only when
 *   no GPU devices (with PCI ID) are available
 * - Sysman errors in metric queries cause just given metric to be
 *      disabled for given GPU
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <level_zero/ze_api.h>
#include <level_zero/zes_api.h>

/* whether to add "dev_file" label to metrics for Kubernetes Intel GPU plugin,
 * needs (POSIX.1-2001) basename() + glob() and (POSIX.1-2008) getline()
 * functions.
 */
#define ADD_DEV_FILE 1
#if ADD_DEV_FILE
#include <glob.h>
#include <libgen.h>
#endif

#include "plugin.h"
#include "libutils/common.h"

/* collectd plugin API callback finished OK */
#define RET_OK                    0
/* plugin specific callback error return values */
#define RET_NO_METRICS           -1
#define RET_INVALID_CONFIG       -2
#define RET_ZE_INIT_FAIL         -3
#define RET_NO_DRIVERS           -4
#define RET_ZE_DRIVER_GET_FAIL   -5
#define RET_ZE_DEVICE_GET_FAIL   -6
#define RET_ZE_DEVICE_PROPS_FAIL -7
#define RET_NO_GPUS              -9

/* handles for the GPU devices discovered by Sysman library */
typedef struct {
    /* GPU info for metric labels */
    char *pci_bdf;  // required
    char *pci_dev;  // if GpuInfo
    char *dev_file; // if ADD_DEV_FILE
    /* number of types for metrics without allocs */
    uint32_t ras_count;
    uint32_t temp_count;
    /* number of types for each counter metric */
    uint32_t engine_count;
    uint32_t fabric_count;
    uint32_t membw_count;
    uint32_t power_count;
    uint32_t throttle_count;
    /* number of types for each sampled metric */
    uint32_t frequency_count;
    uint32_t memory_count;
    /* previous values for counters, must have matching <name>_count */
    zes_engine_stats_t *engine;
    zes_fabric_port_throughput_t *fabric;
    zes_mem_bandwidth_t *membw;
    zes_power_energy_counter_t *power;
    zes_freq_throttle_time_t *throttle;
    /* types * samples sized array of values, used for aggregate outputs */
    zes_freq_state_t **frequency;
    zes_mem_state_t **memory;
    /* GPU  specific flags */
    uint64_t flags;
    zes_device_handle_t handle;
    /* how many times metrics have been checked */
    uint64_t check_count;
} gpu_device_t;

typedef enum {
    OUTPUT_BASE  = (1 << 0),
    OUTPUT_RATE  = (1 << 1),
    OUTPUT_RATIO = (1 << 2),
    OUTPUT_ALL   = (OUTPUT_BASE | OUTPUT_RATE | OUTPUT_RATIO)
} output_t;

static const struct {
    const char *name;
    output_t value;
} metrics_output[] = {
    {"base",  OUTPUT_BASE },
    {"rate",  OUTPUT_RATE },
    {"ratio", OUTPUT_RATIO}
};

static gpu_device_t *gpus;
static uint32_t gpu_count;
static struct {
    bool gpuinfo;
    uint64_t flags;
    output_t output;
    uint32_t samples;
} config;

enum {
    COLLECT_ENGINE           = (1 <<  0),
    COLLECT_ENGINE_SINGLE    = (1 <<  1),
    COLLECT_FABRIC           = (1 <<  2),
    COLLECT_FREQUENCY        = (1 <<  3),
    COLLECT_MEMORY           = (1 <<  4),
    COLLECT_MEMORY_BANDWIDTH = (1 <<  5),
    COLLECT_POWER            = (1 <<  6),
    COLLECT_POWER_RATIO      = (1 <<  7),
    COLLECT_ERRORS           = (1 <<  9),
    COLLECT_SEPARATE_ERRORS  = (1 <<  9),
    COLLECT_TEMPERATURE      = (1 << 10),
    COLLECT_THROTTLETIME     = (1 << 11),
};

static cf_flags_t gpu_intel_flags[] = {
    { "engine",           COLLECT_ENGINE           },
    { "engine_single",    COLLECT_ENGINE_SINGLE    },
    { "fabric",           COLLECT_FABRIC           },
    { "frequency",        COLLECT_FREQUENCY        },
    { "memory",           COLLECT_MEMORY           },
    { "memory_bandwidth", COLLECT_MEMORY_BANDWIDTH },
    { "power",            COLLECT_POWER            },
    { "power_ratio",      COLLECT_POWER_RATIO      },
    { "errors",           COLLECT_ERRORS           },
    { "separate_errors",  COLLECT_SEPARATE_ERRORS  },
    { "temperature",      COLLECT_TEMPERATURE      },
    { "throttle_time",    COLLECT_THROTTLETIME     },
};
static size_t gpu_intel_flags_size = STATIC_ARRAY_SIZE(gpu_intel_flags);

#define MAX_SAMPLES        64

/* Free array of arrays allocated with gpu_subarray_realloc().
 *
 * config.samples must not have changed since allocation, because
 * that determines the number of allocated subarrays
 */
static bool gpu_subarray_free(void **mem)
{
    if (!mem)
        return false;

    for (uint32_t i = 0; i < config.samples; i++) {
        free(mem[i]);
        mem[i] = NULL;
    }
    free(mem);

    return true;
}

/* Allocate 'config.samples' sized array of 'count' sized arrays having 'size'
 * sized items.  If given array is already allocated, it and its subarrays
 * is freed first
 */
static void **gpu_subarray_realloc(void **mem, int count, int size)
{
    gpu_subarray_free(mem);
    mem = malloc(config.samples * sizeof(void *));
    if (mem == NULL) {
        PLUGIN_ERROR("malloc failed.");
        return NULL;
    }

    for (uint32_t i = 0; i < config.samples; i++) {
        // (s)calloc used so pointers in structs are initialized to
        // NULLs for Sysman metric state/property Get calls
        mem[i] = calloc(count, size);
        if (mem[i] == NULL) {
            PLUGIN_ERROR("calloc failed.");
            gpu_subarray_free(mem);
            return NULL;
        }
    }
    return mem;
}

/* Free GPU allocations and zero counters
 *
 * Return RET_OK for shutdown callback success
 */
static int gpu_config_free(void)
{
#define FREE_GPU_ARRAY(i, member)                            \
    if (gpus[i].member) {                                    \
        free(gpus[i].member);                                \
        gpus[i].member##_count = 0;                          \
        gpus[i].member = NULL;                               \
    }
#define FREE_GPU_SAMPLING_ARRAYS(i, member)                  \
    if (gpus[i].member) {                                    \
        gpu_subarray_free((void **)gpus[i].member);          \
        gpus[i].member##_count = 0;                          \
        gpus[i].member = NULL;                               \
    }
    if (!gpus) {
        /* gpu_init() should have failed with no GPUs, so no need for this */
        PLUGIN_WARNING("gpu_config_free() (shutdown) called with no GPUs initialized");
        return RET_NO_GPUS;
    }
    for (uint32_t i = 0; i < gpu_count; i++) {
        /* free previous values for counters & zero their counts */
        FREE_GPU_ARRAY(i, engine);
        FREE_GPU_ARRAY(i, fabric);
        FREE_GPU_ARRAY(i, membw);
        FREE_GPU_ARRAY(i, power);
        FREE_GPU_ARRAY(i, throttle);
        /* and similar for sampling arrays */
        FREE_GPU_SAMPLING_ARRAYS(i, frequency);
        FREE_GPU_SAMPLING_ARRAYS(i, memory);
        /* zero rest of counters & free name */
        gpus[i].ras_count = 0;
        gpus[i].temp_count = 0;
        free(gpus[i].pci_bdf);
        gpus[i].pci_bdf = NULL;
        free(gpus[i].pci_dev);
        gpus[i].pci_dev = NULL;
        free(gpus[i].dev_file);
        gpus[i].dev_file = NULL;
    }
#undef FREE_GPU_SAMPLING_ARRAYS
#undef FREE_GPU_ARRAY
    free(gpus);
    gpus = NULL;
    return RET_OK;
}

/* show plugin GPU metrics config options, return RET_OK
 * if at least some metric is enabled, otherwise error code
 */
static int gpu_config_check(void)
{
    if (!config.output) {
        config.output = OUTPUT_ALL;
    }

    if (config.gpuinfo) {
        PLUGIN_INFO("- 'samples': %u", config.samples);

        double interval = CDTIME_T_TO_DOUBLE(plugin_get_interval());
        if (config.samples > 1) {
            PLUGIN_INFO("- internal sampling interval: %.2fs", interval);
            PLUGIN_INFO("- query / aggregation submit interval: %.2fs", config.samples * interval);
        } else {
            PLUGIN_INFO("- query / submit interval: %.2fs", interval);
        }

        unsigned i;
        PLUGIN_INFO("metrics-output'' variants:");
        for (i = 0; i < STATIC_ARRAY_SIZE(metrics_output); i++) {
            if (config.output & metrics_output[i].value) {
                PLUGIN_INFO("- %s", metrics_output[i].name);
            }
        }
    }

    return RET_OK;
}

/* Set GPU specific flags to initial global configuration values
 * for each GPU.    Allocations of metrics arrays are done when metrics
 * are queried for the first time (not here), and re-allocated if
 * number of types for given metric changes.
 *
 * Return RET_OK if config is OK, (negative) error value otherwise
 */
static int gpu_config_init(unsigned int count)
{
    if (!config.samples) {
        config.samples = 1;
    }
    if (gpu_config_check()) {
        gpu_config_free();
        return RET_NO_METRICS;
    }
    unsigned int i;
    for (i = 0; i < count; i++) {
        gpus[i].flags = config.flags;
        gpus[i].check_count = 0;
    }
    gpu_count = count;
    return RET_OK;
}

/* log given UUID (without dashes):
 * https://en.wikipedia.org/wiki/Universally_unique_identifier
 */
static void log_uuid(const char *prefix, const uint8_t *byte, int len)
{
    int offset = strlen(prefix);
    char buf[offset + 2 * len + 1];
    sstrncpy(buf, prefix, sizeof(buf));
    while (len-- > 0) {
        sprintf(buf + offset, "%02x", *byte++);
        offset += 2;
    }
    PLUGIN_INFO("%s", buf);
}

/* If GPU info setting is enabled, log Sysman API provided info for
 * given GPU, and set PCI device ID to 'pci_dev'.  On success, return
 * true and set GPU PCI address to 'pci_bdf' as string in BDF notation:
 * https://wiki.xen.org/wiki/Bus:Device.Function_(BDF)_Notation
 */
static bool gpu_info(zes_device_handle_t dev, char **pci_bdf, char **pci_dev)
{
    char buf[32];

    *pci_bdf = *pci_dev = NULL;
    zes_pci_properties_t pci = {.pNext = NULL};
    ze_result_t ret = zesDevicePciGetProperties(dev, &pci);
    if (ret == ZE_RESULT_SUCCESS) {
        const zes_pci_address_t *addr = &pci.address;
        ssnprintf(buf, sizeof(buf), "%04x:%02x:%02x.%x", addr->domain, addr->bus,
                         addr->device, addr->function);
    } else {
        PLUGIN_ERROR("failed to get GPU PCI device properties => 0x%x", ret);
        return false;
    }
    *pci_bdf = sstrdup(buf);
    if (!config.gpuinfo) {
        return true;
    }

    PLUGIN_INFO("Level-Zero Sysman API GPU info");
    PLUGIN_INFO("==============================");

    PLUGIN_INFO("PCI info:");
    if (ret == ZE_RESULT_SUCCESS) {
        PLUGIN_INFO("- PCI B/D/F:  %s", *pci_bdf);
        const zes_pci_speed_t *speed = &pci.maxSpeed;
        PLUGIN_INFO("- PCI gen:        %d", speed->gen);
        PLUGIN_INFO("- PCI width:  %d", speed->width);
        double max = speed->maxBandwidth / (double)(1024 * 1024 * 1024);
        PLUGIN_INFO("- max BW:         %.2f GiB/s (all lines)", max);
    } else {
        PLUGIN_INFO("- unavailable");
    }

    PLUGIN_INFO("HW state:");
    zes_device_state_t state = {.pNext = NULL};
    /* Note: there's also zesDevicePciGetState() for PCI link status */
    ret = zesDeviceGetState(dev, &state);
    if (ret == ZE_RESULT_SUCCESS) {
        PLUGIN_INFO("- repaired: %s",
                 (state.repaired == ZES_REPAIR_STATUS_PERFORMED) ? "yes" : "no");
        if (state.reset != 0) {
            PLUGIN_INFO("- device RESET required");
            if (state.reset & ZES_RESET_REASON_FLAG_WEDGED) {
                PLUGIN_INFO(" - HW is wedged");
            }
            if (state.reset & ZES_RESET_REASON_FLAG_REPAIR) {
                PLUGIN_INFO(" - HW needs to complete repairs");
            }
        } else {
            PLUGIN_INFO("- no RESET required");
        }
    } else {
        PLUGIN_INFO("- unavailable");
        PLUGIN_WARNING( ": failed to get GPU device state => 0x%x", ret);
    }

    const char *eccstate = "unavailable";
    zes_device_ecc_properties_t ecc = {.pNext = NULL};
    if (zesDeviceGetEccState(dev, &ecc) == ZE_RESULT_SUCCESS) {
        switch (ecc.currentState) {
        case ZES_DEVICE_ECC_STATE_ENABLED:
            eccstate = "enabled";
            break;
        case ZES_DEVICE_ECC_STATE_DISABLED:
            eccstate = "disabled";
            break;
        default:
            break;
        }
    }
    PLUGIN_INFO("- ECC state: %s", eccstate);

    PLUGIN_INFO("HW identification:");
    zes_device_properties_t props = {.pNext = NULL};
    ret = zesDeviceGetProperties(dev, &props);
    if (ret == ZE_RESULT_SUCCESS) {
        const ze_device_properties_t *core = &props.core;
        snprintf(buf, sizeof(buf), "0x%x", core->deviceId);
        *pci_dev = sstrdup(buf); // used only if present
        PLUGIN_INFO("- name:               %s", core->name);
        PLUGIN_INFO("- vendor ID:  0x%x", core->vendorId);
        PLUGIN_INFO("- device ID:  0x%x", core->deviceId);
        log_uuid("- UUID:               0x", core->uuid.id, sizeof(core->uuid.id));
        PLUGIN_INFO("- serial#:        %s", props.serialNumber);
        PLUGIN_INFO("- board#:         %s", props.boardNumber);
        PLUGIN_INFO("- brand:          %s", props.brandName);
        PLUGIN_INFO("- model:          %s", props.modelName);
        PLUGIN_INFO("- vendor:         %s", props.vendorName);

        PLUGIN_INFO("UMD/KMD driver info:");
        PLUGIN_INFO("- version:        %s", props.driverVersion);
        PLUGIN_INFO("- max alloc:  %lu MiB", core->maxMemAllocSize / (1024 * 1024));

        PLUGIN_INFO("HW info:");
        PLUGIN_INFO("- # sub devs: %u", props.numSubdevices);
        PLUGIN_INFO("- core clock: %u", core->coreClockRate);
        PLUGIN_INFO("- EUs:                %u", core->numEUsPerSubslice *
                                                core->numSubslicesPerSlice * core->numSlices);
    } else {
        PLUGIN_INFO("- unavailable");
        PLUGIN_WARNING("failed to get GPU device properties => 0x%x", ret);
    }

    /* HW info for all memories */
    uint32_t i, mem_count = 0;
    ze_device_handle_t mdev = (ze_device_handle_t)dev;
    ret = zeDeviceGetMemoryProperties(mdev, &mem_count, NULL);
    if (ret != ZE_RESULT_SUCCESS) {
        PLUGIN_WARNING("failed to get memory properties count => 0x%x", ret);
        return true;
    }

    ze_device_memory_properties_t *mems = calloc(mem_count, sizeof(*mems));
    if (mems == NULL) {
        PLUGIN_WARNING("calloc failed");
        return true;
    }

    ret = zeDeviceGetMemoryProperties(mdev, &mem_count, mems);
    if (ret != ZE_RESULT_SUCCESS) {
        PLUGIN_WARNING("failed to get %u memory properties => 0x%x", mem_count, ret);
        free(mems);
        return true;
    }

    for (i = 0; i < mem_count; i++) {
        const char *memname = mems[i].name;
        if (!(memname && *memname)) {
            memname = "Unknown";
        }
        PLUGIN_INFO("Memory - %s:", memname);
        PLUGIN_INFO("- size:               %lu MiB", mems[i].totalSize / (1024 * 1024));
        PLUGIN_INFO("- bus width:  %u", mems[i].maxBusWidth);
        PLUGIN_INFO("- max clock:  %u", mems[i].maxClockRate);
    }
    free(mems);
    return true;
}

/* Add (given) BDF string and device file name to GPU struct for metric labels.
 * Return false if (required) BDF string is missing, true otherwise.
 */
static bool add_gpu_labels(gpu_device_t *gpu, zes_device_handle_t dev)
{
    assert(gpu);
    char *pci_bdf = NULL;
    char *pci_dev = NULL;
    if (!gpu_info(dev, &pci_bdf, &pci_dev) || !pci_bdf) {
        free(pci_bdf);
        free(pci_dev);
        return false;
    }
    gpu->pci_bdf = pci_bdf;
    gpu->pci_dev = pci_dev;
    /*
     * scan devfs and sysfs to find primary GPU device file node matching
     * given BDF, and if one is found, use that as device file name.
     *
     * NOTE: scanning can log only INFO messages, because ERRORs and WARNINGs
     * would FAIL unit test that are run as part of build, if build environment
     * has no GPU access.
     */
#if ADD_DEV_FILE
#define BDF_LINE "PCI_SLOT_NAME="
#define DEVFS_GLOB "/dev/dri/card*"
    glob_t devfs;
    if (glob(DEVFS_GLOB, 0, NULL, &devfs) != 0) {
        PLUGIN_INFO(" device <-> BDF mapping, no matches for: " DEVFS_GLOB);
        globfree(&devfs);
        return true;
    }
    const size_t prefix_size = strlen(BDF_LINE);
    for (size_t i = 0; i < devfs.gl_pathc; i++) {
        char path[PATH_MAX], *dev_file;
        dev_file = basename(devfs.gl_pathv[i]);

        snprintf(path, sizeof(path), "/sys/class/drm/%s/device/uevent", dev_file);
        FILE *fp = fopen(path, "r");
        if (fp == NULL) {
            PLUGIN_INFO("device <-> BDF mapping, file missing: %s", path);
            continue;
        }
        ssize_t nread;
        size_t len = 0;
        char *line = NULL;
        while ((nread = getline(&line, &len, fp)) > 0) {
            if (strncmp(line, BDF_LINE, prefix_size) != 0) {
                continue;
            }
            line[nread - 1] = '\0'; // remove newline
            if (strcmp(line + prefix_size, pci_bdf) == 0) {
                PLUGIN_INFO(" %s <-> %s", dev_file, pci_bdf);
                gpu->dev_file = sstrdup(dev_file);
                break;
            }
        }
        free(line);
        fclose(fp);
        if (gpu->dev_file) {
            break;
        }
    }
    globfree(&devfs);
#undef DEVFS_GLOB
#undef BDF_LINE
#endif
    return true;
}

/* Scan how many GPU devices Sysman reports in total, and set 'scan_count'
 * accordingly
 *
 * Return RET_OK for success, or (negative) error value if any of the device
 * count queries fails
 */
static int gpu_scan(ze_driver_handle_t *drivers, uint32_t driver_count, uint32_t *scan_count)
{
    assert(!gpus);
    *scan_count = 0;
    for (uint32_t drv_idx = 0; drv_idx < driver_count; drv_idx++) {
        uint32_t dev_count = 0;
        ze_result_t ret = zeDeviceGet(drivers[drv_idx], &dev_count, NULL);
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("failed to get device count for driver %u => 0x%x", drv_idx, ret);
            return RET_ZE_DEVICE_GET_FAIL;
        }
        if (config.gpuinfo) {
            PLUGIN_INFO("driver %u: %u devices", drv_idx, dev_count);
        }
        *scan_count += dev_count;
    }
    if (!*scan_count) {
        PLUGIN_ERROR("scan for GPU devices failed");
        return RET_NO_GPUS;
    }
    if (config.gpuinfo)
        PLUGIN_INFO("scan: %u GPUs in total from %u L0 drivers", *scan_count, driver_count);
    return RET_OK;
}

/* Allocate 'scan_count' GPU structs to 'gpus' and fetch Sysman handle & name
 * for them.
 *
 * Counts of still found & ignored GPUs are set to 'scan_count' and
 * 'scan_ignored' arguments before returning.
 *
 * Return RET_OK for success if at least one GPU device info fetch succeeded,
 * otherwise (negative) error value for last error encountered
 */
static int gpu_fetch(ze_driver_handle_t *drivers, uint32_t driver_count,
                     uint32_t *scan_count, uint32_t *scan_ignored)
{
    assert(!gpus);
    assert(*scan_count > 0);

    gpus = calloc(*scan_count, sizeof(*gpus));
    if (gpus == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return RET_ZE_DEVICE_GET_FAIL;
    }

    uint32_t ignored = 0, count = 0;
    int retval = RET_NO_GPUS;
    ze_result_t ret;

    for (uint32_t drv_idx = 0; drv_idx < driver_count; drv_idx++) {
        uint32_t dev_count = 0;
        ret = zeDeviceGet(drivers[drv_idx], &dev_count, NULL);
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("failed to get device count for driver %u => 0x%x", drv_idx, ret);
            retval = RET_ZE_DEVICE_GET_FAIL;
            continue;
        }

        ze_device_handle_t *devs = calloc(dev_count, sizeof(*devs));
        if (devs == NULL) {
            PLUGIN_ERROR("calloc failed.");
            return RET_ZE_DEVICE_GET_FAIL;
        }

        ret = zeDeviceGet(drivers[drv_idx], &dev_count, devs);
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("failed to get %u devices for driver %u => 0x%x", dev_count, drv_idx, ret);
            free(devs);
            devs = NULL;
            retval = RET_ZE_DEVICE_GET_FAIL;
            continue;
        }
        /* Get all GPU devices for the driver */
        for (uint32_t dev_idx = 0; dev_idx < dev_count; dev_idx++) {
            ze_device_properties_t props = {.pNext = NULL};
            ret = zeDeviceGetProperties(devs[dev_idx], &props);
            if (ret != ZE_RESULT_SUCCESS) {
                PLUGIN_ERROR("failed to get driver %u device %u properties => 0x%x",
                             drv_idx, dev_idx, ret);
                retval = RET_ZE_DEVICE_PROPS_FAIL;
                continue;
            }
            assert(ZE_DEVICE_TYPE_GPU == props.type);
            if (count >= *scan_count) {
                ignored++;
                continue;
            }
            gpus[count].handle = (zes_device_handle_t)devs[dev_idx];
            if (!add_gpu_labels(&(gpus[count]), devs[dev_idx])) {
                PLUGIN_ERROR("failed to get driver %u device %u information", drv_idx, dev_idx);
                ignored++;
                continue;
            }
            count++;
        }
        free(devs);
        devs = NULL;
    }

    if (count > 0) {
        retval = RET_OK;
        if (config.gpuinfo) {
            PLUGIN_INFO("fetch: %u/%u GPUs in total from %u L0 drivers", count, *scan_count,
                         driver_count);
        }
    } else {
        PLUGIN_ERROR("fetch for GPU devices failed");
        gpu_config_free();
    }

    *scan_ignored = ignored;
    *scan_count = count;
    return retval;
}

/* Scan Sysman for GPU devices
 * Return RET_OK for success, (negative) error value otherwise
 */
static int gpu_init(void)
{
    if (gpus) {
        PLUGIN_NOTICE("skipping extra gpu_init() call");
        return RET_OK;
    }

    setenv("ZES_ENABLE_SYSMAN", "1", 1);
    ze_result_t ret = zeInit(ZE_INIT_FLAG_GPU_ONLY);
    if (ret != ZE_RESULT_SUCCESS) {
        PLUGIN_ERROR("Level Zero API init failed => 0x%x", ret);
        return RET_ZE_INIT_FAIL;
    }
    /* Discover all the drivers */
    uint32_t driver_count = 0;
    ret = zeDriverGet(&driver_count, NULL);
    if (ret != ZE_RESULT_SUCCESS) {
        PLUGIN_ERROR("failed to get L0 GPU drivers count => 0x%x", ret);
        return RET_ZE_DRIVER_GET_FAIL;
    }
    if (!driver_count) {
        PLUGIN_ERROR("no drivers found with Level-Zero Sysman API");
        return RET_NO_DRIVERS;
    }

    ze_driver_handle_t *drivers = calloc(driver_count, sizeof(*drivers));
    if (drivers == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return RET_ZE_DRIVER_GET_FAIL;
    }

    ret = zeDriverGet(&driver_count, drivers);
    if (ret != ZE_RESULT_SUCCESS) {
        PLUGIN_ERROR("failed to get %u L0 drivers => 0x%x", driver_count, ret);
        free(drivers);
        return RET_ZE_DRIVER_GET_FAIL;
    }
    /* scan number of Sysman provided GPUs... */
    int fail;
    uint32_t count;
    if ((fail = gpu_scan(drivers, driver_count, &count)) < 0) {
        free(drivers);
        return fail;
    }
    uint32_t ignored = 0, scanned = count;
    if (count) {
        /* ...and allocate & fetch data for them */
        if ((fail = gpu_fetch(drivers, driver_count, &count, &ignored)) < 0) {
            free(drivers);
            return fail;
        }
    }
    free(drivers);

    if (scanned > count)
        PLUGIN_WARNING("%u GPUs disappeared after first scan", scanned - count);
    if (ignored)
        PLUGIN_WARNING("%u GPUs appeared after first scan (are ignored)", ignored);
    if (!count) {
        PLUGIN_ERROR("no GPU devices found with Level-Zero Sysman API");
        return RET_NO_GPUS;
    }

    return gpu_config_init(count);
}

/* Add device labels to all metrics in given metric family and submit family to
 * collectd.    Resets metric family after dispatch */
static void gpu_submit(gpu_device_t *gpu, metric_family_t *fam)
{
    metric_t *m = fam->metric.ptr;
    for (size_t i = 0; i < fam->metric.num; i++) {
        metric_label_set(m + i, "pci_bdf", gpu->pci_bdf);
        if (gpu->dev_file) {
            metric_label_set(m + i, "dev_file", gpu->dev_file);
        }
        if (gpu->pci_dev) {
            metric_label_set(m + i, "pci_dev", gpu->pci_dev);
        }
    }
    int status = plugin_dispatch_metric_family(fam, 0);
    if (status != 0)
        PLUGIN_ERROR("gpu_submit(%s, %s) failed: %s", gpu->pci_bdf, fam->name, strerror(status));
    metric_family_metric_reset(fam);
}

/* because of family name change, each RAS metric needs to be submitted +
 * reseted separately */
static void ras_submit(gpu_device_t *gpu, const char *name, const char *help,
                                          const char *type, const char *subdev, double value)
{
    metric_family_t fam = {
            .type = METRIC_TYPE_COUNTER,
            /*
             * String literals are const, so they are passed as such to
             * here, but .name & .help members are not, so casts are
             * necessary.
             *
             * Note that same casts happen implicitly when string
             * literals are assigned directly to these members, GCC
             * just does not warn about that unless "-Write-strings"
             * warning is enabled, which is NOT part of even "-Wall
             * -Wextra".
             *
             * This cast is safe as long as metric_family_free() is not
             * called on these families (which is the case).
             */
            .name = discard_const(name),
            .help = discard_const(help),
    };
    metric_t m = {0};

    m.value = VALUE_COUNTER_FLOAT64(value);
    if (type) {
        metric_label_set(&m, "type", type);
    }
    if (subdev) {
        metric_label_set(&m, "sub_dev", subdev);
    }
    metric_family_metric_append(&fam, m);
    metric_reset(&m, METRIC_TYPE_COUNTER);
    gpu_submit(gpu, &fam);
}

/* Report error set types, return true for success */
static bool gpu_ras(gpu_device_t *gpu)
{
    uint32_t ras_count = 0;
    zes_device_handle_t dev = gpu->handle;
    ze_result_t ret = zesDeviceEnumRasErrorSets(dev, &ras_count, NULL);
    if (ret != ZE_RESULT_SUCCESS) {
        PLUGIN_ERROR("failed to get RAS error sets count => 0x%x", ret);
        return false;
    }

    zes_ras_handle_t *ras = calloc(ras_count, sizeof(*ras));
    if (ras == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return false;
    }

    ret = zesDeviceEnumRasErrorSets(dev, &ras_count, ras);
    if (ret != ZE_RESULT_SUCCESS) {
        PLUGIN_ERROR("failed to get %u RAS error sets => 0x%x", ras_count, ret);
        free(ras);
        return false;
    }
    if (gpu->ras_count != ras_count) {
        PLUGIN_INFO(" Sysman reports %u RAS error sets", ras_count);
        gpu->ras_count = ras_count;
    }

    bool ok = false;
    for (uint32_t i = 0; i < ras_count; i++) {
        zes_ras_properties_t props = {.pNext = NULL};
        ret = zesRasGetProperties(ras[i], &props);
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("failed to get RAS set %u properties => 0x%x", i, ret);
            ok = false;
            break;
        }
        const char *type;
        switch (props.type) {
        case ZES_RAS_ERROR_TYPE_CORRECTABLE:
            type = "correctable";
            break;
        case ZES_RAS_ERROR_TYPE_UNCORRECTABLE:
            type = "uncorrectable";
            break;
        default:
            type = "unknown";
        }
        char buf[12];
        const char *subdev = NULL;
        if (props.onSubdevice) {
            ssnprintf(buf, sizeof(buf), "%u", props.subdeviceId);
            subdev = buf;
        }
        const bool clear = false;
        zes_ras_state_t values = {.pNext = NULL};
        ret = zesRasGetState(ras[i], clear, &values);
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("failed to get RAS set %u (%s) state => 0x%x", i, type, ret);
            ok = false;
            break;
        }

        bool correctable;
        uint64_t value, total = 0;
        const char *catname, *help;
        for (int cat_idx = 0; cat_idx < ZES_MAX_RAS_ERROR_CATEGORY_COUNT; cat_idx++) {
            value = values.category[cat_idx];
            total += value;
            if (!(gpu->flags & COLLECT_SEPARATE_ERRORS))
                continue;
            correctable = true;
            switch (cat_idx) {
                // categories which are not correctable, see:
                // https://spec.oneapi.io/level-zero/latest/sysman/PROG.html#querying-ras-errors
            case ZES_RAS_ERROR_CAT_RESET:
                help = "Total count of HW accelerator resets attempted by the driver";
                catname = "gpu_intel_resets_total";
                correctable = false;
                break;
            case ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS:
                help = "Total count of (non-correctable) HW exceptions generated by the "
                       "way workloads program the HW";
                catname = "gpu_intel_programming_errors_total";
                correctable = false;
                break;
            case ZES_RAS_ERROR_CAT_DRIVER_ERRORS:
                help = "total count of (non-correctable) low-level driver communication "
                       "errors";
                catname = "gpu_intel_driver_errors_total";
                correctable = false;
                break;
                // categories which can have both correctable and uncorrectable errors
            case ZES_RAS_ERROR_CAT_COMPUTE_ERRORS:
                help = "Total count of errors that have occurred in the (shader) "
                       "accelerator HW";
                catname = "gpu_intel_compute_errors_total";
                break;
            case ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS:
                help = "Total count of errors that have occurred in the fixed-function "
                             "accelerator HW";
                catname = "gpu_intel_fixed_function_errors_total";
                break;
            case ZES_RAS_ERROR_CAT_CACHE_ERRORS:
                help = "Total count of ECC errors that have occurred in the on-chip "
                             "caches";
                catname = "gpu_intel_cache_errors_total";
                break;
            case ZES_RAS_ERROR_CAT_DISPLAY_ERRORS:
                help = "Total count of ECC errors that have occurred in the display";
                catname = "gpu_intel_display_errors_total";
                break;
            default:
                help = "Total count of errors in unsupported categories";
                catname = "gpu_intel_unknown_errors_total";
            }
            if (correctable) {
                ras_submit(gpu, catname, help, type, subdev, value);
            } else if (props.type == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
                ras_submit(gpu, catname, help, NULL, subdev, value);
            }
        }
        catname = "gpu_intel_all_errors_total";
        help = "Total count of errors in all categories";
        ras_submit(gpu, catname, help, type, subdev, total);
        ok = true;
    }
    free(ras);
    return ok;
}

static void metric_set_subdev(metric_t *m, bool onsub, uint32_t subid)
{
    if (onsub) {
        char buf[12];
        ssnprintf(buf, sizeof(buf), "%u", subid);
        metric_label_set(m, "sub_dev", buf);
    }
}

/* set memory metric labels based on its properties, return ZE_RESULT_SUCCESS
 * for success
 */
static ze_result_t set_mem_labels(zes_mem_handle_t mem, metric_t *metric)
{
    zes_mem_properties_t props = {.pNext = NULL};
    ze_result_t ret = zesMemoryGetProperties(mem, &props);
    if (ret != ZE_RESULT_SUCCESS)
        return ret;

    const char *location;
    switch (props.location) {
    case ZES_MEM_LOC_SYSTEM:
        location = "system";
        break;
    case ZES_MEM_LOC_DEVICE:
        location = "device";
        break;
    default:
        location = "unknown";
    }

    const char *type;
    switch (props.type) {
    case ZES_MEM_TYPE_HBM:
        type = "HBM";
        break;
    case ZES_MEM_TYPE_DDR:
        type = "DDR";
        break;
    case ZES_MEM_TYPE_DDR3:
        type = "DDR3";
        break;
    case ZES_MEM_TYPE_DDR4:
        type = "DDR4";
        break;
    case ZES_MEM_TYPE_DDR5:
        type = "DDR5";
        break;
    case ZES_MEM_TYPE_LPDDR:
        type = "LPDDR";
        break;
    case ZES_MEM_TYPE_LPDDR3:
        type = "LPDDR3";
        break;
    case ZES_MEM_TYPE_LPDDR4:
        type = "LPDDR4";
        break;
    case ZES_MEM_TYPE_LPDDR5:
        type = "LPDDR5";
        break;
    case ZES_MEM_TYPE_GDDR4:
        type = "GDDR4";
        break;
    case ZES_MEM_TYPE_GDDR5:
        type = "GDDR5";
        break;
    case ZES_MEM_TYPE_GDDR5X:
        type = "GDDR5X";
        break;
    case ZES_MEM_TYPE_GDDR6:
        type = "GDDR6";
        break;
    case ZES_MEM_TYPE_GDDR6X:
        type = "GDDR6X";
        break;
    case ZES_MEM_TYPE_GDDR7:
        type = "GDDR7";
        break;
    case ZES_MEM_TYPE_SRAM:
        type = "SRAM";
        break;
    case ZES_MEM_TYPE_L1:
        type = "L1";
        break;
    case ZES_MEM_TYPE_L3:
        type = "L3";
        break;
    case ZES_MEM_TYPE_GRF:
        type = "GRF";
        break;
    case ZES_MEM_TYPE_SLM:
        type = "SLM";
        break;
    default:
        type = "unknown";
    }

    metric_label_set(metric, "type", type);
    metric_label_set(metric, "location", location);
    metric_set_subdev(metric, props.onSubdevice, props.subdeviceId);
    return ZE_RESULT_SUCCESS;
}

/* Report memory usage for memory modules, return true for success.
 *
 * See gpu_read() on 'cache_idx' usage.
 */
static bool gpu_mems(gpu_device_t *gpu, unsigned int cache_idx)
{
    if (!(config.output & (OUTPUT_BASE | OUTPUT_RATIO))) {
        PLUGIN_ERROR("no memory output variants selected");
        return false;
    }

    uint32_t mem_count = 0;
    zes_device_handle_t dev = gpu->handle;
    ze_result_t ret = zesDeviceEnumMemoryModules(dev, &mem_count, NULL);
    if (ret != ZE_RESULT_SUCCESS) {
        PLUGIN_ERROR("failed to get memory modules count => 0x%x", ret);
        return false;
    }

    zes_mem_handle_t *mems = calloc(mem_count, sizeof(*mems));
    if (mems == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return false;
    }

    ret = zesDeviceEnumMemoryModules(dev, &mem_count, mems);
    if (ret != ZE_RESULT_SUCCESS) {
        PLUGIN_ERROR("failed to get %u memory modules => 0x%x", mem_count, ret);
        free(mems);
        return false;
    }

    if ((gpu->memory_count != mem_count) || (gpu->memory == NULL)) {
        PLUGIN_INFO(" Sysman reports %u memory modules", mem_count);
        gpu->memory = (zes_mem_state_t **)gpu_subarray_realloc(
                                   (void **)gpu->memory, mem_count, sizeof(gpu->memory[0][0]));
        if (gpu->memory == NULL) {
            free(mems);
            return false;
        }
        gpu->memory_count = mem_count;
        assert(gpu->memory);
    }

    metric_family_t fam_bytes = {
            .help = "Sampled memory usage (in bytes)",
            .name = "gpu_intel_memory_used_bytes",
            .type = METRIC_TYPE_GAUGE,
    };
    metric_family_t fam_ratio = {
            .help = "Sampled memory usage ratio (0-1)",
            .name = "gpu_intel_memory_usage_ratio",
            .type = METRIC_TYPE_GAUGE,
    };
    metric_t metric = {0};

    bool reported_ratio = false, reported_base = false, ok = false;
    for (uint32_t i = 0; i < mem_count; i++) {
        /* fetch memory samples */
        ret = zesMemoryGetState(mems[i], &(gpu->memory[cache_idx][i]));
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("failed to get memory module %u state => 0x%x", i, ret);
            ok = false;
            break;
        }
        ok = true;
        if (cache_idx > 0) {
            continue;
        }
        const uint64_t mem_size = gpu->memory[0][i].size;
        if (!mem_size) {
            PLUGIN_ERROR("invalid (zero) memory module %u size", i);
            ok = false;
            break;
        }
        /* process samples */
        ret = set_mem_labels(mems[i], &metric);
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("failed to get memory module %u properties => 0x%x", i, ret);
            ok = false;
            break;
        }
        /* get health status from last i.e. zeroeth sample */
        zes_mem_health_t value = gpu->memory[0][i].health;
        if (value != ZES_MEM_HEALTH_UNKNOWN) {
            const char *health;
            switch (value) {
            case ZES_MEM_HEALTH_OK:
                health = "ok";
                break;
            case ZES_MEM_HEALTH_DEGRADED:
                health = "degraded";
                break;
            case ZES_MEM_HEALTH_CRITICAL:
                health = "critical";
                break;
            case ZES_MEM_HEALTH_REPLACE:
                health = "replace";
                break;
            default:
                health = "unknown";
            }
            metric_label_set(&metric, "health", health);
        }
        double mem_used;
        if (config.samples < 2) {
            const uint64_t mem_free = gpu->memory[0][i].free;
            /* Sysman reports just memory size & free amounts => calculate used */
            mem_used = mem_size - mem_free;
            if (config.output & OUTPUT_BASE) {
                metric.value = VALUE_GAUGE(mem_used);
                metric_family_metric_append(&fam_bytes, metric);
                reported_base = true;
            }
            if (config.output & OUTPUT_RATIO) {
                metric.value = VALUE_GAUGE(mem_used / mem_size);
                metric_family_metric_append(&fam_ratio, metric);
                reported_ratio = true;
            }
        } else {
            /* find min & max values for memory free from
             * (the configured number of) samples
             */
            uint64_t free_min = (uint64_t)0xffffffff;
            uint64_t free_max = 0, mem_free;
            for (uint32_t j = 0; j < config.samples; j++) {
                mem_free = gpu->memory[j][i].free;
                if (mem_free < free_min) {
                    free_min = mem_free;
                }
                if (mem_free > free_max) {
                    free_max = mem_free;
                }
            }
            /* smallest used amount of memory within interval */
            mem_used = mem_size - free_max;
            metric_label_set(&metric, "function", "min");
            if (config.output & OUTPUT_BASE) {
                metric.value = VALUE_GAUGE(mem_used);
                metric_family_metric_append(&fam_bytes, metric);
                reported_base = true;
            }
            if (config.output & OUTPUT_RATIO) {
                metric.value = VALUE_GAUGE(mem_used / mem_size);
                metric_family_metric_append(&fam_ratio, metric);
                reported_ratio = true;
            }
            /* largest used amount of memory within interval */
            mem_used = mem_size - free_min;
            metric_label_set(&metric, "function", "max");
            if (config.output & OUTPUT_BASE) {
                metric.value = VALUE_GAUGE(mem_used);
                metric_family_metric_append(&fam_bytes, metric);
                reported_base = true;
            }
            if (config.output & OUTPUT_RATIO) {
                metric.value = VALUE_GAUGE(mem_used / mem_size);
                metric_family_metric_append(&fam_ratio, metric);
                reported_ratio = true;
            }
        }
        metric_reset(&metric, METRIC_TYPE_GAUGE);
    }
    if (reported_base) {
        gpu_submit(gpu, &fam_bytes);
    }
    if (reported_ratio) {
        gpu_submit(gpu, &fam_ratio);
    }
    free(mems);
    return ok;
}

static void add_bw_gauges(metric_t *metric, metric_family_t *fam, double reads, double writes)
{
    metric->value = VALUE_GAUGE(reads);
    metric_label_set(metric, "direction", "read");
    metric_family_metric_append(fam, *metric);

    metric->value = VALUE_GAUGE(writes);
    metric_label_set(metric, "direction", "write");
    metric_family_metric_append(fam, *metric);
}

/* Report memory modules bandwidth usage, return true for success.  */
static bool gpu_mems_bw(gpu_device_t *gpu)
{
    uint32_t mem_count = 0;
    zes_device_handle_t dev = gpu->handle;
    ze_result_t ret = zesDeviceEnumMemoryModules(dev, &mem_count, NULL);
    if (ret != ZE_RESULT_SUCCESS) {
        PLUGIN_ERROR("failed to get memory (BW) modules count => 0x%x", ret);
        return false;
    }
    zes_mem_handle_t *mems = calloc(mem_count, sizeof(*mems));
    if (mems == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return false;
    }

    ret = zesDeviceEnumMemoryModules(dev, &mem_count, mems);
    if (ret != ZE_RESULT_SUCCESS) {
        PLUGIN_ERROR("failed to get %u memory (BW) modules => 0x%x", mem_count, ret);
        free(mems);
        return false;
    }

    if (gpu->membw_count != mem_count) {
        PLUGIN_INFO("Sysman reports %u memory (BW) modules", mem_count);
        if (gpu->membw) {
            free(gpu->membw);
        }
        gpu->membw = calloc(mem_count, sizeof(*gpu->membw));
        if (gpu->membw == NULL) {
            PLUGIN_ERROR("calloc failed.");
            free(mems);
            return false;
        }
        gpu->membw_count = mem_count;
    }

    metric_family_t fam_ratio = {
            .help = "Average memory bandwidth usage ratio (0-1) over query interval",
            .name = "gpu_intel_memory_bw_ratio",
            .type = METRIC_TYPE_GAUGE,
    };
    metric_family_t fam_rate = {
            .help = "Memory bandwidth usage rate (in bytes per second)",
            .name = "gpu_intel_memory_bw_bytes_per_second",
            .type = METRIC_TYPE_GAUGE,
    };
    metric_family_t fam_counter = {
            .help = "Memory bandwidth usage total (in bytes)",
            .name = "gpu_intel_memory_bw_bytes_total",
            .type = METRIC_TYPE_COUNTER,
    };
    metric_t metric = {0};

    bool reported_rate = false, reported_ratio = false, reported_base = false;

    bool ok = false;
    for (uint32_t i = 0; i < mem_count; i++) {
        zes_mem_bandwidth_t bw;
        ret = zesMemoryGetBandwidth(mems[i], &bw);
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("failed to get memory module %u bandwidth => 0x%x", i, ret);
            ok = false;
            break;
        }
        ret = set_mem_labels(mems[i], &metric);
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("failed to get memory module %u properties => 0x%x", i, ret);
            ok = false;
            break;
        }
        if (config.output & OUTPUT_BASE) {
            metric.value = VALUE_COUNTER(bw.writeCounter);
            metric_label_set(&metric, "direction", "write");
            metric_family_metric_append(&fam_counter, metric);

            metric.value = VALUE_COUNTER(bw.readCounter);
            metric_label_set(&metric, "direction", "read");
            metric_family_metric_append(&fam_counter, metric);
            reported_base = true;
        }
        zes_mem_bandwidth_t *old = &gpu->membw[i];
        if (old->timestamp && bw.timestamp > old->timestamp &&
                (config.output & (OUTPUT_RATIO | OUTPUT_RATE))) {
            /* https://spec.oneapi.com/level-zero/latest/sysman/api.html#_CPPv419zes_mem_bandwidth_t
             */
            uint64_t writes = bw.writeCounter - old->writeCounter;
            uint64_t reads = bw.readCounter - old->readCounter;
            uint64_t timediff = bw.timestamp - old->timestamp;

            if (config.output & OUTPUT_RATE) {
                double factor = 1.0e6 / timediff;
                add_bw_gauges(&metric, &fam_rate, factor * reads, factor * writes);
                reported_rate = true;
            }
            if ((config.output & OUTPUT_RATIO) && old->maxBandwidth) {
                double factor = 1.0e6 / (old->maxBandwidth * timediff);
                add_bw_gauges(&metric, &fam_ratio, factor * reads, factor * writes);
                reported_ratio = true;
            }
        }
        metric_reset(&metric, METRIC_TYPE_COUNTER);
        *old = bw;
        ok = true;
    }

    if (reported_ratio)
        gpu_submit(gpu, &fam_ratio);
    if (reported_rate)
        gpu_submit(gpu, &fam_rate);
    if (reported_base)
        gpu_submit(gpu, &fam_counter);
    free(mems);
    return ok;
}

/* set frequency metric labels based on its properties and maxfreq for non-NULL
 * pointer, return ZE_RESULT_SUCCESS for success
 */
static ze_result_t set_freq_labels(zes_freq_handle_t freq, metric_t *metric, double *maxfreq)
{
    zes_freq_properties_t props = {.pNext = NULL};
    ze_result_t ret = zesFrequencyGetProperties(freq, &props);
    if (ret != ZE_RESULT_SUCCESS)
        return ret;

    if (maxfreq)
        *maxfreq = props.max;

    const char *type;
    switch (props.type) {
    case ZES_FREQ_DOMAIN_GPU:
        type = "gpu";
        break;
    case ZES_FREQ_DOMAIN_MEMORY:
        type = "memory";
        break;
    default:
        type = "unknown";
    }

    metric_label_set(metric, "location", type);
    metric_set_subdev(metric, props.onSubdevice, props.subdeviceId);
    return ZE_RESULT_SUCCESS;
}

/* set label explaining frequency throttling reason(s) */
static void set_freq_throttled_label(metric_t *metric, zes_freq_throttle_reason_flags_t reasons)
{
    static const struct {
        zes_freq_throttle_reason_flags_t flag;
        const char *reason;
    } flags[] = {
            {ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP, "average-power"},
            {ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP, "burst-power"},
            {ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT, "current"},
            {ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT, "temperature"},
            {ZES_FREQ_THROTTLE_REASON_FLAG_PSU_ALERT, "PSU-alert"},
            {ZES_FREQ_THROTTLE_REASON_FLAG_SW_RANGE, "SW-freq-range"},
            {ZES_FREQ_THROTTLE_REASON_FLAG_HW_RANGE, "HW-freq-range"},
    };
    bool found = false;
    const char *reason = NULL;

    for (unsigned int i = 0; i < STATIC_ARRAY_SIZE(flags); i++) {
        if (reasons & flags[i].flag) {
            if (found) {
                reason = "many";
                break;
            }
            reason = flags[i].reason;
            found = true;
        }
    }

    if (reasons) {
        if (!found) {
            reason = "unknown";
        }
        metric_label_set(metric, "throttled_by", reason);
    }
}

/* Report frequency domains request & actual frequency, return true for success
 *
 * See gpu_read() on 'cache_idx' usage.
 */
static bool gpu_freqs(gpu_device_t *gpu, unsigned int cache_idx)
{
    if (!(config.output & (OUTPUT_BASE | OUTPUT_RATIO))) {
        PLUGIN_ERROR("no frequency output variants selected");
        return false;
    }

    uint32_t freq_count = 0;
    zes_device_handle_t dev = gpu->handle;
    ze_result_t ret = zesDeviceEnumFrequencyDomains(dev, &freq_count, NULL);
    if (ret != ZE_RESULT_SUCCESS) {
        PLUGIN_ERROR("failed to get frequency domains count => 0x%x", ret);
        return false;
    }

    zes_freq_handle_t *freqs = calloc(freq_count, sizeof(*freqs));
    if (freqs == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return false;
    }

    ret = zesDeviceEnumFrequencyDomains(dev, &freq_count, freqs);
    if (ret != ZE_RESULT_SUCCESS) {
        PLUGIN_ERROR("failed to get %u frequency domains => 0x%x", freq_count, ret);
        free(freqs);
        return false;
    }

    if (gpu->frequency_count != freq_count) {
        PLUGIN_INFO(" Sysman reports %u frequency domains", freq_count);
        gpu->frequency = (zes_freq_state_t **)gpu_subarray_realloc(
                         (void **)gpu->frequency, freq_count, sizeof(gpu->frequency[0][0]));
        gpu->frequency_count = freq_count;
        assert(gpu->frequency);
    }

    metric_family_t fam_freq = {
            .help = "Sampled HW frequency (in MHz)",
            .name = "gpu_intel_frequency_mhz",
            .type = METRIC_TYPE_GAUGE,
    };
    metric_family_t fam_ratio = {
            .help = "Sampled HW frequency ratio vs (non-overclocked) max frequency",
            .name = "gpu_intel_frequency_ratio",
            .type = METRIC_TYPE_GAUGE,
    };
    metric_t metric = {0};

    bool reported_ratio = false, reported_base = false, ok = false;
    for (uint32_t i = 0; i < freq_count; i++) {
        /* fetch freq samples */
        ret = zesFrequencyGetState(freqs[i], &(gpu->frequency[cache_idx][i]));
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("Failed to get frequency domain %u state => 0x%x", i, ret);
            ok = false;
            break;
        }
        ok = true;
        if (cache_idx > 0) {
            continue;
        }
        /* process samples */
        double maxfreq;
        ret = set_freq_labels(freqs[i], &metric, &maxfreq);
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("Failed to get frequency domain %u properties => 0x%x", i, ret);
            ok = false;
            break;
        }
        double value;

        if (config.samples < 2) {
            set_freq_throttled_label(&metric, gpu->frequency[0][i].throttleReasons);
            /* negative value = unsupported:
             * https://spec.oneapi.com/level-zero/latest/sysman/api.html#_CPPv416zes_freq_state_t
             */
            value = gpu->frequency[0][i].request;
            if (value >= 0) {
                metric_label_set(&metric, "type", "request");
                if (config.output & OUTPUT_BASE) {
                    metric.value = VALUE_GAUGE(value);
                    metric_family_metric_append(&fam_freq, metric);
                    reported_base = true;
                }
                if ((config.output & OUTPUT_RATIO) && maxfreq > 0) {
                    metric.value = VALUE_GAUGE(value / maxfreq);
                    metric_family_metric_append(&fam_ratio, metric);
                    reported_ratio = true;
                }
            }
            value = gpu->frequency[0][i].actual;
            if (value >= 0) {
                metric_label_set(&metric, "type", "actual");
                if (config.output & OUTPUT_BASE) {
                    metric.value = VALUE_GAUGE(value);
                    metric_family_metric_append(&fam_freq, metric);
                    reported_base = true;
                }
                if ((config.output & OUTPUT_RATIO) && maxfreq > 0) {
                    metric.value = VALUE_GAUGE(value / maxfreq);
                    metric_family_metric_append(&fam_ratio, metric);
                    reported_ratio = true;
                }
            }
        } else {
            /* find min & max values for actual frequency & its request
             * from (the configured number of) samples
             */
            double req_min = 1.0e12, req_max = -1.0e12;
            double act_min = 1.0e12, act_max = -1.0e12;
            zes_freq_throttle_reason_flags_t reasons = 0;
            for (uint32_t j = 0; j < config.samples; j++) {
                reasons |= gpu->frequency[j][i].throttleReasons;
                value = gpu->frequency[j][i].request;
                if (value < req_min) {
                    req_min = value;
                }
                if (value > req_max) {
                    req_max = value;
                }
                value = gpu->frequency[j][i].actual;
                if (value < act_min) {
                    act_min = value;
                }
                if (value > act_max) {
                    act_max = value;
                }
            }
            set_freq_throttled_label(&metric, reasons);
            if (req_max >= 0.0) {
                metric_label_set(&metric, "type", "request");
                metric_label_set(&metric, "function", "min");
                if (config.output & OUTPUT_BASE) {
                    metric.value = VALUE_GAUGE(req_min);
                    metric_family_metric_append(&fam_freq, metric);
                    reported_base = true;
                }
                if ((config.output & OUTPUT_RATIO) && maxfreq > 0) {
                    metric.value = VALUE_GAUGE(req_min / maxfreq);
                    metric_family_metric_append(&fam_ratio, metric);
                    reported_ratio = true;
                }
                metric_label_set(&metric, "function", "max");
                if (config.output & OUTPUT_BASE) {
                    metric.value = VALUE_GAUGE(req_max);
                    metric_family_metric_append(&fam_freq, metric);
                    reported_base = true;
                }
                if ((config.output & OUTPUT_RATIO) && maxfreq > 0) {
                    metric.value = VALUE_GAUGE(req_max / maxfreq);
                    metric_family_metric_append(&fam_ratio, metric);
                    reported_ratio = true;
                }
            }
            if (act_max >= 0.0) {
                metric_label_set(&metric, "type", "actual");
                metric_label_set(&metric, "function", "min");
                if (config.output & OUTPUT_BASE) {
                    metric.value = VALUE_GAUGE(act_min);
                    metric_family_metric_append(&fam_freq, metric);
                    reported_base = true;
                }
                if ((config.output & OUTPUT_RATIO) && maxfreq > 0) {
                    metric.value = VALUE_GAUGE(act_min / maxfreq);
                    metric_family_metric_append(&fam_ratio, metric);
                    reported_ratio = true;
                }
                metric_label_set(&metric, "function", "max");
                if (config.output & OUTPUT_BASE) {
                    metric.value = VALUE_GAUGE(act_max);
                    metric_family_metric_append(&fam_freq, metric);
                    reported_base = true;
                }
                if ((config.output & OUTPUT_RATIO) && maxfreq > 0) {
                    metric.value = VALUE_GAUGE(act_max / maxfreq);
                    metric_family_metric_append(&fam_ratio, metric);
                    reported_ratio = true;
                }
            }
        }
        metric_reset(&metric, METRIC_TYPE_GAUGE);
        if (!(reported_base || reported_ratio)) {
            PLUGIN_ERROR("neither requests nor actual frequencies supported for domain %u", i);
            ok = false;
            break;
        }
    }

    if (reported_base)
        gpu_submit(gpu, &fam_freq);
    if (reported_ratio)
        gpu_submit(gpu, &fam_ratio);
    free(freqs);
    return ok;
}

/* Report throttling time, return true for success */
static bool gpu_freqs_throttle(gpu_device_t *gpu)
{
    if (!(config.output & (OUTPUT_BASE | OUTPUT_RATIO))) {
        PLUGIN_ERROR("no throttle-time output variants selected");
        return false;
    }

    uint32_t freq_count = 0;
    zes_device_handle_t dev = gpu->handle;
    ze_result_t ret = zesDeviceEnumFrequencyDomains(dev, &freq_count, NULL);
    if (ret != ZE_RESULT_SUCCESS) {
        PLUGIN_ERROR("failed to get frequency (throttling) domains count => 0x%x", ret);
        return false;
    }

    zes_freq_handle_t *freqs = calloc(freq_count, sizeof(*freqs));
    if (freqs == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return false;
    }

    ret = zesDeviceEnumFrequencyDomains(dev, &freq_count, freqs);
    if (ret != ZE_RESULT_SUCCESS) {
        PLUGIN_ERROR("Failed to get %u frequency (throttling) domains => 0x%x", freq_count, ret);
        free(freqs);
        return false;
    }

    if ((gpu->throttle_count != freq_count) || (gpu->throttle == NULL)) {
        PLUGIN_INFO("Sysman reports %u frequency (throttling) domains", freq_count);
        if (gpu->throttle != NULL)
            free(gpu->throttle);
        gpu->throttle = calloc(freq_count, sizeof(*gpu->throttle));
        if (gpu->throttle == NULL) {
            PLUGIN_ERROR("calloc failed.");
            free(freqs);
            return false;
        }
        gpu->throttle_count = freq_count;
    }

    metric_family_t fam_ratio = {
            .name = "gpu_intel_throttled_ratio",
            .type = METRIC_TYPE_GAUGE,
            .help = "Ratio (0-1) of HW frequency being throttled during query interval",
    };
    metric_family_t fam_counter = {
            .name = "gpu_intel_throttled_usecs_total",
            .type = METRIC_TYPE_COUNTER,
            .help = "Total time HW frequency has been throttled (in microseconds)",
    };
    metric_t metric = {0};

    bool reported_ratio = false, reported_base = false, ok = false;
    for (uint32_t i = 0; i < freq_count; i++) {
        zes_freq_throttle_time_t throttle;
        ret = zesFrequencyGetThrottleTime(freqs[i], &throttle);
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("failed to get frequency domain %u throttle time => 0x%x", i, ret);
            ok = false;
            break;
        }
        ret = set_freq_labels(freqs[i], &metric, NULL);
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("failed to get frequency domain %u properties => 0x%x", i, ret);
            ok = false;
            break;
        }
        if (config.output & OUTPUT_BASE) {
            /* cannot convert microsecs to secs as counters are integers */
            metric.value = VALUE_COUNTER(throttle.throttleTime);
            metric_family_metric_append(&fam_counter, metric);
            reported_base = true;
        }
        zes_freq_throttle_time_t *old = &gpu->throttle[i];
        if (old->timestamp && throttle.timestamp > old->timestamp &&
            (config.output & OUTPUT_RATIO)) {
            /* micro seconds => throttle ratio */
            metric.value = VALUE_GAUGE((throttle.throttleTime - old->throttleTime) /
                                       (double)(throttle.timestamp - old->timestamp));
            metric_family_metric_append(&fam_ratio, metric);
            reported_ratio = true;
        }
        metric_reset(&metric, METRIC_TYPE_GAUGE);
        *old = throttle;
        ok = true;
    }

    if (reported_ratio)
        gpu_submit(gpu, &fam_ratio);
    if (reported_base)
        gpu_submit(gpu, &fam_counter);
    free(freqs);
    return ok;
}

/* Report relevant temperature sensor values, return true for success */
static bool gpu_temps(gpu_device_t *gpu)
{
    if (!(config.output & (OUTPUT_BASE | OUTPUT_RATIO))) {
        PLUGIN_ERROR("no temperature output variants selected");
        return false;
    }

    uint32_t temp_count = 0;
    zes_device_handle_t dev = gpu->handle;
    ze_result_t ret = zesDeviceEnumTemperatureSensors(dev, &temp_count, NULL);
    if (ret != ZE_RESULT_SUCCESS) {
        PLUGIN_ERROR("failed to get temperature sensors count => 0x%x", ret);
        return false;
    }

    zes_temp_handle_t *temps = calloc(temp_count, sizeof(*temps));
    if (temps == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return false;
    }

    ret = zesDeviceEnumTemperatureSensors(dev, &temp_count, temps);
    if (ret != ZE_RESULT_SUCCESS) {
        PLUGIN_ERROR("failed to get %u temperature sensors => 0x%x", temp_count, ret);
        free(temps);
        return false;
    }
    if (gpu->temp_count != temp_count) {
        PLUGIN_INFO(" Sysman reports %u temperature sensors", temp_count);
        gpu->temp_count = temp_count;
    }

    metric_family_t fam_temp = {
            .help = "Temperature sensor value (in Celsius) when queried",
            .name = "gpu_intel_temperature_celsius",
            .type = METRIC_TYPE_GAUGE,
    };
    metric_family_t fam_ratio = {
            .help = "Temperature sensor value ratio to its max value when queried",
            .name = "gpu_intel_temperature_ratio",
            .type = METRIC_TYPE_GAUGE,
    };
    metric_t metric = {0};

    bool reported_ratio = false, reported_base = false, ok = false;
    for (uint32_t i = 0; i < temp_count; i++) {
        zes_temp_properties_t props = {.pNext = NULL};
        ret = zesTemperatureGetProperties(temps[i], &props);
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("failed to get temperature sensor %u properties => 0x%x", i, ret);
            ok = false;
            break;
        }
        const char *type;
        /* https://spec.oneapi.io/level-zero/latest/sysman/PROG.html#querying-temperature */
        switch (props.type) {
        /* max temperatures */
        case ZES_TEMP_SENSORS_GLOBAL:
            type = "global-max";
            break;
        case ZES_TEMP_SENSORS_GPU:
            type = "gpu-max";
            break;
        case ZES_TEMP_SENSORS_MEMORY:
            type = "memory-max";
            break;
        /* min temperatures */
        case ZES_TEMP_SENSORS_GLOBAL_MIN:
            type = "global-min";
            break;
        case ZES_TEMP_SENSORS_GPU_MIN:
            type = "gpu-min";
            break;
        case ZES_TEMP_SENSORS_MEMORY_MIN:
            type = "memory-min";
            break;
        default:
            type = "unknown";
        }

        double value;
        ret = zesTemperatureGetState(temps[i], &value);
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("failed to get temperature sensor %u (%s) state => 0x%x", i, type, ret);
            ok = false;
            break;
        }
        metric_label_set(&metric, "location", type);
        metric_set_subdev(&metric, props.onSubdevice, props.subdeviceId);
        if (config.output & OUTPUT_BASE) {
            metric.value = VALUE_GAUGE(value);
            metric_family_metric_append(&fam_temp, metric);
            reported_base = true;
        }
        if (props.maxTemperature > 0 && (config.output & OUTPUT_RATIO)) {
            metric.value = VALUE_GAUGE(value / props.maxTemperature);
            metric_family_metric_append(&fam_ratio, metric);
            reported_ratio = true;
        }
        metric_reset(&metric, METRIC_TYPE_GAUGE);
        ok = true;
    }

    if (reported_base)
        gpu_submit(gpu, &fam_temp);
    if (reported_ratio)
        gpu_submit(gpu, &fam_ratio);
    free(temps);
    return ok;
}

/* status / health labels */
static void add_fabric_state_labels(metric_t *metric, zes_fabric_port_state_t *state)
{
    const char *status;
    switch (state->status) {
    case ZES_FABRIC_PORT_STATUS_UNKNOWN:
        status = "unknown";
        break;
    case ZES_FABRIC_PORT_STATUS_HEALTHY:
        status = "healthy";
        break;
    case ZES_FABRIC_PORT_STATUS_DEGRADED:
        status = "degraded";
        break;
    case ZES_FABRIC_PORT_STATUS_FAILED:
        status = "failed";
        break;
    case ZES_FABRIC_PORT_STATUS_DISABLED:
        status = "disabled";
        break;
    default:
        status = "unsupported";
    }
    metric_label_set(metric, "status", status);

    const char *issues = NULL;
    switch (state->qualityIssues) {
    case 0:
        break;
    case ZES_FABRIC_PORT_QUAL_ISSUE_FLAG_LINK_ERRORS:
        issues = "link";
        break;
    case ZES_FABRIC_PORT_QUAL_ISSUE_FLAG_SPEED:
        issues = "speed";
        break;
    default:
        issues = "link+speed";
    }

    switch (state->failureReasons) {
    case 0:
        break;
    case ZES_FABRIC_PORT_FAILURE_FLAG_FAILED:
        issues = "failure";
        break;
    case ZES_FABRIC_PORT_FAILURE_FLAG_TRAINING_TIMEOUT:
        issues = "training";
        break;
    case ZES_FABRIC_PORT_FAILURE_FLAG_FLAPPING:
        issues = "flapping";
        break;
    default:
        issues = "multiple";
    }

    if (issues)
        metric_label_set(metric, "issues", issues);
}

/* Report metrics for relevant fabric ports, return true for success */
static bool gpu_fabrics(gpu_device_t *gpu)
{
    uint32_t port_count = 0;
    zes_device_handle_t dev = gpu->handle;
    ze_result_t ret = zesDeviceEnumFabricPorts(dev, &port_count, NULL);
    if (ret != ZE_RESULT_SUCCESS) {
        PLUGIN_ERROR("failed to get fabric port count => 0x%x", ret);
        return false;
    }

    zes_fabric_port_handle_t *ports = calloc(port_count, sizeof(*ports));
    if (ports == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return false;
    }

    ret = zesDeviceEnumFabricPorts(dev, &port_count, ports);
    if (ret != ZE_RESULT_SUCCESS) {
        PLUGIN_ERROR("failed to get %u fabric ports => 0x%x", port_count, ret);
        free(ports);
        return false;
    }

    if (gpu->fabric_count != port_count) {
        PLUGIN_INFO("Sysman reports %u fabric ports", port_count);
        if (gpu->fabric) {
            free(gpu->fabric);
        }
        gpu->fabric = calloc(port_count, sizeof(*gpu->fabric));
        if (gpu->fabric == NULL) {
            PLUGIN_ERROR("calloc failed.");
            free(ports);
            return false;
        }
        gpu->fabric_count = port_count;
    }

    metric_family_t fam_ratio = {
            .help =
                    "Average fabric port bandwidth usage ratio (0-1) over query interval",
            .name = "gpu_intel_fabric_port_ratio",
            .type = METRIC_TYPE_GAUGE,
    };
    metric_family_t fam_rate = {
            .help = "Fabric port throughput rate (in bytes per second)",
            .name = "gpu_intel_fabric_port_bytes_per_second",
            .type = METRIC_TYPE_GAUGE,
    };
    metric_family_t fam_counter = {
            .help = "Fabric port throughput total (in bytes)",
            .name = "gpu_intel_fabric_port_bytes_total",
            .type = METRIC_TYPE_COUNTER,
    };
    metric_t metric = {0};

    bool reported_rate = false, reported_ratio = false, reported_base = false;

    bool ok = false;
    for (uint32_t i = 0; i < port_count; i++) {

        /* fetch all information before allocing labels */

        zes_fabric_port_state_t state = {.pNext = NULL};
        ret = zesFabricPortGetState(ports[i], &state);
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("failed to get fabric port %u state => 0x%x", i, ret);
            ok = false;
            break;
        }
        zes_fabric_port_properties_t props = {.pNext = NULL};
        ret = zesFabricPortGetProperties(ports[i], &props);
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("failed to get fabric port %u properties => 0x%x", i, ret);
            ok = false;
            break;
        }
        zes_fabric_port_config_t conf = {.pNext = NULL};
        ret = zesFabricPortGetConfig(ports[i], &conf);
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("failed to get fabric port %u config => 0x%x", i, ret);
            ok = false;
            break;
        }
        zes_fabric_port_throughput_t bw;
        ret = zesFabricPortGetThroughput(ports[i], &bw);
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("failed to get fabric port %u throughput => 0x%x", i, ret);
            ok = false;
            break;
        }
        zes_fabric_link_type_t link;
        ret = zesFabricPortGetLinkType(ports[i], &link);
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("failed to get fabric port %u link type => 0x%x", i, ret);
            ok = false;
            break;
        }

        /* port setting / identity setting labels */

        link.desc[sizeof(link.desc) - 1] = '\0';
        metric_label_set(&metric, "link", link.desc);
        metric_label_set(&metric, "enabled", conf.enabled ? "on" : "off");
        metric_label_set(&metric, "beaconing", conf.beaconing ? "on" : "off");

        props.model[sizeof(props.model) - 1] = '\0';
        metric_label_set(&metric, "model", props.model);
        metric_set_subdev(&metric, props.onSubdevice, props.subdeviceId);

        /* topology labels */

        char buf[32];
        zes_fabric_port_id_t *pid = &props.portId;
        snprintf(buf, sizeof(buf), "%08x.%08x.%02x", pid->fabricId, pid->attachId,
                         pid->portNumber);
        metric_label_set(&metric, "port", buf);

        pid = &state.remotePortId;
        snprintf(buf, sizeof(buf), "%08x.%08x.%02x", pid->fabricId, pid->attachId,
                         pid->portNumber);
        metric_label_set(&metric, "remote", buf);

        /* status / health labels */

        add_fabric_state_labels(&metric, &state);

        /* add counters with direction labels */

        if (config.output & OUTPUT_BASE) {
            metric.value = VALUE_COUNTER(bw.txCounter);
            metric_label_set(&metric, "direction", "write");
            metric_family_metric_append(&fam_counter, metric);

            metric.value = VALUE_COUNTER(bw.rxCounter);
            metric_label_set(&metric, "direction", "read");
            metric_family_metric_append(&fam_counter, metric);
            reported_base = true;
        }

        /* add rate + ratio gauges with direction labels */

        zes_fabric_port_throughput_t *old = &gpu->fabric[i];
        if (old->timestamp && bw.timestamp > old->timestamp &&
                (config.output & (OUTPUT_RATIO | OUTPUT_RATE))) {
            /* https://spec.oneapi.io/level-zero/latest/sysman/api.html#zes-fabric-port-throughput-t
             */
            uint64_t writes = bw.txCounter - old->txCounter;
            uint64_t reads = bw.rxCounter - old->rxCounter;
            uint64_t timediff = bw.timestamp - old->timestamp;

            if (config.output & OUTPUT_RATE) {
                double factor = 1.0e6 / timediff;
                add_bw_gauges(&metric, &fam_rate, factor * reads, factor * writes);
                reported_rate = true;
            }
            if (config.output & OUTPUT_RATIO) {
                int64_t maxr = props.maxRxSpeed.bitRate * props.maxRxSpeed.width / 8;
                int64_t maxw = props.maxTxSpeed.bitRate * props.maxTxSpeed.width / 8;
                if (maxr > 0 && maxw > 0) {
                    double rfactor = 1.0e6 / (maxr * timediff);
                    double wfactor = 1.0e6 / (maxw * timediff);
                    add_bw_gauges(&metric, &fam_ratio, rfactor * reads, wfactor * writes);
                    reported_ratio = true;
                }
            }
        }
        metric_reset(&metric, METRIC_TYPE_COUNTER);
        *old = bw;
        ok = true;
    }

    if (reported_ratio)
        gpu_submit(gpu, &fam_ratio);
    if (reported_rate)
        gpu_submit(gpu, &fam_rate);
    if (reported_base)
        gpu_submit(gpu, &fam_counter);
    free(ports);
    return ok;
}

/* Report power usage for relevant domains, return true for success */
static bool gpu_powers(gpu_device_t *gpu)
{
    uint32_t power_count = 0;
    zes_device_handle_t dev = gpu->handle;
    ze_result_t ret = zesDeviceEnumPowerDomains(dev, &power_count, NULL);
    if (ret != ZE_RESULT_SUCCESS) {
        PLUGIN_ERROR("failed to get power domains count => 0x%x", ret);
        return false;
    }

    zes_pwr_handle_t *powers = calloc(power_count, sizeof(*powers));
    if (powers == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return false;
    }

    ret = zesDeviceEnumPowerDomains(dev, &power_count, powers);
    if (ret != ZE_RESULT_SUCCESS) {
        PLUGIN_ERROR("failed to get %u power domains => 0x%x", power_count, ret);
        free(powers);
        return false;
    }

    if (gpu->power_count != power_count) {
        PLUGIN_INFO(" Sysman reports %u power domains", power_count);
        if (gpu->power) {
            free(gpu->power);
        }
        gpu->power = calloc(power_count, sizeof(*gpu->power));
        if (gpu->power == NULL) {
            free(powers);
            PLUGIN_ERROR("calloc failed.");
            return false;
        }
        gpu->power_count = power_count;
    }

    metric_family_t fam_ratio = {
            .name = "gpu_intel_power_ratio",
            .type = METRIC_TYPE_GAUGE,
            .help = "Ratio of average power usage vs sustained or burst power limit",
    };
    metric_family_t fam_power = {
            .name = "gpu_intel_power_watts",
            .type = METRIC_TYPE_GAUGE,
            .help = "Average power usage (in Watts) over query interval",
    };
    metric_family_t fam_energy = {
            .name = "gpu_intel_energy_ujoules_total",
            .type = METRIC_TYPE_COUNTER,
            .help = "Total energy consumption since boot (in microjoules)",
    };
    metric_t metric = {0};

    ze_result_t limit_ret = ZE_RESULT_SUCCESS;
    bool reported_ratio = false, reported_rate = false, reported_base = false;
    bool ratio_fail = false;
    bool ok = false;

    for (uint32_t i = 0; i < power_count; i++) {
        zes_power_properties_t props = {.pNext = NULL};
        ret = zesPowerGetProperties(powers[i], &props);
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("failed to get power domain %u properties => 0x%x", i, ret);
            ok = false;
            break;
        }
        zes_power_energy_counter_t counter;
        ret = zesPowerGetEnergyCounter(powers[i], &counter);
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("failed to get power domain %u energy counter => 0x%x", i, ret);
            ok = false;
            break;
        }
        metric_set_subdev(&metric, props.onSubdevice, props.subdeviceId);
        if (config.output & OUTPUT_BASE) {
            metric.value = VALUE_COUNTER(counter.energy);
            metric_family_metric_append(&fam_energy, metric);
            reported_base = true;
        }
        zes_power_energy_counter_t *old = &gpu->power[i];
        if (old->timestamp && counter.timestamp > old->timestamp &&
                (config.output & (OUTPUT_RATIO | OUTPUT_RATE))) {

            uint64_t energy_diff = counter.energy - old->energy;
            double time_diff = counter.timestamp - old->timestamp;

            if (config.output & OUTPUT_RATE) {
                /* microJoules / microSeconds => watts */
                metric.value = VALUE_GAUGE(energy_diff / time_diff);
                metric_family_metric_append(&fam_power, metric);
                reported_rate = true;
            }
            if ((config.output & OUTPUT_RATIO) && (gpu->flags & COLLECT_POWER)) {
                zes_power_burst_limit_t burst;
                zes_power_sustained_limit_t sustain;
                /* TODO: future spec version deprecates zesPowerGetLimits():
                 *              https://github.com/oneapi-src/level-zero-spec/issues/12
                 * Switch to querying list of limits after Sysman plugin starts
                 * requiring that spec version / loader.
                 */
                limit_ret = zesPowerGetLimits(powers[i], &sustain, &burst, NULL);
                if (limit_ret == ZE_RESULT_SUCCESS) {
                    const char *name;
                    int32_t limit = 0;
                    /* Multiply by 1000, as sustain interval is in ms & power in mJ/s,
                     * whereas energy is in uJ and its timestamp in us:
                     * https://spec.oneapi.io/level-zero/latest/sysman/api.html#zes-power-energy-counter-t
                     */
                    if (sustain.enabled &&
                        (time_diff >= 1000 * sustain.interval || !burst.enabled)) {
                        name = "sustained";
                        limit = sustain.power;
                    } else if (burst.enabled) {
                        name = "burst";
                        limit = burst.power;
                    }
                    if (limit > 0) {
                        metric_label_set(&metric, "limit", name);
                        metric.value = VALUE_GAUGE(1000 * energy_diff / (limit * time_diff));
                        metric_family_metric_append(&fam_ratio, metric);
                        reported_ratio = true;
                    } else {
                        ratio_fail = true;
                    }
                } else {
                    ratio_fail = true;
                }
            }
        }
        metric_reset(&metric, METRIC_TYPE_GAUGE);
        *old = counter;
        ok = true;
    }

    if (reported_base)
        gpu_submit(gpu, &fam_energy);
    if (reported_rate)
        gpu_submit(gpu, &fam_power);

    if (reported_ratio) {
        gpu_submit(gpu, &fam_ratio);
    } else if (ratio_fail) {
        gpu->flags &= ~COLLECT_POWER_RATIO;
        if (ok)
            PLUGIN_WARNING("failed to get power limit(s) for any of the %u domain(s), "
                           "last error = 0x%x", power_count, limit_ret);
    }

    free(powers);
    return ok;
}

/* Report engine activity in relevant groups, return true for success */
static bool gpu_engines(gpu_device_t *gpu)
{
    if (!(config.output & (OUTPUT_BASE | OUTPUT_RATIO))) {
        PLUGIN_ERROR("no engine output variants selected");
        return false;
    }

    uint32_t engine_count = 0;
    zes_device_handle_t dev = gpu->handle;
    ze_result_t ret = zesDeviceEnumEngineGroups(dev, &engine_count, NULL);
    if (ret != ZE_RESULT_SUCCESS) {
        PLUGIN_ERROR("failed to get engine groups count => 0x%x", ret);
        return false;
    }

    zes_engine_handle_t *engines = calloc(engine_count, sizeof(*engines));
    if (engines == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return false;
    }

    ret = zesDeviceEnumEngineGroups(dev, &engine_count, engines);
    if (ret != ZE_RESULT_SUCCESS) {
        PLUGIN_ERROR("failed to get %u engine groups => 0x%x", engine_count, ret);
        free(engines);
        return false;
    }

    if (gpu->engine_count != engine_count) {
        PLUGIN_INFO(" Sysman reports %u engine groups", engine_count);
        if (gpu->engine) {
            free(gpu->engine);
        }
        gpu->engine = calloc(engine_count, sizeof(*gpu->engine));
        if (gpu->engine == NULL) {
            free(engines);
            PLUGIN_ERROR("calloc failed.");
            return false;
        }
        gpu->engine_count = engine_count;
    }

    metric_family_t fam_ratio = {
            .name = "gpu_intel_engine_ratio",
            .type = METRIC_TYPE_GAUGE,
            .help = "Average GPU engine / group utilization ratio (0-1) over query interval",
    };
    metric_family_t fam_counter = {
            .name = "gpu_intel_engine_use_usecs_total",
            .type = METRIC_TYPE_COUNTER,
            .help = "GPU engine / group execution time (activity) total (in microseconds)",
    };
    metric_t metric = {0};

    int type_idx[16] = {0};
    bool reported_ratio = false, reported_base = false, ok = false;
    for (uint32_t i = 0; i < engine_count; i++) {
        zes_engine_properties_t props = {.pNext = NULL};
        ret = zesEngineGetProperties(engines[i], &props);
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("failed to get engine group %u properties => 0x%x", i, ret);
            ok = false;
            break;
        }
        bool all = false;
        const char *type;
        switch (props.type) {
        case ZES_ENGINE_GROUP_ALL:
            type = "all";
            all = true;
            break;
            /* multiple engines */
        case ZES_ENGINE_GROUP_COMPUTE_ALL:
            type = "compute";
            all = true;
            break;
        case ZES_ENGINE_GROUP_MEDIA_ALL:
            type = "media";
            all = true;
            break;
        case ZES_ENGINE_GROUP_COPY_ALL:
            type = "copy";
            all = true;
            break;
            /* individual engines */
        case ZES_ENGINE_GROUP_COMPUTE_SINGLE:
            type = "compute";
            break;
        case ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE:
            type = "decode";
            break;
        case ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE:
            type = "encode";
            break;
        case ZES_ENGINE_GROUP_COPY_SINGLE:
            type = "copy";
            break;
        case ZES_ENGINE_GROUP_RENDER_SINGLE:
            type = "render";
            break;

        /* Following defines require at least Level-Zero relase v1.1 */
        case ZES_ENGINE_GROUP_RENDER_ALL:
            type = "render";
            all = true;
            break;
        case ZES_ENGINE_GROUP_3D_ALL:
            type = "3d";
            all = true;
            break;
        case ZES_ENGINE_GROUP_3D_RENDER_COMPUTE_ALL:
            type = "3d-render-compute";
            all = true;
            break;
        case ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE:
            type = "enhance";
            break;
        case ZES_ENGINE_GROUP_3D_SINGLE:
            type = "3d";
            break;

        default:
            type = "unknown";
        }
        const char *vname;
        char buf[32];
        if (all) {
            vname = type;
        } else {
            if (!(gpu->flags &COLLECT_ENGINE_SINGLE))
                continue;
            assert(props.type < sizeof(type_idx));
            /* include engine index as there can be multiple engines of same type */
            snprintf(buf, sizeof(buf), "%s-%03d", type, type_idx[props.type]);
            type_idx[props.type]++;
            vname = buf;
        }
        zes_engine_stats_t stats;
        ret = zesEngineGetActivity(engines[i], &stats);
        if (ret != ZE_RESULT_SUCCESS) {
            PLUGIN_ERROR("failed to get engine %u (%s) group activity => 0x%x", i, vname, ret);
            ok = false;
            break;
        }
        metric_set_subdev(&metric, props.onSubdevice, props.subdeviceId);
        metric_label_set(&metric, "type", vname);
        if (config.output & OUTPUT_BASE) {
            metric.value = VALUE_COUNTER(stats.activeTime);
            metric_family_metric_append(&fam_counter, metric);
            reported_base = true;
        }
        zes_engine_stats_t *old = &gpu->engine[i];
        if (old->timestamp && stats.timestamp > old->timestamp &&
                (config.output & OUTPUT_RATIO)) {
            metric.value = VALUE_GAUGE((double)(stats.activeTime - old->activeTime) /
                                               (stats.timestamp - old->timestamp));
            metric_family_metric_append(&fam_ratio, metric);
            reported_ratio = true;
        }
        metric_reset(&metric, METRIC_TYPE_GAUGE);
        *old = stats;
        ok = true;
    }

    if (reported_ratio)
        gpu_submit(gpu, &fam_ratio);
    if (reported_base)
        gpu_submit(gpu, &fam_counter);
    free(engines);

    return ok;
}

static int gpu_read(void)
{
    /* no metrics yet */
    int retval = RET_NO_METRICS;
    /* go through all GPUs */
    for (uint32_t i = 0; i < gpu_count; i++) {
        gpu_device_t *gpu = &gpus[i];
        if (gpu->flags == 0)
            continue;

        if (!gpu->check_count)
            PLUGIN_INFO("GPU-%u queries:", i);

        /* 'cache_idx' is high frequency sampling aggregation counter.
         *
         * Functions needing that should use gpu_subarray_realloc() to
         * allocate 'config.samples' sized array of metric value arrays,
         * and use 'cache_idx' as index to that array.
         *
         * 'cache_idx' goes down to zero, so that functions themselves
         * need to care less about config.samples value.    But when it
         * does reache zero, function should process 'config.samples'
         * amount of cached items and provide aggregated metrics of
         * them to gpu_submit().
         */
        unsigned int cache_idx = (config.samples - 1) - gpu->check_count % config.samples;
        /* get potentially high-frequency metrics data (aggregate metrics sent when * counter=0) */
        if ((gpu->flags & COLLECT_FREQUENCY) && !gpu_freqs(gpu, cache_idx)) {
            PLUGIN_WARNING("GPU-%u frequency query fail / no domains => disabled", i);
            gpu->flags &= ~COLLECT_FREQUENCY;
        }

        if ((gpu->flags & COLLECT_MEMORY) && !gpu_mems(gpu, cache_idx)) {
            PLUGIN_WARNING("GPU-%u memory query fail / no modules => disabled", i);
            gpu->flags &= ~COLLECT_MEMORY;
        }
        /* rest of the metrics are read only when the high frequency
         * counter goes down to zero
         */
        gpu->check_count++;
        if (cache_idx > 0) {
            if (gpu->flags) {
                /* there are still valid counters at least for this GPU */
                retval = RET_OK;
            }
            continue;
        }

        /* process lower frequency counters */
        if (config.samples > 1 && gpu->check_count <= config.samples) {
            PLUGIN_INFO("GPU-%u queries:", i);
        }
        /* get lower frequency metrics */
        if ((gpu->flags & COLLECT_ENGINE) && !gpu_engines(gpu)) {
            PLUGIN_WARNING("GPU-%u engine query fail / no groups => disabled", i);
            gpu->flags &= ~ COLLECT_ENGINE;
        }
        if ((gpu->flags & COLLECT_FABRIC) && !gpu_fabrics(gpu)) {
            PLUGIN_WARNING("GPU-%u fabric query fail / no fabric ports => disabled", i);
            gpu->flags &= ~COLLECT_FABRIC;
        }
        if ((gpu->flags & COLLECT_MEMORY_BANDWIDTH) && !gpu_mems_bw(gpu)) {
            PLUGIN_WARNING("GPU-%u mem BW query fail / no modules => disabled", i);
            gpu->flags &= ~COLLECT_MEMORY_BANDWIDTH;
        }
        if ((gpu->flags & COLLECT_POWER) && !gpu_powers(gpu)) {
            PLUGIN_WARNING("GPU-%u power query fail / no domains => disabled", i);
            gpu->flags &= ~COLLECT_POWER;
        }
        if ((gpu->flags & COLLECT_ERRORS) && !gpu_ras(gpu)) {
            PLUGIN_WARNING("GPU-%u errors query fail / no sets => disabled", i);
            gpu->flags &= ~COLLECT_ERRORS;
        }
        if ((gpu->flags & COLLECT_TEMPERATURE) && !gpu_temps(gpu)) {
            PLUGIN_WARNING("GPU-%u temperature query fail / no sensors => disabled", i);
            gpu->flags &= ~COLLECT_TEMPERATURE;
        }
        if ((gpu->flags & COLLECT_THROTTLETIME) && !gpu_freqs_throttle(gpu)) {
            PLUGIN_WARNING("GPU-%u throttle time query fail / no domains => disabled", i);
            gpu->flags &= ~COLLECT_THROTTLETIME;
        }

        if (gpu->flags & (COLLECT_ENGINE | COLLECT_ENGINE_SINGLE | COLLECT_FABRIC |
                COLLECT_FREQUENCY | COLLECT_MEMORY | COLLECT_MEMORY_BANDWIDTH |
                COLLECT_POWER | COLLECT_ERRORS | COLLECT_SEPARATE_ERRORS |
                COLLECT_TEMPERATURE | COLLECT_THROTTLETIME)) {
            /* all metrics missing -> disable use of that GPU */
            PLUGIN_ERROR("No metrics from GPU-%u, disabling its querying", i);
            gpu->flags = 0ULL;
        } else {
            retval = RET_OK;
        }
    }
    return retval;
}

static int gpu_config(config_item_t *ci)
{
    int status = 0;

    config.flags = ~0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "collect") == 0) {
            status = cf_util_get_flags(child, gpu_intel_flags, gpu_intel_flags_size, &config.flags);
        } else if (strcasecmp(child->key, "log-gpu-info") == 0) {
            status = cf_util_get_boolean(child, &config.gpuinfo);
        } else if (strcasecmp(child->key, "metrics-output") == 0) {
            char *flags = NULL;
            status = cf_util_get_string(child, &flags);
            if (status == 0) {
                config.output = 0;
                static const char delim[] = ",:/ ";

                char *save, *flag;
                for (flag = strtok_r(flags, delim, &save); flag; flag = strtok_r(NULL, delim, &save)) {
                    bool found = false;
                    for (unsigned j = 0; j < STATIC_ARRAY_SIZE(metrics_output); j++) {
                        if (strcasecmp(flag, metrics_output[j].name) == 0) {
                            config.output |= metrics_output[j].value;
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        free(flags);
                        PLUGIN_ERROR("Invalid '%s' config key value '%s'", child->key, ci->values[0].value.string);
                        return RET_INVALID_CONFIG;
                    }
                }
                free(flags);
                if (!config.output) {
                    PLUGIN_ERROR("Invalid '%s' config key value '%s'", child->key, ci->values[0].value.string);
                    return RET_INVALID_CONFIG;
                }
            }
        } else if (strcasecmp(child->key, "samples") == 0) {
            /* because collectd converts config values to floating point strings,
             * this can't use strtol() to check that value is integer, so simply
             * just take the integer part
             */
            int samples = 0;
            status = cf_util_get_int(child, &samples);
            if (status == 0) {
                if (samples < 1 || samples > MAX_SAMPLES) {
                    PLUGIN_ERROR("Invalid 'samples' value '%d'", samples);
                    return RET_INVALID_CONFIG;
                }
                /* number of samples cannot be changed without freeing per-GPU
                 * metrics cache arrays & members, zeroing metric counters and
                 * GPU cache index counter.  However, this parse function should
                 * be called only before gpu structures have been initialized, so
                 * just assert here
                 */
                assert(gpus == NULL);
                config.samples = samples;
            }
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

void module_register(void)
{
    plugin_register_config("gpu_intel", gpu_config);
    plugin_register_init("gpu_intel", gpu_init);
    plugin_register_read("gpu_intel", gpu_read);
    plugin_register_shutdown("gpu_intel", gpu_config_free);
}
