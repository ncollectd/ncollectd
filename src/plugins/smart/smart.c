// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2014 Vincent Bernat
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Vincent Bernat <vbe at exoscale.ch>
// SPDX-FileContributor: Maciej Fijalkowski <maciej.fijalkowski at intel.com>
// SPDX-FileContributor: Bartlomiej Kotlowski <bartlomiej.kotlowski at intel.com>
// SPDX-FileContributor: Slawomir Strehlau <slawomir.strehlau at intel.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#include <atasmart.h>
#include <libudev.h>
#include <sys/ioctl.h>

#include "intel-nvme.h"
#include "nvme.h"

#ifdef HAVE_SYS_CAPABILITY_H
#include <sys/capability.h>
#endif

#include "smart_fams.h"

#define O_RDWR 02
#define NVME_SMART_CDW10 0x00800002
#define SHIFT_BYTE_LEFT 256
struct nvme_admin_cmd {
    __u8 opcode;
    __u8 rsvd1[3];
    __u32 nsid;
    __u8 rsvd2[16];
    __u64 addr;
    __u8 rsvd3[4];
    __u32 data_len;
    __u32 cdw10;
    __u32 cdw11;
    __u8 rsvd4[24];
};

#define NVME_IOCTL_ADMIN_CMD _IOWR('N', 0x41, struct nvme_admin_cmd)

typedef struct {
    char const *name;
    metric_family_t *fams;
    label_set_t *labels;
} smart_user_data_t;

static exclist_t excl_disk;
static exclist_t excl_serial;

static bool ignore_sleep_mode;
static bool use_serial;

static int pagesize;

static void *smart_alloc(size_t len)
{
    len = ((len + 0x1000 - 1) / 0x1000) * 0x1000;

    void *p = NULL;
    int status = posix_memalign((void *)&p, pagesize, len);
    if (status != 0)
        return NULL;

    memset(p, 0, len);
    return p;
}

static int create_ignorelist_by_serial(void)
{
    // Use udev to get a list of disks
    struct udev *handle_udev = udev_new();
    if (!handle_udev) {
        PLUGIN_ERROR("unable to initialize udev.");
        return 1;
    }

    struct udev_enumerate *enumerate = udev_enumerate_new(handle_udev);
    if (enumerate == NULL) {
        PLUGIN_ERROR("fail udev_enumerate_new");
        return 1;
    }

    udev_enumerate_add_match_subsystem(enumerate, "block");
    udev_enumerate_add_match_property(enumerate, "DEVTYPE", "disk");
    udev_enumerate_scan_devices(enumerate);
    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
    if (devices == NULL) {
        PLUGIN_ERROR("udev returned an empty list deviecs");
        return 1;
    }
    struct udev_list_entry *dev_list_entry;
    udev_list_entry_foreach(dev_list_entry, devices) {
        const char *path = udev_list_entry_get_name(dev_list_entry);
        struct udev_device *dev = udev_device_new_from_syspath(handle_udev, path);
        const char *devpath = udev_device_get_devnode(dev);
        const char *serial = udev_device_get_property_value(dev, "ID_SERIAL_SHORT");
        if (devpath != NULL) {
            if (exclist_match(&excl_disk, devpath) && (serial != NULL)) {
                exclist_add_incl_string(&excl_serial, serial);
            }
        }
    }

    return 0;
}

