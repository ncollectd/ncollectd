/* SPDX-License-Identifier: GPL-2.0-only OR MIT                                   */
/* SPDX-FileCopyrightText: Copyright (C) 2020 Intel Corporation.                  */
/* SPDX-FileContributor: Bartlomiej Kotlowski <bartlomiej.kotlowski at intel.com> */

#pragma once

#include <linux/types.h>

#ifdef __CHECKER__
#define __force __attribute__((force))
#else
#define __force
#endif

#define INTEL_VENDOR_ID 0x8086

struct __attribute__((packed)) nvme_additional_smart_log_item {
    __u8 key;
    __u8 _kp[2];
    __u8 norm;
    __u8 _np;
    union __attribute__((packed)) {
        __u8 raw[6];
        struct __attribute__((packed)) wear_level {
            __le16 min;
            __le16 max;
            __le16 avg;
        } wear_level;
        struct __attribute__((packed)) thermal_throttle {
            __u8 pct;
            __u32 count;
        } thermal_throttle;
    };
    __u8 _rp;
};

struct nvme_additional_smart_log {
    struct nvme_additional_smart_log_item program_fail_cnt;
    struct nvme_additional_smart_log_item erase_fail_cnt;
    struct nvme_additional_smart_log_item wear_leveling_cnt;
    struct nvme_additional_smart_log_item e2e_err_cnt;
    struct nvme_additional_smart_log_item crc_err_cnt;
    struct nvme_additional_smart_log_item timed_workload_media_wear;
    struct nvme_additional_smart_log_item timed_workload_host_reads;
    struct nvme_additional_smart_log_item timed_workload_timer;
    struct nvme_additional_smart_log_item thermal_throttle_status;
    struct nvme_additional_smart_log_item retry_buffer_overflow_cnt;
    struct nvme_additional_smart_log_item pll_lock_loss_cnt;
    struct nvme_additional_smart_log_item nand_bytes_written;
    struct nvme_additional_smart_log_item host_bytes_written;

    struct nvme_additional_smart_log_item host_ctx_wear_used;
    struct nvme_additional_smart_log_item perf_stat_indicator;
    struct nvme_additional_smart_log_item re_alloc_sectr_cnt;
    struct nvme_additional_smart_log_item soft_ecc_err_rate;
    struct nvme_additional_smart_log_item unexp_power_loss;
    struct nvme_additional_smart_log_item media_bytes_read;
    struct nvme_additional_smart_log_item avail_fw_downgrades;
};

#define NVME_SMART_INTEL_CDW10 ((((sizeof(struct nvme_additional_smart_log) >> 2) - 1) << 16) | 0x000000ca)

