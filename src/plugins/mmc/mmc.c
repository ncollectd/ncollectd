// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) Florian Eckert
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian Eckert <fe at dev.tdt.de>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

#include <libudev.h>
#include <linux/major.h>
#include <linux/mmc/ioctl.h>
#include <sys/ioctl.h>

#define MMC_BLOCK_SIZE 512

// The flags are what emmcparm uses and translate to (include/linux/mmc/core.h):
// MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE | MMC_CMD_ADTC |
// MMC_RSP_SPI_S1
#define MICRON_CMD56_FLAGS 0x00b5
#define MICRON_CMD56ARG_BAD_BLOCKS 0x11
#define MICRON_CMD56ARG_ERASES_SLC 0x23
#define MICRON_CMD56ARG_ERASES_MLC 0x25

// Copy what worked for the micron eMMC but allow busy response in report mode
// enable flags (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE | MMC_CMD_ADTC |
// MMC_RSP_BUSY | MMC_RSP_SPI_S1 | MMC_RSP_SPI_BUSY) The _ARG is a magic value
// from the datasheet
#define SANDISK_CMD_EN_REPORT_MODE_FLAGS 0x04bd
#define SANDISK_CMD_EN_REPORT_MODE_OP 62
#define SANDIKS_CMD_EN_REPORT_MODE_ARG 0x96C9D71C

#define SANDISK_CMD_READ_REPORT_FLAGS 0x00b5
#define SANDISK_CMD_READ_REPORT_OP 63
#define SANDISK_CMD_READ_REPORT_ARG 0

// Fields in the Device Report / Advanced Health Status structure
#define SANDISK_FIELDS_POWER_UPS 25
#define SANDISK_FIELDS_TEMP_CUR 41

#define SANDISK_FIELDS_BB_INITIAL 6
#define SANDISK_FIELDS_BB_RUNTIME_MLC 9
#define SANDISK_FIELDS_BB_RUNTIME_SLC 36
#define SANDISK_FIELDS_BB_RUNTIME_SYS 7

#define SANDISK_FIELDS_ER_MLC_AVG 2
#define SANDISK_FIELDS_ER_MLC_MIN 31
#define SANDISK_FIELDS_ER_MLC_MAX 28

#define SANDISK_FIELDS_ER_SLC_AVG 34
#define SANDISK_FIELDS_ER_SLC_MIN 33
#define SANDISK_FIELDS_ER_SLC_MAX 32

#define SANDISK_FIELDS_ER_SYS_AVG 0
#define SANDISK_FIELDS_ER_SYS_MIN 29
#define SANDISK_FIELDS_ER_SYS_MAX 26
// Size of string buffer with '\0'
#define SWISSBIT_LENGTH_SPARE_BLOCKS 3
#define SWISSBIT_LENGTH_BLOCK_ERASES 13
#define SWISSBIT_LENGTH_POWER_ON 9

#define SWISSBIT_SSR_START_SPARE_BLOCKS 66
#define SWISSBIT_SSR_START_BLOCK_ERASES 92
#define SWISSBIT_SSR_START_POWER_ON 112

enum mmc_manfid {
    MANUFACTUR_MICRON = 0x13,
    MANUFACTUR_SANDISK = 0x45,
    MANUFACTUR_SWISSBIT = 0x5d,
};

enum mmc_oemid_swissbit {
    OEMID_SWISSBIT_1 = 21314, // 0x5342
};

enum {
    FAM_MMC_BAD_BLOCKS,
    FAM_MMC_BLOCK_ERASES,
    FAM_MMC_SPARE_BLOCKS,
    FAM_MMC_POWER_CYCLES,
    FAM_MMC_TEMPERATURE,
    FAM_MMC_ERASES_SLC_MIN,
    FAM_MMC_ERASES_SLC_MAX,
    FAM_MMC_ERASES_SLC_AVG,
    FAM_MMC_ERASES_MLC_MIN,
    FAM_MMC_ERASES_MLC_MAX,
    FAM_MMC_ERASES_MLC_AVG,
    FAM_MMC_ERASES_SYS_MAX,
    FAM_MMC_ERASES_SYS_MIN,
    FAM_MMC_ERASES_SYS_AVG,
    FAM_MMC_LIFE_TIME_EST_TYP_A,
    FAM_MMC_LIFE_TIME_EST_TYP_B,
    FAM_MMC_PRE_EOL_INFO,
    FAM_MMC_MAX,
};