static void handle_attribute(__attribute__((unused)) SkDisk *d,
                             const SkSmartAttributeParsedData *a, void *userdata)
{
    smart_user_data_t *ud = userdata;

    if (!a->current_value_valid || !a->worst_value_valid)
        return;

    char id[24];
    ssnprintf(id, sizeof(id), "%u", (unsigned int) a->id);

    metric_family_append(&ud->fams[FAM_SMART_ATTRIBUTE_CURRENT],
                         VALUE_GAUGE(a->current_value), ud->labels,
                         &(label_pair_const_t){.name="attribute", .value=a->name,},
                         &(label_pair_const_t){.name="attribute_id", .value=id,},
                         NULL);
    metric_family_append(&ud->fams[FAM_SMART_ATTRIBUTE_PRETTY],
                         VALUE_GAUGE(a->pretty_value), ud->labels,
                         &(label_pair_const_t){.name="attribute", .value=a->name,},
                         &(label_pair_const_t){.name="attribute_id", .value=id,},
                         NULL);
    metric_family_append(&ud->fams[FAM_SMART_ATTRIBUTE_THRESHOLD],
                         VALUE_GAUGE(a->threshold_valid ? a->threshold : 0),
                         ud->labels,
                         &(label_pair_const_t){.name="attribute", .value=a->name,},
                         &(label_pair_const_t){.name="attribute_id", .value=id,},
                         NULL);
    metric_family_append(&ud->fams[FAM_SMART_ATTRIBUTE_WORST],
                         VALUE_GAUGE(a->worst_value), ud->labels,
                         &(label_pair_const_t){.name="attribute", .value=a->name,},
                         &(label_pair_const_t){.name="attribute_id", .value=id,},
                         NULL);

    if (a->threshold_valid && a->current_value <= a->threshold) {
        notification_t n = {
            .severity = NOTIF_WARNING,
            .time = cdtime(),
            .name = "smart_attribute",
        };

        notification_label_set(&n, "device", ud->name);
        notification_label_set(&n, "attribute", a->name);

        char message[1024];
        ssnprintf(message, sizeof(message), "attribute %s is below allowed threshold (%d < %d)",
                                            a->name, a->current_value, a->threshold);
        notification_annotation_set(&n, "summary", message);

        ssnprintf(message, sizeof(message), "%d", a->current_value);
        notification_annotation_set(&n, "current_value", message);

        ssnprintf(message, sizeof(message), "%d", a->threshold);
        notification_annotation_set(&n, "threshold", message);

        plugin_dispatch_notification(&n);
    }
}

static inline uint16_t le16_to_cpu(__le16 x)
{
    return le16toh((__force __u16)x);
}

static inline double int96_to_double(__u8 *data)
{
    double sum = 0;
    for (int i = 0; i < 16; i++) {
        double add = data[15 - i];
        for (int j = i + 1; j < 16; j++) {
            add *= SHIFT_BYTE_LEFT;
        }
        sum += add;
    }
    return sum;
}

static inline double int48_to_double(__u8 *data)
{
    double sum = 0;
    for (int i = 0; i < 6; i++) {
        double add = data[5 - i];
        for (int j = i + 1; j < 6; j++) {
            add *= SHIFT_BYTE_LEFT;
        }
        sum += add;
    }
    return sum;
}

static int get_vendor_id(const char *dev, __attribute__((unused)) char const *name)
{
    __le16 *vid = smart_alloc(sizeof(*vid));
    if (vid == NULL) {
        PLUGIN_ERROR("Failed to alloc __len16.");
        return -1;
    }

    int fd = open(dev, O_RDWR);
    if (unlikely(fd < 0)) {
        PLUGIN_ERROR("open failed with %s\n", strerror(errno));
        free(vid);
        return fd;
    }

    int err = ioctl(fd, NVME_IOCTL_ADMIN_CMD,
                     &(struct nvme_admin_cmd){.opcode = NVME_ADMIN_IDENTIFY,
                                              .nsid = 0,
                                              .addr = (unsigned long)vid,
                                              .data_len = sizeof(*vid),
                                              .cdw10 = 1,
                                              .cdw11 = 0});

    if (unlikely(err < 0)) {
        PLUGIN_ERROR("ioctl for NVME_IOCTL_ADMIN_CMD failed with %s\n", strerror(errno));
        close(fd);
        free(vid);
        return err;
    }

    int vendor_id = (int)le16_to_cpu(*vid);
    free(vid);
    close(fd);

    return vendor_id;
}

