/**
 * collectd - src/smart.c
 * Copyright (C) 2014       Vincent Bernat
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Vincent Bernat <vbe at exoscale.ch>
 *   Maciej Fijalkowski <maciej.fijalkowski@intel.com>
 *   Bartlomiej Kotlowski <bartlomiej.kotlowski@intel.com>
 *   Slawomir Strehlau <slawomir.strehlau@intel.com>
 **/

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"
#include "utils/ignorelist/ignorelist.h"

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
  metric_t *tmpl;
} smart_user_data_t;

static const char *config_keys[] = {"Disk", "IgnoreSelected", "IgnoreSleepMode",
                                    "UseSerial"};

static int config_keys_num = STATIC_ARRAY_SIZE(config_keys);

static ignorelist_t *ignorelist, *ignorelist_by_serial;
static int ignore_sleep_mode;
static int use_serial;
static int invert_ignorelist;

static int smart_config(const char *key, const char *value)
{
  if (ignorelist == NULL)
    ignorelist = ignorelist_create(/* invert = */ 1);
  if (ignorelist == NULL)
    return 1;

  if (strcasecmp("Disk", key) == 0) {
    ignorelist_add(ignorelist, value);
  } else if (strcasecmp("IgnoreSelected", key) == 0) {
    invert_ignorelist = 1;
    if (IS_TRUE(value))
      invert_ignorelist = 0;
    ignorelist_set_invert(ignorelist, invert_ignorelist);
  } else if (strcasecmp("IgnoreSleepMode", key) == 0) {
    if (IS_TRUE(value))
      ignore_sleep_mode = 1;
  } else if (strcasecmp("UseSerial", key) == 0) {
    if (IS_TRUE(value))
      use_serial = 1;
  } else {
    return -1;
  }

  return 0;
}

static int create_ignorelist_by_serial(ignorelist_t *il)
{
  struct udev *handle_udev;
  struct udev_enumerate *enumerate;
  struct udev_list_entry *devices, *dev_list_entry;
  struct udev_device *dev;

  if (ignorelist_by_serial == NULL)
    ignorelist_by_serial = ignorelist_create(invert_ignorelist);
  if (ignorelist_by_serial == NULL)
    return 1;

  if (invert_ignorelist == 0) {
    ignorelist_set_invert(ignorelist, 1);
  }

  // Use udev to get a list of disks
  handle_udev = udev_new();
  if (!handle_udev) {
    ERROR("smart plugin: unable to initialize udev.");
    return 1;
  }
  enumerate = udev_enumerate_new(handle_udev);
  if (enumerate == NULL) {
    ERROR("fail udev_enumerate_new");
    return 1;
  }
  udev_enumerate_add_match_subsystem(enumerate, "block");
  udev_enumerate_add_match_property(enumerate, "DEVTYPE", "disk");
  udev_enumerate_scan_devices(enumerate);
  devices = udev_enumerate_get_list_entry(enumerate);
  if (devices == NULL) {
    ERROR("udev returned an empty list deviecs");
    return 1;
  }
  udev_list_entry_foreach(dev_list_entry, devices) {
    const char *path, *devpath, *serial, *name;
    path = udev_list_entry_get_name(dev_list_entry);
    dev = udev_device_new_from_syspath(handle_udev, path);
    devpath = udev_device_get_devnode(dev);
    serial = udev_device_get_property_value(dev, "ID_SERIAL_SHORT");
    name = strrchr(devpath, '/');
    if (name != NULL) {
      if (name[0] == '/')
        name++;

      if (ignorelist_match(ignorelist, name) == 0 && serial != NULL) {
        ignorelist_add(ignorelist_by_serial, serial);
      }
    }
  }

  if (invert_ignorelist == 0) {
    ignorelist_set_invert(ignorelist, 1);
  }
  return 0;
}