static metric_family_t fams[FAM_MMC_MAX] = {
    [FAM_MMC_BAD_BLOCKS] = {
        .name = "system_mmc_bad_blocks",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MMC_BLOCK_ERASES] = {
        .name = "system_mmc_block_erases",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MMC_SPARE_BLOCKS] = {
        .name = "system_mmc_spare_blocks",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MMC_POWER_CYCLES] = {
        .name = "system_mmc_power_cycles",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MMC_TEMPERATURE] = {
        .name = "system_mmc_temperature",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MMC_ERASES_SLC_MIN] = {
        .name = "system_mmc_erases_slc_min",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MMC_ERASES_SLC_MAX] = {
        .name = "system_mmc_erases_slc_max",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MMC_ERASES_SLC_AVG] = {
        .name = "system_mmc_erases_slc_avg",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MMC_ERASES_MLC_MIN] = {
        .name = "system_mmc_erases_mlc_min",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MMC_ERASES_MLC_MAX] = {
        .name = "system_mmc_erases_mlc_max",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MMC_ERASES_MLC_AVG] = {
        .name = "system_mmc_erases_mlc_avg",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MMC_ERASES_SYS_MAX] = {
        .name = "system_mmc_erases_sys_max",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MMC_ERASES_SYS_MIN] = {
        .name = "system_mmc_erases_sys_min",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MMC_ERASES_SYS_AVG] = {
        .name = "system_mmc_erases_sys_avg",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MMC_LIFE_TIME_EST_TYP_A] = {
        .name = "system_mmc_life_time_est_typ_a",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MMC_LIFE_TIME_EST_TYP_B] = {
        .name = "system_mmc_life_time_est_typ_b",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MMC_PRE_EOL_INFO] = {
        .name = "system_mmc_pre_eol_info",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
};

// Cache of open file descriptors for /dev/mmcblk? block devices.
// The purpose of caching the file descriptors instead of open()/close()ing for
// every read is to prevent the generation of udev events for every close().
typedef struct dev_cache_entry_s {
    char *path;
    int fd;
    struct dev_cache_entry_s *next;
} dev_cache_entry_t;

static dev_cache_entry_t *block_dev_cache = NULL;

static exclist_t excl_device;

static int mmc_read_manfid(struct udev_device *mmc_dev, int *value)
{
    const char *attr = udev_device_get_sysattr_value(mmc_dev, "manfid");
    if (attr == NULL) {
        PLUGIN_WARNING("(%s): Unable to read manufacturer identifier (manfid)",
                        udev_device_get_sysname(mmc_dev));
        return 1;
    }

    *value = (int)strtol(attr, NULL, 0);
    return 0;
}

static int mmc_read_oemid(struct udev_device *mmc_dev, int *value)
{
    const char *attr = udev_device_get_sysattr_value(mmc_dev, "oemid");
    if (attr == NULL) {
        PLUGIN_WARNING("(%s): Unable to read original equipment manufacturer identifier (oemid)",
                        udev_device_get_sysname(mmc_dev));
        return 1;
    }

    *value = (int)strtol(attr, NULL, 0);
    return 0;
}

static int mmc_read_emmc_generic(struct udev_device *mmc_dev)
{
    int res = 1;

    const char *dev_name = udev_device_get_sysname(mmc_dev);
    const char *attr_life_time = udev_device_get_sysattr_value(mmc_dev, "life_time");
    const char *attr_pre_eol = udev_device_get_sysattr_value(mmc_dev, "pre_eol_info");

    // write generic eMMC 5.0 lifetime estimates
    if (attr_life_time != NULL) {
        uint8_t life_time_a, life_time_b;
        if (sscanf(attr_life_time, "%hhx %hhx", &life_time_a, &life_time_b) == 2) {
            metric_family_append(&fams[FAM_MMC_LIFE_TIME_EST_TYP_A],
                                 VALUE_GAUGE((double)life_time_a), NULL,
                                 &(label_pair_const_t){.name="device", .value=dev_name}, NULL);
            metric_family_append(&fams[FAM_MMC_LIFE_TIME_EST_TYP_B],
                                 VALUE_GAUGE((double)life_time_b), NULL,
                                 &(label_pair_const_t){.name="device", .value=dev_name}, NULL);
            res = 0;
        }
    }

    // write generic eMMC 5.0 pre_eol estimate
    if (attr_pre_eol != NULL) {
        uint8_t pre_eol;
        if (sscanf(attr_pre_eol, "%hhx", &pre_eol) == 1) {
            metric_family_append(&fams[FAM_MMC_PRE_EOL_INFO], VALUE_GAUGE((double)pre_eol), NULL,
                                 &(label_pair_const_t){.name="device", .value=dev_name}, NULL);
            res = 0;
        }
    }

    return res;
}

// mmc_open_block_dev open file descriptor for device at path "dev_path" or
// return a fd from a previous invocation.
static int mmc_open_block_dev(const char *dev_name, const char *dev_path)
{
    if (dev_path == NULL) {
        PLUGIN_INFO("(%s) failed to find block device", dev_name);
        return -1;
    }

    // Check if we have already opened this block device before.
    // The purpose of this file descriptor caching is to prevent the generation
    // of periodic udev events.
    // Why does udev generate an event whenever a block device opened for
    // writing is closed? Because this usually happens when a device is
    // partitioned or mkfs is called (e.g. an actual change to the dev).
    // This is however not what we do. We only need O_RDWR to send special MMC
    // commands which do not modify the content of the device, so we don't want
    // to generate the events.
    for (dev_cache_entry_t *piv = block_dev_cache; piv != NULL; piv = piv->next) {
        if (strcmp(dev_path, piv->path) == 0) {
            return piv->fd;
        }
    }

    // This dev_path was not opened before. Open it now:
    int block_fd = open(dev_path, O_RDWR);
    if (block_fd < 0) {
        PLUGIN_INFO("(%s) failed to open block device (%s): (%s)", dev_name, dev_path, strerror(errno));
        return -1;
    }

    // And add it to the cache of already opened block devices:
    dev_cache_entry_t *cache_entry = calloc(1, sizeof(*cache_entry));
    if (cache_entry == NULL) {
        close(block_fd);
        PLUGIN_ERROR("(%s) failed to allocate memory (%s)", dev_name, strerror(errno));
        return -1;
    }

    cache_entry->path = strdup(dev_path);
    if (cache_entry->path == NULL) {
        close(block_fd);
        PLUGIN_ERROR("(%s) failed to copy path string (%s)", dev_name, strerror(errno));
        free(cache_entry);
        return -1;
    }

    cache_entry->fd = block_fd;
    cache_entry->next = block_dev_cache;
    block_dev_cache = cache_entry;

    return cache_entry->fd;
}

// mmc_close_block_dev close a file descriptor returned by mmc_open_block_dev.
static int mmc_close_block_dev(int fd)
{
    for (dev_cache_entry_t **elem_ptr = &block_dev_cache; *elem_ptr != NULL;
             elem_ptr = &(*elem_ptr)->next) {
        dev_cache_entry_t *elem = *elem_ptr;

        if (elem->fd == fd) {
            // Unhook the element from the linked list by overwriting the pointer
            // that pointed to it. This could be &block_dev_cache or the &->next
            // pointer of the previous element.
            *elem_ptr = elem->next;

            free(elem->path);
            free(elem);

            return close(fd);
        }
    }

    return -1;
}

static int mmc_micron_cmd56(int block_fd, uint32_t arg, uint16_t *val1,
                                          uint16_t *val2, uint16_t *val3)
{
    uint16_t cmd_data[MMC_BLOCK_SIZE / sizeof(uint16_t)];
    struct mmc_ioc_cmd cmd = {
        .opcode = 56,
        .arg = arg,
        .flags = MICRON_CMD56_FLAGS,
        .blksz = sizeof(cmd_data),
        .blocks = 1,
    };

    mmc_ioc_cmd_set_data(cmd, cmd_data);

    if (ioctl(block_fd, MMC_IOC_CMD, &cmd) < 0)
        return 1;

    *val1 = be16toh(cmd_data[0]);
    *val2 = be16toh(cmd_data[1]);
    *val3 = be16toh(cmd_data[2]);

    return 0;
}

static int mmc_read_micron(struct udev_device *mmc_dev, struct udev_device *block_dev)
{
    const char *dev_name = udev_device_get_sysname(mmc_dev);
    const char *dev_path = udev_device_get_devnode(block_dev);

    int block_fd = mmc_open_block_dev(dev_name, dev_path);
    if (block_fd < 0)
        return 1;

    uint16_t bb_initial, bb_runtime, bb_remaining;
    if (mmc_micron_cmd56(block_fd, MICRON_CMD56ARG_BAD_BLOCKS, &bb_initial,
                                   &bb_runtime, &bb_remaining) != 0) {
        PLUGIN_INFO("(%s) failed to send ioctl to %s: %s", dev_name, dev_path, strerror(errno));
        mmc_close_block_dev(block_fd);
        return 1;
    }

    uint16_t er_slc_min, er_slc_max, er_slc_avg;
    if (mmc_micron_cmd56(block_fd, MICRON_CMD56ARG_ERASES_SLC, &er_slc_min,
                                   &er_slc_max, &er_slc_avg) != 0) {
        PLUGIN_INFO("(%s) failed to send ioctl to %s: %s", dev_name, dev_path, strerror(errno));
        mmc_close_block_dev(block_fd);
        return 1;
    }

    uint16_t er_mlc_min, er_mlc_max, er_mlc_avg;
    if (mmc_micron_cmd56(block_fd, MICRON_CMD56ARG_ERASES_MLC, &er_mlc_min,
                                   &er_mlc_max, &er_mlc_avg) != 0) {
        PLUGIN_INFO("(%s) failed to send ioctl to %s: %s", dev_name, dev_path, strerror(errno));
        mmc_close_block_dev(block_fd);
        return 1;
    }


    double bb_total = (double)(bb_initial) + (double)(bb_runtime);

    metric_family_append(&fams[FAM_MMC_BAD_BLOCKS], VALUE_GAUGE(bb_total), NULL,
                         &(label_pair_const_t){.name="device", .value=dev_name}, NULL);

    metric_family_append(&fams[FAM_MMC_SPARE_BLOCKS], VALUE_GAUGE((double)(bb_remaining)), NULL,
                         &(label_pair_const_t){.name="device", .value=dev_name}, NULL);

    metric_family_append(&fams[FAM_MMC_ERASES_SLC_MIN], VALUE_GAUGE((double)(er_slc_min)), NULL,
                         &(label_pair_const_t){.name="device", .value=dev_name}, NULL);
    metric_family_append(&fams[FAM_MMC_ERASES_SLC_MAX], VALUE_GAUGE((double)(er_slc_max)), NULL,
                         &(label_pair_const_t){.name="device", .value=dev_name}, NULL);
    metric_family_append(&fams[FAM_MMC_ERASES_SLC_AVG], VALUE_GAUGE((double)(er_slc_avg)), NULL,
                         &(label_pair_const_t){.name="device", .value=dev_name}, NULL);

    metric_family_append(&fams[FAM_MMC_ERASES_MLC_MIN], VALUE_GAUGE((double)(er_mlc_min)), NULL,
                         &(label_pair_const_t){.name="device", .value=dev_name}, NULL);
    metric_family_append(&fams[FAM_MMC_ERASES_MLC_MAX], VALUE_GAUGE((double)(er_mlc_max)), NULL,
                         &(label_pair_const_t){.name="device", .value=dev_name}, NULL);
    metric_family_append(&fams[FAM_MMC_ERASES_MLC_AVG], VALUE_GAUGE((double)(er_mlc_avg)), NULL,
                         &(label_pair_const_t){.name="device", .value=dev_name}, NULL);

    return 0;
}

static int mmc_read_sandisk(struct udev_device *mmc_dev, struct udev_device *block_dev)
{
    uint32_t cmd_data[MMC_BLOCK_SIZE / sizeof(uint32_t)];

    struct mmc_ioc_cmd cmd_en_report_mode = {
        .opcode = SANDISK_CMD_EN_REPORT_MODE_OP,
        .arg = SANDIKS_CMD_EN_REPORT_MODE_ARG,
        .flags = SANDISK_CMD_EN_REPORT_MODE_FLAGS,
    };
    struct mmc_ioc_cmd cmd_read_report = {
        .opcode = SANDISK_CMD_READ_REPORT_OP,
        .arg = SANDISK_CMD_READ_REPORT_ARG,
        .flags = SANDISK_CMD_READ_REPORT_FLAGS,
        .blksz = sizeof(cmd_data),
        .blocks = 1,
    };

    const char *dev_name = udev_device_get_sysname(mmc_dev);
    const char *dev_path = udev_device_get_devnode(block_dev);

    int block_fd = mmc_open_block_dev(dev_name, dev_path);
    if (block_fd < 0) {
        return 1;
    }

    mmc_ioc_cmd_set_data(cmd_read_report, cmd_data);

    if (ioctl(block_fd, MMC_IOC_CMD, &cmd_en_report_mode) < 0) {
        mmc_close_block_dev(block_fd);
        PLUGIN_INFO("(%s) failed to send enable report mode MMC ioctl to %s: %s",
                    dev_name, dev_path, strerror(errno));
        return 1;
    }

    if (ioctl(block_fd, MMC_IOC_CMD, &cmd_read_report) < 0) {
        mmc_close_block_dev(block_fd);
        PLUGIN_INFO("(%s) failed to send read_report MMC ioctl to %s: %s",
                    dev_name, dev_path, strerror(errno));
        return 1;
    }

    double bb_total = le32toh(cmd_data[SANDISK_FIELDS_BB_INITIAL]) +
                      le32toh(cmd_data[SANDISK_FIELDS_BB_RUNTIME_MLC]) +
                      le32toh(cmd_data[SANDISK_FIELDS_BB_RUNTIME_SLC]) +
                      le32toh(cmd_data[SANDISK_FIELDS_BB_RUNTIME_SYS]);

    metric_family_append(&fams[FAM_MMC_BAD_BLOCKS], VALUE_GAUGE(bb_total), NULL,
                         &(label_pair_const_t){.name="device", .value=dev_name}, NULL);

    metric_family_append(&fams[FAM_MMC_POWER_CYCLES],
                         VALUE_GAUGE((double)le32toh(cmd_data[SANDISK_FIELDS_POWER_UPS])), NULL,
                         &(label_pair_const_t){.name="device", .value=dev_name}, NULL);

    metric_family_append(&fams[FAM_MMC_TEMPERATURE],
                         VALUE_GAUGE((double)le32toh(cmd_data[SANDISK_FIELDS_TEMP_CUR])), NULL,
                         &(label_pair_const_t){.name="device", .value=dev_name}, NULL);

    metric_family_append(&fams[FAM_MMC_ERASES_MLC_AVG],
                         VALUE_GAUGE((double)le32toh(cmd_data[SANDISK_FIELDS_ER_MLC_AVG])), NULL,
                         &(label_pair_const_t){.name="device", .value=dev_name}, NULL);
    metric_family_append(&fams[FAM_MMC_ERASES_MLC_MAX],
                         VALUE_GAUGE((double)le32toh(cmd_data[SANDISK_FIELDS_ER_MLC_MAX])), NULL,
                         &(label_pair_const_t){.name="device", .value=dev_name}, NULL);
    metric_family_append(&fams[FAM_MMC_ERASES_MLC_MIN],
                         VALUE_GAUGE((double)le32toh(cmd_data[SANDISK_FIELDS_ER_MLC_MIN])), NULL,
                         &(label_pair_const_t){.name="device", .value=dev_name}, NULL);

    metric_family_append(&fams[FAM_MMC_ERASES_SLC_AVG],
                         VALUE_GAUGE((double)le32toh(cmd_data[SANDISK_FIELDS_ER_SLC_AVG])), NULL,
                         &(label_pair_const_t){.name="device", .value=dev_name}, NULL);
    metric_family_append(&fams[FAM_MMC_ERASES_SLC_MAX],
                         VALUE_GAUGE((double)le32toh(cmd_data[SANDISK_FIELDS_ER_SLC_MAX])), NULL,
                         &(label_pair_const_t){.name="device", .value=dev_name}, NULL);
    metric_family_append(&fams[FAM_MMC_ERASES_SLC_MIN],
                         VALUE_GAUGE((double)le32toh(cmd_data[SANDISK_FIELDS_ER_SLC_MIN])), NULL,
                         &(label_pair_const_t){.name="device", .value=dev_name}, NULL);

    metric_family_append(&fams[FAM_MMC_ERASES_SYS_AVG],
                         VALUE_GAUGE((double)le32toh(cmd_data[SANDISK_FIELDS_ER_SYS_AVG])), NULL,
                         &(label_pair_const_t){.name="device", .value=dev_name}, NULL);
    metric_family_append(&fams[FAM_MMC_ERASES_SYS_MAX],
                         VALUE_GAUGE((double)le32toh(cmd_data[SANDISK_FIELDS_ER_SYS_MAX])), NULL,
                         &(label_pair_const_t){.name="device", .value=dev_name}, NULL);
    metric_family_append(&fams[FAM_MMC_ERASES_SYS_MIN],
                         VALUE_GAUGE((double)le32toh(cmd_data[SANDISK_FIELDS_ER_SYS_MIN])), NULL,
                         &(label_pair_const_t){.name="device", .value=dev_name}, NULL);

    return 0;
}

static int mmc_read_ssr_swissbit(struct udev_device *mmc_dev)
{
    const char *dev_name = udev_device_get_sysname(mmc_dev);

    int oemid;
    if (mmc_read_oemid(mmc_dev, &oemid) != 0) {
        return 1;
    }

    if (oemid != OEMID_SWISSBIT_1) {
        PLUGIN_INFO("(%s): The mmc device is not supported by this plugin (oemid: 0x%x)",
                    dev_name, (unsigned int)oemid);
        return 1;
    }

    const char *attr = udev_device_get_sysattr_value(mmc_dev, "ssr");
    if (attr == NULL)
        return 1;

    /*
     * Since the register is read out as a byte stream, it is 128 bytes long.
     * One char represents a half byte (nibble).
     *
     */
    int length = strlen(attr);
    PLUGIN_DEBUG("%d byte read from SSR register", length);
    if (length < 128) {
        PLUGIN_INFO("(%s): The SSR register is not 128 byte long", dev_name);
        return 1;
    }

    PLUGIN_DEBUG("(%s): [ssr]=%s", dev_name, attr);

    int value;

    char bad_blocks[SWISSBIT_LENGTH_SPARE_BLOCKS];
    sstrncpy(bad_blocks, &attr[SWISSBIT_SSR_START_SPARE_BLOCKS], sizeof(bad_blocks) - 1);
    bad_blocks[sizeof(bad_blocks) - 1] = '\0';
    value = (int)strtol(bad_blocks, NULL, 16);
    /* convert to more common bad blocks information */
    value = abs(value - 100);
    PLUGIN_DEBUG("(%s): [bad_blocks] str=%s int=%d", dev_name, bad_blocks, value);
    metric_family_append(&fams[FAM_MMC_BAD_BLOCKS], VALUE_GAUGE(value), NULL,
                         &(label_pair_const_t){.name="device", .value=dev_name}, NULL);

    char block_erases[SWISSBIT_LENGTH_BLOCK_ERASES];
    sstrncpy(block_erases, &attr[SWISSBIT_SSR_START_BLOCK_ERASES], sizeof(block_erases) - 1);
    block_erases[sizeof(block_erases) - 1] = '\0';
    value = (int)strtol(block_erases, NULL, 16);
    PLUGIN_DEBUG("(%s): [block_erases] str=%s int=%d", dev_name, block_erases, value);
    metric_family_append(&fams[FAM_MMC_BLOCK_ERASES], VALUE_GAUGE(value), NULL,
                         &(label_pair_const_t){.name="device", .value=dev_name}, NULL);

    char power_on[SWISSBIT_LENGTH_POWER_ON];
    sstrncpy(power_on, &attr[SWISSBIT_SSR_START_POWER_ON], sizeof(power_on) - 1);
    power_on[sizeof(power_on) - 1] = '\0';
    value = (int)strtol(power_on, NULL, 16);
    PLUGIN_DEBUG("(%s): [power_on] str=%s int=%d", dev_name, power_on, value);
    metric_family_append(&fams[FAM_MMC_POWER_CYCLES], VALUE_GAUGE(value), NULL,
                         &(label_pair_const_t){.name="device", .value=dev_name}, NULL);

    return 0;
}

static int mmc_read(void)
{
    struct udev *handle_udev = udev_new();
    if (!handle_udev) {
        PLUGIN_ERROR("unable to initialize udev for device enumeration");
        return -1;
    }

    struct udev_enumerate *enumerate = udev_enumerate_new(handle_udev);
    if (enumerate == NULL) {
        PLUGIN_ERROR("udev_enumerate_new failed");
        return -1;
    }

    udev_enumerate_add_match_subsystem(enumerate, "block");

    if (udev_enumerate_scan_devices(enumerate) < 0) {
        PLUGIN_WARNING("udev scan devices failed");
        return -1;
    }

    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
    if (devices == NULL) {
        PLUGIN_WARNING("udev did not return any block devices");
        return -1;
    }

    struct udev_list_entry *dev_list_entry;
    udev_list_entry_foreach(dev_list_entry, devices) {
        const char *path = udev_list_entry_get_name(dev_list_entry);
        struct udev_device *block_dev = udev_device_new_from_syspath(handle_udev, path);

        // Get the parent of the block device.
        // Note that _get_parent() just gives us its reference to the parent device
        // and does not increment the reference count, so mmc_dev should not be
        // _unrefed.
        struct udev_device *mmc_dev = udev_device_get_parent(block_dev);
        if (!mmc_dev) {
            udev_device_unref(block_dev);
            continue;
        }

        // Select only block devices that have a mmcblk device as first parent.
        // This selects e.g. /dev/mmcblk1, but not /dev/mmcblk1p* or
        // /dev/mmcblk1boot* and especially not /dev/sda, /dev/vda ....
        const char *driver = udev_device_get_driver(mmc_dev);
        if (driver == NULL || strcmp(driver, "mmcblk") != 0) {
            udev_device_unref(block_dev);
            continue;
        }

        // Check if Device name (Something like "mmc2:0001") matches an entry in the
        // ignore list
        const char *dev_name = udev_device_get_sysname(mmc_dev);
        if (!exclist_match(&excl_device, dev_name)) {
            udev_device_unref(block_dev);
            continue;
        }

        // Read generic health metrics that should be available for all eMMC 5.0+
        // devices.
        bool have_stats = (mmc_read_emmc_generic(mmc_dev) == 0);

        // Read more datailed vendor-specific health info
        int manfid;
        if (mmc_read_manfid(mmc_dev, &manfid) == 0) {
            switch (manfid) {
            case MANUFACTUR_MICRON:
                have_stats |= (mmc_read_micron(mmc_dev, block_dev) == 1);
                break;
            case MANUFACTUR_SANDISK:
                have_stats |= (mmc_read_sandisk(mmc_dev, block_dev) == 1);
                break;
            case MANUFACTUR_SWISSBIT:
                have_stats |= (mmc_read_ssr_swissbit(mmc_dev) == 0);
                break;
            }
        }

        // Print a warning if no info at all could be collected for a device
        if (!have_stats)
            PLUGIN_INFO("(%s): Could not collect any info for device", dev_name);

        udev_device_unref(block_dev);
    }

    udev_enumerate_unref(enumerate);
    udev_unref(handle_udev);

    plugin_dispatch_metric_family_array(fams, FAM_MMC_MAX, 0);

    return 0;
}

static int mmc_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "device") == 0) {
            status = cf_util_exclist(child, &excl_device);
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

int mmc_shutdown(void)
{
    while (block_dev_cache != NULL) {
        mmc_close_block_dev(block_dev_cache->fd);
    }

    exclist_reset(&excl_device);
    return 0;
}

void module_register(void)
{
    plugin_register_config("mmc", mmc_config);
    plugin_register_read("mmc", mmc_read);
    plugin_register_shutdown("mmc", mmc_shutdown);
}