static int smart_read_nvme_disk(const char *dev, __attribute__((unused)) char const *name,
                                metric_family_t *fams, label_set_t *labels)
{
    union nvme_smart_log *smart_log = smart_alloc(sizeof(*smart_log));
    if (smart_log == NULL) {
        PLUGIN_ERROR("Failed to alloc union nvme_smart_log.");
        return -1;
    }

    int fd = open(dev, O_RDWR);
    if (unlikely(fd < 0)) {
        PLUGIN_ERROR("open failed with %s\n", strerror(errno));
        free(smart_log);
        return -1;
    }

    /**
     * Prepare Get Log Page command
     * Fill following fields (see NVMe 1.4 spec, section 5.14.1)
     * - Number of DWORDS (bits 27:16) - the struct that will be passed for
     *   filling has 512 bytes which gives 128 (0x80) DWORDS
     * - Log Page Indentifier (bits 7:0) - for SMART the id is 0x02
     */

    int status = ioctl(fd, NVME_IOCTL_ADMIN_CMD,
                       &(struct nvme_admin_cmd){.opcode = NVME_ADMIN_GET_LOG_PAGE,
                                                .nsid = NVME_NSID_ALL,
                                                .addr = (unsigned long)smart_log,
                                                .data_len = sizeof(*smart_log),
                                                .cdw10 = NVME_SMART_CDW10});
    if (unlikely(status < 0)) {
        PLUGIN_ERROR("ioctl for NVME_IOCTL_ADMIN_CMD failed with %s\n", strerror(errno));
        free(smart_log);
        close(fd);
        return status;
    }

    close(fd);

    value_t value = {0};

    value = VALUE_GAUGE((double)smart_log->data.critical_warning);
    metric_family_append(&fams[FAM_SMART_NVME_CRITICAL_WARNING], value, labels, NULL);

    value = VALUE_GAUGE(((double)(smart_log->data.temperature[1] << 8) +
                                    smart_log->data.temperature[0] - 273));
    metric_family_append(&fams[FAM_SMART_NVME_TEMPERATURE], value, labels, NULL);

    value = VALUE_GAUGE((double)smart_log->data.avail_spare);
    metric_family_append(&fams[FAM_SMART_NVME_AVAIL_SPARE], value, labels, NULL);

    value = VALUE_GAUGE((double)smart_log->data.spare_thresh);
    metric_family_append(&fams[FAM_SMART_NVME_AVAIL_SPARE_THRESH], value, labels, NULL);

    value = VALUE_GAUGE((double)smart_log->data.percent_used);
    metric_family_append(&fams[FAM_SMART_NVME_PERCENT_USED], value, labels, NULL);

    value = VALUE_GAUGE((double)smart_log->data.endu_grp_crit_warn_sumry);
    metric_family_append(&fams[FAM_SMART_NVME_ENDU_GRP_CRIT_WARN_SUMRY], value, labels, NULL);

    value = VALUE_GAUGE(int96_to_double(smart_log->data.data_units_read));
    metric_family_append(&fams[FAM_SMART_NVME_DATA_UNITS_READ], value, labels, NULL);

    value = VALUE_GAUGE(int96_to_double(smart_log->data.data_units_written));
    metric_family_append(&fams[FAM_SMART_NVME_DATA_UNITS_WRITTEN], value, labels, NULL);

    value = VALUE_GAUGE(int96_to_double(smart_log->data.host_commands_read));
    metric_family_append(&fams[FAM_SMART_NVME_HOST_COMMANDS_READ], value, labels, NULL);

    value = VALUE_GAUGE(int96_to_double(smart_log->data.host_commands_written));
    metric_family_append(&fams[FAM_SMART_NVME_HOST_COMMANDS_WRITTEN], value, labels, NULL);

    value = VALUE_GAUGE(int96_to_double(smart_log->data.ctrl_busy_time));
    metric_family_append(&fams[FAM_SMART_NVME_CTRL_BUSY_TIME], value, labels, NULL);

    value = VALUE_GAUGE(int96_to_double(smart_log->data.power_cycles));
    metric_family_append(&fams[FAM_SMART_NVME_POWER_CYCLES], value, labels, NULL);

    value = VALUE_GAUGE(int96_to_double(smart_log->data.power_on_hours));
    metric_family_append(&fams[FAM_SMART_NVME_POWER_ON_HOURS], value, labels, NULL);

    value = VALUE_GAUGE(int96_to_double(smart_log->data.unsafe_shutdowns));
    metric_family_append(&fams[FAM_SMART_NVME_UNSAFE_SHUTDOWNS], value, labels, NULL);

    value = VALUE_GAUGE(int96_to_double(smart_log->data.media_errors));
    metric_family_append(&fams[FAM_SMART_NVME_MEDIA_ERRORS], value, labels, NULL);

    value = VALUE_GAUGE(int96_to_double(smart_log->data.num_err_log_entries));
    metric_family_append(&fams[FAM_SMART_NVME_NUM_ERR_LOG_ENTRIES], value, labels, NULL);

    value = VALUE_GAUGE((double)smart_log->data.warning_temp_time);
    metric_family_append(&fams[FAM_SMART_NVME_WARNING_TEMP_TIME], value, labels, NULL);

    value = VALUE_GAUGE((double)smart_log->data.critical_comp_time);
    metric_family_append(&fams[FAM_SMART_NVME_CRITICAL_COMP_TIME], value, labels, NULL);

    const char *temp_sensor[] = { "1", "2", "3", "4", "5", "6", "7", "8" };
    for (size_t i = 0; i < 8; i++) {
        if (smart_log->data.temp_sensor[i] > 0) {
            value = VALUE_GAUGE((double)smart_log->data.temp_sensor[i] - 273);
            metric_family_append(&fams[FAM_SMART_NVME_TEMP_SENSOR], value, labels,
                                 &(label_pair_const_t){.name="sensor", .value=temp_sensor[i]},
                                 NULL);
        }
    }

    value = VALUE_GAUGE((double)smart_log->data.thm_temp1_trans_count);
    metric_family_append(&fams[FAM_SMART_NVME_THERMAL_MGMT_TEMP1_TRANSITION_COUNT],
                         value, labels, NULL);

    value = VALUE_GAUGE((double)smart_log->data.thm_temp1_total_time);
    metric_family_append(&fams[FAM_SMART_NVME_THERMAL_MGMT_TEMP1_TOTAL_TIME],
                         value, labels, NULL);

    value = VALUE_GAUGE((double)smart_log->data.thm_temp2_trans_count);
    metric_family_append(&fams[FAM_SMART_NVME_THERMAL_MGMT_TEMP2_TRANSITION_COUNT],
                         value, labels, NULL);

    value = VALUE_GAUGE((double)smart_log->data.thm_temp2_total_time);
    metric_family_append(&fams[FAM_SMART_NVME_THERMAL_MGMT_TEMP2_TOTAL_TIME],
                         value, labels, NULL);

    free(smart_log);
    return 0;
}