static void handle_attribute(SkDisk *d, const SkSmartAttributeParsedData *a, void *userdata)
{
  smart_user_data_t *ud = userdata;

  if (!a->current_value_valid || !a->worst_value_valid)
    return;

  char id[24];
  ssnprintf(id, sizeof(id), "%u", (unsigned int) a->id);
  metric_label_set(ud->tmpl, "attribute_id", id);

  metric_family_append(&ud->fams[FAM_SMART_ATTRIBUTE_CURRENT], "attribute", a->name,
                       (value_t){.gauge.float64 = a->current_value } , ud->tmpl);
  metric_family_append(&ud->fams[FAM_SMART_ATTRIBUTE_PRETTY], "attribute", a->name,
                       (value_t){.gauge.float64 = a->pretty_value } , ud->tmpl);
  metric_family_append(&ud->fams[FAM_SMART_ATTRIBUTE_THRESHOLD], "attribute", a->name,
                       (value_t){.gauge.float64 = a->threshold_valid ? a->threshold : 0 } , ud->tmpl);
  metric_family_append(&ud->fams[FAM_SMART_ATTRIBUTE_WORST], "attribute", a->name,
                       (value_t){.gauge.float64 = a->worst_value } , ud->tmpl);
  
  metric_label_set(ud->tmpl, "attribute_id", NULL);

  if (a->threshold_valid && a->current_value <= a->threshold) {
    notification_t n = {
      .severity = NOTIF_WARNING,
      .time = cdtime(),
      .name = "smart_attribute",
    };
    
    notification_label_set(&n, "device", ud->name);
    notification_label_set(&n, "attribute", a->name);

    char message[1024];
    ssnprintf(message, sizeof(message), "attribute %s is below allowed threshold (%d < %d)", a->name, a->current_value, a->threshold);
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
  double add = 0;

  for (int i = 0; i < 16; i++) {
    add = data[15 - i];
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
  double add = 0;

  for (int i = 0; i < 6; i++) {
    add = data[5 - i];
    for (int j = i + 1; j < 6; j++) {
      add *= SHIFT_BYTE_LEFT;
    }
    sum += add;
  }
  return sum;
}

static int get_vendor_id(const char *dev, char const *name)
{
  int fd, err;
  __le16 vid;

  fd = open(dev, O_RDWR);
  if (fd < 0) {
    ERROR("open failed with %s\n", strerror(errno));
    return fd;
  }

  err = ioctl(fd, NVME_IOCTL_ADMIN_CMD,
              &(struct nvme_admin_cmd){.opcode = NVME_ADMIN_IDENTIFY,
                                       .nsid = NVME_NSID_ALL,
                                       .addr = (unsigned long)&vid,
                                       .data_len = sizeof(vid),
                                       .cdw10 = 1,
                                       .cdw11 = 0});

  if (err < 0) {
    ERROR("ioctl for NVME_IOCTL_ADMIN_CMD failed with %s\n", strerror(errno));
    close(fd);
    return err;
  }

  close(fd);
  return (int)le16_to_cpu(vid);
}

static int smart_read_nvme_disk(const char *dev, char const *name, metric_family_t *fams, metric_t *tmpl)
{
  union nvme_smart_log smart_log = {};
  int fd, status;

  fd = open(dev, O_RDWR);
  if (fd < 0) {
    ERROR("open failed with %s\n", strerror(errno));
    return fd;
  }

  /**
   * Prepare Get Log Page command
   * Fill following fields (see NVMe 1.4 spec, section 5.14.1)
   * - Number of DWORDS (bits 27:16) - the struct that will be passed for
   *   filling has 512 bytes which gives 128 (0x80) DWORDS
   * - Log Page Indentifier (bits 7:0) - for SMART the id is 0x02
   */

  status = ioctl(fd, NVME_IOCTL_ADMIN_CMD,
                 &(struct nvme_admin_cmd){.opcode = NVME_ADMIN_GET_LOG_PAGE,
                                          .nsid = NVME_NSID_ALL,
                                          .addr = (unsigned long)&smart_log,
                                          .data_len = sizeof(smart_log),
                                          .cdw10 = NVME_SMART_CDW10});
  if (status < 0) {
    ERROR("ioctl for NVME_IOCTL_ADMIN_CMD failed with %s\n", strerror(errno));
    close(fd);
    return status;
  }

  value_t value = {0};

  value.gauge.float64 = (double)smart_log.data.critical_warning;
  metric_family_append(&fams[FAM_SMART_NVME_CRITICAL_WARNING], NULL, NULL, value, tmpl);                  

  value.gauge.float64 = ((double)(smart_log.data.temperature[1] << 8) + smart_log.data.temperature[0] - 273);
  metric_family_append(&fams[FAM_SMART_NVME_TEMPERATURE], NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)smart_log.data.avail_spare;
  metric_family_append(&fams[FAM_SMART_NVME_AVAIL_SPARE], NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)smart_log.data.spare_thresh;
  metric_family_append(&fams[FAM_SMART_NVME_AVAIL_SPARE_THRESH], NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)smart_log.data.percent_used;
  metric_family_append(&fams[FAM_SMART_NVME_PERCENT_USED], NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)smart_log.data.endu_grp_crit_warn_sumry;
  metric_family_append(&fams[FAM_SMART_NVME_ENDU_GRP_CRIT_WARN_SUMRY], NULL, NULL, value, tmpl);

  value.gauge.float64 = int96_to_double(smart_log.data.data_units_read);
  metric_family_append(&fams[FAM_SMART_NVME_DATA_UNITS_READ], NULL, NULL, value, tmpl);

  value.gauge.float64 = int96_to_double(smart_log.data.data_units_written);
  metric_family_append(&fams[FAM_SMART_NVME_DATA_UNITS_WRITTEN], NULL, NULL, value, tmpl);

  value.gauge.float64 = int96_to_double(smart_log.data.host_commands_read);
  metric_family_append(&fams[FAM_SMART_NVME_HOST_COMMANDS_READ], NULL, NULL, value, tmpl);

  value.gauge.float64 = int96_to_double(smart_log.data.host_commands_written);
  metric_family_append(&fams[FAM_SMART_NVME_HOST_COMMANDS_WRITTEN], NULL, NULL, value, tmpl);

  value.gauge.float64 = int96_to_double(smart_log.data.ctrl_busy_time);
  metric_family_append(&fams[FAM_SMART_NVME_CTRL_BUSY_TIME], NULL, NULL, value, tmpl);

  value.gauge.float64 = int96_to_double(smart_log.data.power_cycles);
  metric_family_append(&fams[FAM_SMART_NVME_POWER_CYCLES], NULL, NULL, value, tmpl);

  value.gauge.float64 = int96_to_double(smart_log.data.power_on_hours);
  metric_family_append(&fams[FAM_SMART_NVME_POWER_ON_HOURS], NULL, NULL, value, tmpl);

  value.gauge.float64 = int96_to_double(smart_log.data.unsafe_shutdowns);
  metric_family_append(&fams[FAM_SMART_NVME_UNSAFE_SHUTDOWNS], NULL, NULL, value, tmpl);

  value.gauge.float64 = int96_to_double(smart_log.data.media_errors);
  metric_family_append(&fams[FAM_SMART_NVME_MEDIA_ERRORS], NULL, NULL, value, tmpl);

  value.gauge.float64 = int96_to_double(smart_log.data.num_err_log_entries);
  metric_family_append(&fams[FAM_SMART_NVME_NUM_ERR_LOG_ENTRIES], NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)smart_log.data.warning_temp_time;
  metric_family_append(&fams[FAM_SMART_NVME_WARNING_TEMP_TIME], NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)smart_log.data.critical_comp_time;
  metric_family_append(&fams[FAM_SMART_NVME_CRITICAL_COMP_TIME], NULL, NULL, value, tmpl);

  if (smart_log.data.temp_sensor[0] > 0) {
    value.gauge.float64 = (double)smart_log.data.temp_sensor[0] - 273;  
    metric_family_append(&fams[FAM_SMART_NVME_TEMP_SENSOR], "sensor", "1", value, tmpl);
  }
  if (smart_log.data.temp_sensor[1] > 0) {
    value.gauge.float64 = (double)smart_log.data.temp_sensor[1] - 273;  
    metric_family_append(&fams[FAM_SMART_NVME_TEMP_SENSOR], "sensor", "2", value, tmpl);
  }
  if (smart_log.data.temp_sensor[2] > 0) {
    value.gauge.float64 = (double)smart_log.data.temp_sensor[2] - 273;  
    metric_family_append(&fams[FAM_SMART_NVME_TEMP_SENSOR], "sensor", "3", value, tmpl);
  }
  if (smart_log.data.temp_sensor[3] > 0) {
    value.gauge.float64 = (double)smart_log.data.temp_sensor[3] - 273;  
    metric_family_append(&fams[FAM_SMART_NVME_TEMP_SENSOR], "sensor", "4", value, tmpl);
  }
  if (smart_log.data.temp_sensor[4] > 0) {
    value.gauge.float64 = (double)smart_log.data.temp_sensor[4] - 273;  
    metric_family_append(&fams[FAM_SMART_NVME_TEMP_SENSOR], "sensor", "5", value, tmpl);
  }
  if (smart_log.data.temp_sensor[5] > 0) {
    value.gauge.float64 = (double)smart_log.data.temp_sensor[5] - 273;  
    metric_family_append(&fams[FAM_SMART_NVME_TEMP_SENSOR], "sensor", "6", value, tmpl);
  }
  if (smart_log.data.temp_sensor[6] > 0) {
    value.gauge.float64 = (double)smart_log.data.temp_sensor[6] - 273;  
    metric_family_append(&fams[FAM_SMART_NVME_TEMP_SENSOR], "sensor", "7", value, tmpl);
  }
  if (smart_log.data.temp_sensor[7] > 0) {
    value.gauge.float64 = (double)smart_log.data.temp_sensor[7] - 273;  
    metric_family_append(&fams[FAM_SMART_NVME_TEMP_SENSOR], "sensor", "8", value, tmpl);
  }

  value.gauge.float64 = (double)smart_log.data.thm_temp1_trans_count; 
  metric_family_append(&fams[FAM_SMART_NVME_THERMAL_MGMT_TEMP1_TRANSITION_COUNT], NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)smart_log.data.thm_temp1_total_time;  
  metric_family_append(&fams[FAM_SMART_NVME_THERMAL_MGMT_TEMP1_TOTAL_TIME], NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)smart_log.data.thm_temp2_trans_count; 
  metric_family_append(&fams[FAM_SMART_NVME_THERMAL_MGMT_TEMP2_TRANSITION_COUNT], NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)smart_log.data.thm_temp2_total_time;  
  metric_family_append(&fams[FAM_SMART_NVME_THERMAL_MGMT_TEMP2_TOTAL_TIME], NULL, NULL, value, tmpl);


  close(fd);
  return 0;
}

static int smart_read_nvme_intel_disk(const char *dev, char const *name, metric_family_t *fams, metric_t *tmpl)
{
  DEBUG("name = %s", name);
  DEBUG("dev = %s", dev);

  struct nvme_additional_smart_log intel_smart_log;
  int fd, status;
  fd = open(dev, O_RDWR);
  if (fd < 0) {
    ERROR("open failed with %s\n", strerror(errno));
    return fd;
  }

  /**
   * Prepare Get Log Page command
   * - Additional SMART Attributes (Log Identfiter CAh)
   */

  status = ioctl(fd, NVME_IOCTL_ADMIN_CMD,
            &(struct nvme_admin_cmd){.opcode = NVME_ADMIN_GET_LOG_PAGE,
                                     .nsid = NVME_NSID_ALL,
                                     .addr = (unsigned long)&intel_smart_log,
                                     .data_len = sizeof(intel_smart_log),
                                     .cdw10 = NVME_SMART_INTEL_CDW10});
  if (status < 0) {
    ERROR("ioctl for NVME_IOCTL_ADMIN_CMD failed with %s\n", strerror(errno));
    close(fd);
    return status;
  }

  value_t value = {0};

  value.gauge.float64 = (double)intel_smart_log.program_fail_cnt.norm;
  metric_family_append(&fams[FAM_SMART_NVME_PROGRAM_FAIL_COUNT_NORM], NULL, NULL, value, tmpl);

  value.gauge.float64 = int48_to_double(intel_smart_log.program_fail_cnt.raw);
  metric_family_append(&fams[FAM_SMART_NVME_PROGRAM_FAIL_COUNT_RAW], NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)intel_smart_log.erase_fail_cnt.norm;
  metric_family_append(&fams[FAM_SMART_NVME_ERASE_FAIL_COUNT_NORM], NULL, NULL, value, tmpl);

  value.gauge.float64 = int48_to_double(intel_smart_log.program_fail_cnt.raw);
  metric_family_append(&fams[FAM_SMART_NVME_ERASE_FAIL_COUNT_RAW],  NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)intel_smart_log.wear_leveling_cnt.norm;
  metric_family_append(&fams[FAM_SMART_NVME_WEAR_LEVELING_NORM], NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)le16_to_cpu(intel_smart_log.wear_leveling_cnt.wear_level.min);
  metric_family_append(&fams[FAM_SMART_NVME_WEAR_LEVELING_MIN], NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)le16_to_cpu(intel_smart_log.wear_leveling_cnt.wear_level.max);
  metric_family_append(&fams[FAM_SMART_NVME_WEAR_LEVELING_MAX], NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)le16_to_cpu(intel_smart_log.wear_leveling_cnt.wear_level.avg);
  metric_family_append(&fams[FAM_SMART_NVME_WEAR_LEVELING_AVG], NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)intel_smart_log.e2e_err_cnt.norm;
  metric_family_append(&fams[FAM_SMART_NVME_END_TO_END_ERROR_DETECTION_COUNT_NORM], NULL, NULL, value, tmpl);

  value.gauge.float64 = int48_to_double(intel_smart_log.e2e_err_cnt.raw);
  metric_family_append(&fams[FAM_SMART_NVME_END_TO_END_ERROR_DETECTION_COUNT_RAW], NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)intel_smart_log.crc_err_cnt.norm;
  metric_family_append(&fams[FAM_SMART_NVME_CRC_ERROR_COUNT_NORM], NULL, NULL, value, tmpl);

  value.gauge.float64 = int48_to_double(intel_smart_log.crc_err_cnt.raw);
  metric_family_append(&fams[FAM_SMART_NVME_CRC_ERROR_COUNT_RAW], NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)intel_smart_log.timed_workload_media_wear.norm;
  metric_family_append(&fams[FAM_SMART_NVME_TIMED_WORKLOAD_MEDIA_WEAR_NORM],  NULL, NULL, value, tmpl);

  value.gauge.float64 = int48_to_double(intel_smart_log.timed_workload_media_wear.raw);
  metric_family_append(&fams[FAM_SMART_NVME_TIMED_WORKLOAD_MEDIA_WEAR_RAW],  NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)intel_smart_log.timed_workload_host_reads.norm;
  metric_family_append(&fams[FAM_SMART_NVME_TIMED_WORKLOAD_HOST_READS_NORM], NULL, NULL, value, tmpl);

  value.gauge.float64 = int48_to_double(intel_smart_log.timed_workload_host_reads.raw);
  metric_family_append(&fams[FAM_SMART_NVME_TIMED_WORKLOAD_HOST_READS_RAW], NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)intel_smart_log.timed_workload_timer.norm;
  metric_family_append(&fams[FAM_SMART_NVME_TIMED_WORKLOAD_TIMER_NORM], NULL, NULL, value, tmpl);

  value.gauge.float64 = int48_to_double(intel_smart_log.timed_workload_timer.raw);
  metric_family_append(&fams[FAM_SMART_NVME_TIMED_WORKLOAD_TIMER_RAW], NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)intel_smart_log.thermal_throttle_status.norm;
  metric_family_append(&fams[FAM_SMART_NVME_THERMAL_THROTTLE_STATUS_NORM], NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)intel_smart_log.thermal_throttle_status.thermal_throttle.pct;
  metric_family_append(&fams[FAM_SMART_NVME_THERMAL_THROTTLE_STATUS_PCT], NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)intel_smart_log.thermal_throttle_status.thermal_throttle.count;
  metric_family_append(&fams[FAM_SMART_NVME_THERMAL_THROTTLE_STATUS_COUNT], NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)intel_smart_log.retry_buffer_overflow_cnt.norm;
  metric_family_append(&fams[FAM_SMART_NVME_RETRY_BUFFER_OVERFLOW_COUNT_NORM], NULL, NULL, value, tmpl);

  value.gauge.float64 = int48_to_double(intel_smart_log.retry_buffer_overflow_cnt.raw);
  metric_family_append(&fams[FAM_SMART_NVME_RETRY_BUFFER_OVERFLOW_COUNT_RAW], NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)intel_smart_log.pll_lock_loss_cnt.norm;
  metric_family_append(&fams[FAM_SMART_NVME_PLL_LOCK_LOSS_COUNT_NORM], NULL, NULL, value, tmpl);

  value.gauge.float64 = int48_to_double(intel_smart_log.pll_lock_loss_cnt.raw);
  metric_family_append(&fams[FAM_SMART_NVME_PLL_LOCK_LOSS_COUNT_RAW],  NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)intel_smart_log.host_bytes_written.norm;
  metric_family_append(&fams[FAM_SMART_NVME_NAND_BYTES_WRITTEN_NORM], NULL, NULL, value, tmpl);

  value.gauge.float64 = int48_to_double(intel_smart_log.host_bytes_written.raw);
  metric_family_append(&fams[FAM_SMART_NVME_NAND_BYTES_WRITTEN_RAW], NULL, NULL, value, tmpl);

  value.gauge.float64 = (double)intel_smart_log.host_bytes_written.norm;
  metric_family_append(&fams[FAM_SMART_NVME_HOST_BYTES_WRITTEN_NORM], NULL, NULL, value, tmpl);

  value.gauge.float64 = int48_to_double(intel_smart_log.host_bytes_written.raw);
  metric_family_append(&fams[FAM_SMART_NVME_HOST_BYTES_WRITTEN_RAW], NULL, NULL, value, tmpl);

  close(fd);
  return 0;
}

static void smart_read_sata_disk(SkDisk *d, char const *name, metric_family_t *fams, metric_t *tmpl)
{
  SkBool available = FALSE;
  if (sk_disk_identify_is_available(d, &available) < 0 || !available) {
    DEBUG("smart plugin: disk %s cannot be identified.", name);
    return;
  }
  if (sk_disk_smart_is_available(d, &available) < 0 || !available) {
    DEBUG("smart plugin: disk %s has no SMART support.", name);
    return;
  }
  if (!ignore_sleep_mode) {
    SkBool awake = FALSE;
    if (sk_disk_check_sleep_mode(d, &awake) < 0 || !awake) {
      DEBUG("smart plugin: disk %s is sleeping.", name);
      return;
    }
  }
  if (sk_disk_smart_read_data(d) < 0) {
    ERROR("smart plugin: unable to get SMART data for disk %s.", name);
    return;
  }

  if (sk_disk_smart_parse(d, &(SkSmartParsedData const *){NULL}) < 0) {
    ERROR("smart plugin: unable to parse SMART data for disk %s.", name);
    return;
  }

  /* Get some specific values */
  uint64_t value;
  if (sk_disk_smart_get_power_on(d, &value) >= 0)
    metric_family_append(&fams[FAM_SMART_POWER_ON], NULL, NULL,
                         (value_t){.gauge.float64 = ((double)value) / 1000. } , tmpl);
  else
    DEBUG("smart plugin: unable to get milliseconds since power on for %s.", name);

  if (sk_disk_smart_get_power_cycle(d, &value) >= 0) {
    metric_family_append(&fams[FAM_SMART_POWER_CYCLES], NULL, NULL, 
                         (value_t){.gauge.float64 = (double)value } , tmpl);
  } else
    DEBUG("smart plugin: unable to get number of power cycles for %s.", name);

  if (sk_disk_smart_get_bad(d, &value) >= 0)
    metric_family_append(&fams[FAM_SMART_BAD_SECTORS], NULL, NULL,
                         (value_t){.gauge.float64 = (double)value } , tmpl);
  else
    DEBUG("smart plugin: unable to get number of bad sectors for %s.", name);

  if (sk_disk_smart_get_temperature(d, &value) >= 0)
    metric_family_append(&fams[FAM_SMART_TEMPERATURE], NULL, NULL,
                         (value_t){.gauge.float64 = ((double)value) / 1000. - 273.15 } , tmpl);
  else
    DEBUG("smart plugin: unable to get temperature for %s.", name);

  /* Grab all attributes */
  smart_user_data_t ud = {
    .fams = fams,
    .tmpl = tmpl,
    .name = name,
  };
  if (sk_disk_smart_parse_attributes(d, handle_attribute, (void *)&ud) < 0) {
    ERROR("smart plugin: unable to handle SMART attributes for %s.", name);
  }
}

static void smart_handle_disk(const char *dev, const char *serial, metric_family_t *fams)
{
  SkDisk *d = NULL;
  const char *name;
  int err;

  if (use_serial && serial) {
    name = serial;
  } else {
    name = strrchr(dev, '/');
    if (!name)
      return;
    name++;
  }

  if (use_serial) {
    if (ignorelist_match(ignorelist_by_serial, name) != 0) {
      DEBUG("smart plugin: ignoring %s. Name = %s", dev, name);
      return;
    }
  } else {
    if (ignorelist_match(ignorelist, name) != 0) {
      DEBUG("smart plugin: ignoring %s. Name = %s", dev, name);
      return;
    }
  }

  DEBUG("smart plugin: checking SMART status of %s.", dev);

  metric_t tmpl = {0};
  metric_label_set(&tmpl, "disk", name);
  if (serial != NULL)
    metric_label_set(&tmpl, "serial", serial);

  if (strstr(dev, "nvme")) {
    err = smart_read_nvme_disk(dev, name, fams, &tmpl);
    if (err) {
      ERROR("smart plugin: smart_read_nvme_disk failed, %d", err);
    } else {
      switch (get_vendor_id(dev, name)) {
      case INTEL_VENDOR_ID:
        err = smart_read_nvme_intel_disk(dev, name, fams, &tmpl);
        if (err) {
          ERROR("smart plugin: smart_read_nvme_intel_disk failed, %d", err);
        }
        break;

      default:
        DEBUG("No support vendor specific attributes");
        break;
      }
    }
  } else {
    if (sk_disk_open(dev, &d) < 0) {
      ERROR("smart plugin: unable to open %s.", dev);
    } else {
      smart_read_sata_disk(d, name, fams, &tmpl);
      sk_disk_free(d);
    }
  }

  metric_reset(&tmpl);
}

static int smart_read(void)
{
  struct udev *handle_udev;
  struct udev_enumerate *enumerate;
  struct udev_list_entry *devices, *dev_list_entry;
  struct udev_device *dev;

  /* Use udev to get a list of disks */
  handle_udev = udev_new();
  if (!handle_udev) {
    ERROR("smart plugin: unable to initialize udev.");
    return -1;
  }
  enumerate = udev_enumerate_new(handle_udev);
  if (enumerate == NULL) {
    ERROR("fail udev_enumerate_new");
    return -1;
  }
  udev_enumerate_add_match_subsystem(enumerate, "block");
  udev_enumerate_add_match_property(enumerate, "DEVTYPE", "disk");
  udev_enumerate_scan_devices(enumerate);
  devices = udev_enumerate_get_list_entry(enumerate);
  if (devices == NULL) {
    ERROR("udev returned an empty list deviecs");
    return -1;
  }
 
  udev_list_entry_foreach(dev_list_entry, devices) {
    const char *path, *devpath, *serial;
    path = udev_list_entry_get_name(dev_list_entry);
    dev = udev_device_new_from_syspath(handle_udev, path);
    devpath = udev_device_get_devnode(dev);
    serial = udev_device_get_property_value(dev, "ID_SERIAL_SHORT");

    /* Query status with libatasmart */
    smart_handle_disk(devpath, serial, fams_smart);
    udev_device_unref(dev);
  }

  udev_enumerate_unref(enumerate);
  udev_unref(handle_udev);

  for (size_t i = 0; i < FAM_SMART_MAX; i++) {
    if (fams_smart[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&fams_smart[i]);
      if (status != 0) {
        ERROR("smart plugin: plugin_dispatch_metric_family failed: %s",
              STRERROR(status));
      }
      metric_family_metric_reset(&fams_smart[i]);
    }
  }

  return 0;
}

static int smart_init(void)
{
  int err;
  if (use_serial) {
    err = create_ignorelist_by_serial(ignorelist);
    if (err != 0) {
      ERROR("Enable to create ignorelist_by_serial");
      return 1;
    }
  }

#if defined(HAVE_SYS_CAPABILITY_H) && defined(CAP_SYS_RAWIO)
  if (check_capability(CAP_SYS_RAWIO) != 0) {
    if (getuid() == 0)
      WARNING("smart plugin: Running collectd as root, but the "
              "CAP_SYS_RAWIO capability is missing. The plugin's read "
              "function will probably fail. Is your init system dropping "
              "capabilities?");
    else
      WARNING("smart plugin: collectd doesn't have the CAP_SYS_RAWIO "
              "capability. If you don't want to run collectd as root, try "
              "running \"setcap cap_sys_rawio=ep\" on the collectd binary.");
  }
#endif
  return 0;
}

void module_register(void)
{
  plugin_register_config("smart", smart_config, config_keys, config_keys_num);
  plugin_register_init("smart", smart_init);
  plugin_register_read("smart", smart_read);
}