static int smart_read_nvme_intel_disk(const char *dev, __attribute__((unused)) char const *name,
                                      metric_family_t *fams, label_set_t *labels)
{
    struct nvme_additional_smart_log *intel_smart_log = smart_alloc(sizeof(*intel_smart_log));
    if (intel_smart_log == NULL) {
        PLUGIN_ERROR("Failed to alloc struct nvme_additional_smart_log.");
        return -1;
    }

    int fd = open(dev, O_RDWR);
    if (unlikely(fd < 0)) {
        PLUGIN_ERROR("open failed with %s\n", strerror(errno));
        free(intel_smart_log);
        return -1;
    }

    /**
     * Prepare Get Log Page command
     * - Additional SMART Attributes (Log Identfiter CAh)
     */
    int status = ioctl(fd, NVME_IOCTL_ADMIN_CMD,
                        &(struct nvme_admin_cmd){.opcode = NVME_ADMIN_GET_LOG_PAGE,
                                                 .nsid = NVME_NSID_ALL,
                                                 .addr = (unsigned long)intel_smart_log,
                                                 .data_len = sizeof(*intel_smart_log),
                                                 .cdw10 = NVME_SMART_INTEL_CDW10});
    if (unlikely(status < 0)) {
        PLUGIN_ERROR("ioctl for NVME_IOCTL_ADMIN_CMD failed with %s\n", strerror(errno));
        free(intel_smart_log);
        close(fd);
        return -1;
    }

    close(fd);

    value_t value = {0};

    value = VALUE_GAUGE((double)intel_smart_log->program_fail_cnt.norm);
    metric_family_append(&fams[FAM_SMART_NVME_PROGRAM_FAIL_COUNT_NORM], value, labels, NULL);

    value = VALUE_GAUGE(int48_to_double(intel_smart_log->program_fail_cnt.raw));
    metric_family_append(&fams[FAM_SMART_NVME_PROGRAM_FAIL_COUNT_RAW], value, labels, NULL);

    value = VALUE_GAUGE((double)intel_smart_log->erase_fail_cnt.norm);
    metric_family_append(&fams[FAM_SMART_NVME_ERASE_FAIL_COUNT_NORM], value, labels, NULL);

    value = VALUE_GAUGE(int48_to_double(intel_smart_log->program_fail_cnt.raw));
    metric_family_append(&fams[FAM_SMART_NVME_ERASE_FAIL_COUNT_RAW], value, labels, NULL);

    value = VALUE_GAUGE((double)intel_smart_log->wear_leveling_cnt.norm);
    metric_family_append(&fams[FAM_SMART_NVME_WEAR_LEVELING_NORM], value, labels, NULL);

    value = VALUE_GAUGE((double)le16_to_cpu(intel_smart_log->wear_leveling_cnt.wear_level.min));
    metric_family_append(&fams[FAM_SMART_NVME_WEAR_LEVELING_MIN], value, labels, NULL);

    value = VALUE_GAUGE((double)le16_to_cpu(intel_smart_log->wear_leveling_cnt.wear_level.max));
    metric_family_append(&fams[FAM_SMART_NVME_WEAR_LEVELING_MAX], value, labels, NULL);

    value = VALUE_GAUGE((double)le16_to_cpu(intel_smart_log->wear_leveling_cnt.wear_level.avg));
    metric_family_append(&fams[FAM_SMART_NVME_WEAR_LEVELING_AVG], value, labels, NULL);

    value = VALUE_GAUGE((double)intel_smart_log->e2e_err_cnt.norm);
    metric_family_append(&fams[FAM_SMART_NVME_END_TO_END_ERROR_DETECTION_COUNT_NORM], value, labels, NULL);

    value = VALUE_GAUGE(int48_to_double(intel_smart_log->e2e_err_cnt.raw));
    metric_family_append(&fams[FAM_SMART_NVME_END_TO_END_ERROR_DETECTION_COUNT_RAW], value, labels, NULL);

    value = VALUE_GAUGE((double)intel_smart_log->crc_err_cnt.norm);
    metric_family_append(&fams[FAM_SMART_NVME_CRC_ERROR_COUNT_NORM], value, labels, NULL);

    value = VALUE_GAUGE(int48_to_double(intel_smart_log->crc_err_cnt.raw));
    metric_family_append(&fams[FAM_SMART_NVME_CRC_ERROR_COUNT_RAW], value, labels, NULL);

    value = VALUE_GAUGE((double)intel_smart_log->timed_workload_media_wear.norm);
    metric_family_append(&fams[FAM_SMART_NVME_TIMED_WORKLOAD_MEDIA_WEAR_NORM], value, labels, NULL);

    value = VALUE_GAUGE(int48_to_double(intel_smart_log->timed_workload_media_wear.raw));
    metric_family_append(&fams[FAM_SMART_NVME_TIMED_WORKLOAD_MEDIA_WEAR_RAW], value, labels, NULL);

    value = VALUE_GAUGE((double)intel_smart_log->timed_workload_host_reads.norm);
    metric_family_append(&fams[FAM_SMART_NVME_TIMED_WORKLOAD_HOST_READS_NORM], value, labels, NULL);

    value = VALUE_GAUGE(int48_to_double(intel_smart_log->timed_workload_host_reads.raw));
    metric_family_append(&fams[FAM_SMART_NVME_TIMED_WORKLOAD_HOST_READS_RAW], value, labels, NULL);

    value = VALUE_GAUGE((double)intel_smart_log->timed_workload_timer.norm);
    metric_family_append(&fams[FAM_SMART_NVME_TIMED_WORKLOAD_TIMER_NORM], value, labels, NULL);

    value = VALUE_GAUGE(int48_to_double(intel_smart_log->timed_workload_timer.raw));
    metric_family_append(&fams[FAM_SMART_NVME_TIMED_WORKLOAD_TIMER_RAW], value, labels, NULL);

    value = VALUE_GAUGE((double)intel_smart_log->thermal_throttle_status.norm);
    metric_family_append(&fams[FAM_SMART_NVME_THERMAL_THROTTLE_STATUS_NORM], value, labels, NULL);

    value = VALUE_GAUGE((double)intel_smart_log->thermal_throttle_status.thermal_throttle.pct);
    metric_family_append(&fams[FAM_SMART_NVME_THERMAL_THROTTLE_STATUS_PCT], value, labels, NULL);

    value = VALUE_GAUGE((double)intel_smart_log->thermal_throttle_status.thermal_throttle.count);
    metric_family_append(&fams[FAM_SMART_NVME_THERMAL_THROTTLE_STATUS_COUNT], value, labels, NULL);

    value = VALUE_GAUGE((double)intel_smart_log->retry_buffer_overflow_cnt.norm);
    metric_family_append(&fams[FAM_SMART_NVME_RETRY_BUFFER_OVERFLOW_COUNT_NORM], value, labels, NULL);

    value = VALUE_GAUGE(int48_to_double(intel_smart_log->retry_buffer_overflow_cnt.raw));
    metric_family_append(&fams[FAM_SMART_NVME_RETRY_BUFFER_OVERFLOW_COUNT_RAW], value, labels, NULL);

    value = VALUE_GAUGE((double)intel_smart_log->pll_lock_loss_cnt.norm);
    metric_family_append(&fams[FAM_SMART_NVME_PLL_LOCK_LOSS_COUNT_NORM], value, labels, NULL);

    value = VALUE_GAUGE(int48_to_double(intel_smart_log->pll_lock_loss_cnt.raw));
    metric_family_append(&fams[FAM_SMART_NVME_PLL_LOCK_LOSS_COUNT_RAW], value, labels, NULL);

    value = VALUE_GAUGE((double)intel_smart_log->host_bytes_written.norm);
    metric_family_append(&fams[FAM_SMART_NVME_NAND_BYTES_WRITTEN_NORM], value, labels, NULL);

    value = VALUE_GAUGE(int48_to_double(intel_smart_log->host_bytes_written.raw));
    metric_family_append(&fams[FAM_SMART_NVME_NAND_BYTES_WRITTEN_RAW], value, labels, NULL);

    value = VALUE_GAUGE((double)intel_smart_log->host_bytes_written.norm);
    metric_family_append(&fams[FAM_SMART_NVME_HOST_BYTES_WRITTEN_NORM], value, labels, NULL);

    value = VALUE_GAUGE(int48_to_double(intel_smart_log->host_bytes_written.raw));
    metric_family_append(&fams[FAM_SMART_NVME_HOST_BYTES_WRITTEN_RAW], value, labels, NULL);

    free(intel_smart_log);

    return 0;
}

static void smart_read_sata_disk(SkDisk *d, char const *name,
                                 metric_family_t *fams, label_set_t *labels)
{
    SkBool available = FALSE;
    if (sk_disk_identify_is_available(d, &available) < 0 || !available) {
        PLUGIN_DEBUG("disk %s cannot be identified.", name);
        return;
    }
    if (sk_disk_smart_is_available(d, &available) < 0 || !available) {
        PLUGIN_DEBUG("disk %s has no SMART support.", name);
        return;
    }
    if (!ignore_sleep_mode) {
        SkBool awake = FALSE;
        if (sk_disk_check_sleep_mode(d, &awake) < 0 || !awake) {
            PLUGIN_DEBUG("disk %s is sleeping.", name);
            return;
        }
    }
    if (sk_disk_smart_read_data(d) < 0) {
        PLUGIN_ERROR("unable to get SMART data for disk %s.", name);
        return;
    }

    if (sk_disk_smart_parse(d, &(SkSmartParsedData const *){NULL}) < 0) {
        PLUGIN_ERROR("unable to parse SMART data for disk %s.", name);
        return;
    }

    /* Get some specific values */
    uint64_t value;
    if (sk_disk_smart_get_power_on(d, &value) >= 0) {
        metric_family_append(&fams[FAM_SMART_POWER_ON],
                             VALUE_GAUGE(((double)value) / 1000.0), labels, NULL);
    } else {
        PLUGIN_DEBUG("unable to get milliseconds since power on for %s.", name);
    }

    if (sk_disk_smart_get_power_cycle(d, &value) >= 0) {
        metric_family_append(&fams[FAM_SMART_POWER_CYCLES],
                             VALUE_GAUGE((double)value), labels, NULL);
    } else {
        PLUGIN_DEBUG("unable to get number of power cycles for %s.", name);
    }

    if (sk_disk_smart_get_bad(d, &value) >= 0) {
        metric_family_append(&fams[FAM_SMART_BAD_SECTORS],
                             VALUE_GAUGE((double)value), labels, NULL);
    } else {
        PLUGIN_DEBUG("unable to get number of bad sectors for %s.", name);
    }

    if (sk_disk_smart_get_temperature(d, &value) >= 0) {
        metric_family_append(&fams[FAM_SMART_TEMPERATURE],
                             VALUE_GAUGE(((double)value) / 1000. - 273.15),
                             labels, NULL);
    } else {
        PLUGIN_DEBUG("unable to get temperature for %s.", name);
    }

    /* Grab all attributes */
    smart_user_data_t ud = {
        .fams = fams,
        .labels = labels,
        .name = name,
    };
    if (sk_disk_smart_parse_attributes(d, handle_attribute, (void *)&ud) < 0) {
        PLUGIN_ERROR("unable to handle SMART attributes for %s.", name);
    }
}

static void smart_handle_disk(const char *dev, const char *serial, metric_family_t *fams)
{
    const char *name = NULL;
    if (use_serial && (serial != NULL)) {
        name = serial;
    } else {
        if (dev == NULL)
            return;
        name = strrchr(dev, '/');
        if (!name)
            return;
        name++;
    }

    if (use_serial) {
        if (!exclist_match(&excl_serial, name)) {
            PLUGIN_DEBUG("ignoring %s. Name = %s", dev, name);
            return;
        }
    } else {
        if (!exclist_match(&excl_disk, name)) {
            PLUGIN_DEBUG("ignoring %s. Name = %s", dev, name);
            return;
        }
    }

    PLUGIN_DEBUG("checking SMART status of %s.", dev);


    label_set_t labels = {0};
    label_set_add(&labels, true, "disk", name);
    if (serial != NULL)
        label_set_add(&labels, true, "serial", serial);

    if (strstr(dev, "nvme")) {
        int err = smart_read_nvme_disk(dev, name, fams, &labels);
        if (err < 0) {
            PLUGIN_ERROR("smart_read_nvme_disk failed, %d", err);
        } else {
            switch (get_vendor_id(dev, name)) {
            case INTEL_VENDOR_ID:
                err = smart_read_nvme_intel_disk(dev, name, fams, &labels);
                if (err < 0)
                    PLUGIN_ERROR("smart_read_nvme_intel_disk failed, %d", err);
                break;

            default:
                PLUGIN_DEBUG("No support vendor specific attributes");
                break;
            }
        }
    } else {
        SkDisk *d = NULL;
        if (sk_disk_open(dev, &d) < 0) {
            PLUGIN_ERROR("unable to open %s.", dev);
        } else {
            smart_read_sata_disk(d, name, fams, &labels);
            sk_disk_free(d);
        }
    }

    label_set_reset(&labels);
}

// __attribute__((stack_protect))
static int smart_read(void)
{
    /* Use udev to get a list of disks */
    struct udev *handle_udev = udev_new();
    if (handle_udev == NULL) {
        PLUGIN_ERROR("unable to initialize udev.");
        return -1;
    }

    struct udev_enumerate *enumerate = udev_enumerate_new(handle_udev);
    if (enumerate == NULL) {
        udev_unref(handle_udev);
        PLUGIN_ERROR("fail udev_enumerate_new");
        return -1;
    }

    udev_enumerate_add_match_subsystem(enumerate, "block");
    udev_enumerate_add_match_property(enumerate, "DEVTYPE", "disk");
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
    if (devices == NULL) {
        udev_enumerate_unref(enumerate);
        udev_unref(handle_udev);
        PLUGIN_ERROR("udev returned an empty list deviecs");
        return -1;
    }

    struct udev_list_entry *dev_list_entry;
    udev_list_entry_foreach(dev_list_entry, devices) {
        const char *path = udev_list_entry_get_name(dev_list_entry);
        if (path == NULL)
            continue;
        struct udev_device *dev = udev_device_new_from_syspath(handle_udev, path);
        if (dev == NULL)
            continue;
        const char *devpath = udev_device_get_devnode(dev);
        const char *serial = udev_device_get_property_value(dev, "ID_SERIAL_SHORT");

        /* Query status with libatasmart */
        smart_handle_disk(devpath, serial, fams_smart);

        udev_device_unref(dev);
    }

    udev_enumerate_unref(enumerate);
    udev_unref(handle_udev);

    plugin_dispatch_metric_family_array(fams_smart, FAM_SMART_MAX, 0);
    return 0;
}

static int smart_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "disk") == 0) {
            status = cf_util_exclist(child, &excl_disk);
        } else if (strcasecmp(child->key, "ignore-sleep-mode") == 0) {
            status = cf_util_get_boolean(child, &ignore_sleep_mode);
        } else if (strcasecmp(child->key, "use-serial") == 0) {
            status = cf_util_get_boolean(child, &use_serial);
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

static int smart_init(void)
{
    if (use_serial) {
        int err = create_ignorelist_by_serial();
        if (err != 0) {
            PLUGIN_ERROR("Enable to create ignorelist_by_serial");
            return 1;
        }
    }

    pagesize = getpagesize();

#if defined(HAVE_SYS_CAPABILITY_H) && defined(CAP_SYS_RAWIO)
    if (plugin_check_capability(CAP_SYS_RAWIO) != 0) {
        if (getuid() == 0)
            PLUGIN_WARNING("Running ncollectd as root, but the CAP_SYS_RAWIO capability is missing. "
                           "The plugin's read function will probably fail. "
                           "Is your init system dropping capabilities?");
        else
            PLUGIN_WARNING("ncollectd doesn't have the CAP_SYS_RAWIO capability. "
                           "If you don't want to run ncollectd as root, try "
                           "running 'setcap cap_sys_rawio=ep' on the ncollectd binary.");
    }
#endif
    return 0;
}

static int smart_shutdown(void)
{
    exclist_reset(&excl_disk);
    exclist_reset(&excl_serial);
    return 0;
}

void module_register(void)
{
    plugin_register_config("smart", smart_config);
    plugin_register_init("smart", smart_init);
    plugin_register_read("smart", smart_read);
    plugin_register_shutdown("smart", smart_shutdown);
}
