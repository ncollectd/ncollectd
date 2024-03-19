// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright(c) 2019 Intel Corporation.
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Hari TG <hari.tg at intel.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/itoa.h"

#include <nvm_types.h>
#include <nvm_management.h>

enum {
    FAM_DCPMM_HEALTH_STATUS,
    FAM_DCPMM_LIFESPAN_REMAINING_RATIO,
    FAM_DCPMM_LIFESPAN_USED_RATIO,
    FAM_DCPMM_POWER_ON_TIME_SECONDS,
    FAM_DCPMM_UPTIME_SECONDS,
    FAM_DCPMM_LAST_SHUTDOWN_TIME_SECONDS,
    FAM_DCPMM_MEDIA_TEMPERATURE_CELSIUS,
    FAM_DCPMM_CONTROLLER_TEMPERATURE_CELSIUS,
    FAM_DCPMM_MEDIA_MAX_TEMPERATURE_CELSIUS,
    FAM_DCPMM_CONTROLLER_MAX_TEMPERATURE_CELSIUS,
    FAM_DCPMM_READ_BYTES,
    FAM_DCPMM_WRITTEN_BYTES,
    FAM_DCPMM_READ_64B_OPS,
    FAM_DCPMM_WRITE_64B_OPS,
    FAM_DCPMM_MEDIA_READ_OPS,
    FAM_DCPMM_MEDIA_WRITE_OPS,
    FAM_DCPMM_HOST_READS,
    FAM_DCPMM_HOST_WRITES,
    FAM_DCPMM_READ_HIT_RATIO,
    FAM_DCPMM_WRITE_HIT_RATIO,
    FAM_DCPMM_MAX,
};

static metric_family_t fams[FAM_DCPMM_MAX] = {
    [FAM_DCPMM_HEALTH_STATUS] = {
        .name = "system_dcpmm_health_status",
        .type = METRIC_TYPE_STATE_SET,
        .help = "Overall health summary (0: normal | 1: non-critical | 2: critical | 3: fatal).",
    },
    [FAM_DCPMM_LIFESPAN_REMAINING_RATIO] = {
        .name = "system_dcpmm_lifespan_remaining_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "The module’s remaining life as a percentage value of factory expected life span.",
    },
    [FAM_DCPMM_LIFESPAN_USED_RATIO] = {
        .name = "system_dcpmm_lifespan_used_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "The module’s used life as a percentage value of factory expected life span.",
    },
    [FAM_DCPMM_POWER_ON_TIME_SECONDS] = {
        .name = "system_dcpmm_power_on_time_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "The lifetime the DIMM has been powered on in seconds.",
    },
    [FAM_DCPMM_UPTIME_SECONDS] = {
        .name = "system_dcpmm_uptime_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current uptime of the DIMM for the current power cycle in seconds.",
    },
    [FAM_DCPMM_LAST_SHUTDOWN_TIME_SECONDS] = {
        .name = "system_dcpmm_last_shutdown_time_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "The time the system was last shutdown. The time is represented in epoch (seconds).",
    },
    [FAM_DCPMM_MEDIA_TEMPERATURE_CELSIUS] = {
        .name = "system_dcpmm_media_temperature_celsius",
        .type = METRIC_TYPE_GAUGE,
        .help = "The media’s current temperature in degree Celsius.",
    },
    [FAM_DCPMM_CONTROLLER_TEMPERATURE_CELSIUS] = {
        .name = "system_dcpmm_controller_temperature_celsius",
        .type = METRIC_TYPE_GAUGE,
        .help = "The controller’s current temperature in degree Celsius.",
    },
    [FAM_DCPMM_MEDIA_MAX_TEMPERATURE_CELSIUS] =  {
        .name = "system_dcpmm_media_max_temperature_celsius",
        .type = METRIC_TYPE_GAUGE,
        .help = "The media’s the highest temperature reported in degree Celsius.",
    },
    [FAM_DCPMM_CONTROLLER_MAX_TEMPERATURE_CELSIUS] = {
        .name = "system_dcpmm_controller_max_temperature_celsius",
        .type = METRIC_TYPE_GAUGE,
        .help = "The controller’s highest temperature reported in degree Celsius.",
    },
    [FAM_DCPMM_READ_BYTES] = {
        .name = "system_dcpmm_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of bytes transacted by the read operations.",
    },
    [FAM_DCPMM_WRITTEN_BYTES] = {
        .name = "system_dcpmm_written_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of bytes transacted by the write operations.",
    },
    [FAM_DCPMM_READ_64B_OPS] = {
        .name = "system_dcpmm_read_64B_ops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of read operations performed to the physical media in 64 bytes granularity."
    },
    [FAM_DCPMM_WRITE_64B_OPS] = {
        .name = "system_dcpmm_write_64B_ops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of write operations performed to the physical media in 64 bytes granularity."
    },
    [FAM_DCPMM_MEDIA_READ_OPS] = {
        .name = "system_dcpmm_media_read_ops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of read operations performed to the physical media."
    },
    [FAM_DCPMM_MEDIA_WRITE_OPS] = {
        .name = "system_dcpmm_media_write_ops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of write operations performed to the physical media."
    },
    [FAM_DCPMM_HOST_READS] = {
        .name = "system_dcpmm_host_reads",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of read operations received from the CPU (memory controller)."
    },
    [FAM_DCPMM_HOST_WRITES] = {
        .name = "system_dcpmm_host_writes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of write operations received from the CPU (memory controller)."
    },
    [FAM_DCPMM_READ_HIT_RATIO] = {
        .name = "system_dcpmm_read_hit_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "Measures the efficiency of the buffer in the read path. Range of 0.0 - 1.0."
    },
    [FAM_DCPMM_WRITE_HIT_RATIO] = {
        .name = "system_dcpmm_write_hit_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "Measures the efficiency of the buffer in the write path. Range of 0.0 - 1.0."
    },
};

#define HEALTH_INFO_VENDOR_SPECIFIC_DATA_SIZE 11
#define HEALTH_STATUS_FATAL 2
#define TEMP_VALUE_MASK     0x7FFF
#define TEMP_SIGN_BIT_INDEX 15
#define CELCIUS_CONV_VAL    0.0625

enum passthrough_opcode {
    PT_GET_LOG = 0x08
};

enum get_log_subop {
    SUBOP_SMART_HEALTH = 0x00,
    SUBOP_MEM_INFO     = 0x03
};

typedef struct __attribute__((packed)) {
    unsigned char total_bytes_read[16];    // Number of 64 byte reads from the DIMM
    unsigned char total_bytes_written[16]; // Number of 64 byte writes from the DIMM
    unsigned char total_read_reqs[16];     // Number of DDRT read transactions the DIMM has serviced
    unsigned char total_write_reqs[16];    // Number of DDRT write transactions the DIMM has serviced
    unsigned char rsvd[64];
} memory_info_page_node_t;

typedef struct __attribute__((packed)) {
    unsigned char validation_flags[4];
    unsigned char rsvd1[4];
    unsigned char health_status;
    unsigned char percentage_remaining;
    unsigned char percentage_used;
    unsigned char rsvd2;
    unsigned char media_temp[2];
    unsigned char controller_temp[2];
    unsigned char rsvd3[16];
    unsigned char vendor_data_size[4];
    unsigned char rsvd4[8];
    unsigned char power_on_time[8];
    unsigned char uptime[8];
    unsigned char rsvd5[5];
    unsigned char last_shutdown_time[8];
    unsigned char rsvd6[9];
    unsigned char max_media_temp[2];
    unsigned char max_controller_temp[2];
    unsigned char rsvd7[42];
} health_info_page_node_t;

typedef struct __attribute__((packed)) {
    unsigned char memory_page; // The page of memory information you want to retrieve
    unsigned char rsvd[127];
} payload_input_memory_info_t;

typedef struct {
    uint64_t bytes_read;
    uint64_t bytes_written;
    uint64_t host_reads;
    uint64_t host_writes;

    double media_temp;
    double controller_temp;

    uint64_t vendor_data_size;
    uint64_t power_on_time;
    uint64_t uptime;
    uint64_t last_shutdown_time;
    uint64_t max_media_temp;
    uint64_t max_controller_temp;
    uint64_t health_status;
    uint64_t percentage_remaining;
    uint64_t percentage_used;
} device_stats_t;

typedef struct {
    uint64_t bytes_read;
    uint64_t bytes_written;
    uint64_t host_reads;
    uint64_t host_writes;
    uint64_t total_bytes_read;
    uint64_t total_bytes_written;
    uint64_t media_read;
    uint64_t media_write;
    uint64_t poll_count;
} device_io_stats_t;

static unsigned int devices_count;
static struct device_discovery *devices;
static device_io_stats_t *devices_io_stats;

static int device_cmp_topology(const void *a, const void *b)
{
    const struct device_discovery *da = a;
    const struct device_discovery *db = b;

    return da->device_handle.handle - db->device_handle.handle;
}

static uint64_t a8toint(unsigned char arr[8])
{
    return ((unsigned long long)(arr[7] & 0xFF) << 56) +
           ((unsigned long long)(arr[6] & 0xFF) << 48) +
           ((unsigned long long)(arr[5] & 0xFF) << 40) +
           ((unsigned long long)(arr[4] & 0xFF) << 32) +
           ((unsigned long long)(arr[3] & 0xFF) << 24) +
           ((unsigned long long)(arr[2] & 0xFF) << 16) +
           ((unsigned long long)(arr[1] & 0xFF) << 8) +
            (unsigned long long)(arr[0] & 0xFF);
}

static uint64_t a4toint(unsigned char arr[4])
{
    return ((unsigned long long)(arr[3] & 0xFF) << 24) +
           ((unsigned long long)(arr[2] & 0xFF) << 16) +
           ((unsigned long long)(arr[1] & 0xFF) << 8) +
            (unsigned long long)(arr[0] & 0xFF);
}

static uint64_t a2toint(unsigned char arr[2])
{
    return ((unsigned long long)(arr[1] & 0xFF) << 8) +
            (unsigned long long)(arr[0] & 0xFF);
}


static int dcpmm_device_stats(const NVM_UID device_uid, device_stats_t *stats)
{

    memory_info_page_node_t mip = {0};
    payload_input_memory_info_t minput = {.memory_page = 1};
    struct device_pt_cmd mcmd = {
        .opcode              = PT_GET_LOG,
        .sub_opcode          = SUBOP_MEM_INFO,
        .input_payload_size  = sizeof(minput),
        .input_payload       = &minput,
        .output_payload_size =  sizeof(mip),
        .output_payload      = &mip,
    };
    int status = nvm_send_device_passthrough_cmd(device_uid, &mcmd);
    if (status != NVM_SUCCESS) {
        PLUGIN_ERROR("nvm_send_device_passthrough_cmd PT_GET_LOG SUBOP_MEM_INFO failed.");
        return -1;
    }

    stats->bytes_read = a8toint(mip.total_bytes_read);
    stats->bytes_written = a8toint(mip.total_bytes_written);
    stats->host_reads = a8toint(mip.total_read_reqs);
    stats->host_writes = a8toint(mip.total_write_reqs);

    health_info_page_node_t hip = {0};
    payload_input_memory_info_t hinput = {0};
    struct device_pt_cmd hcmd = {
        .opcode              = PT_GET_LOG,
        .sub_opcode          = SUBOP_SMART_HEALTH,
        .input_payload_size  = sizeof(hinput),
        .input_payload       = &hinput,
        .output_payload_size = sizeof(hip),
        .output_payload      = &hip,
    };
//  gp_DIMM_devices[dimm_index].uid
    status = nvm_send_device_passthrough_cmd(device_uid, &hcmd);
    if (status != NVM_SUCCESS) {
        PLUGIN_ERROR("nvm_send_device_passthrough_cmd PT_GET_LOG SUBOP_SMART_HEALTH failed.");
        return -1;
    }

    uint64_t validation_flags = a4toint(hip.validation_flags);

    uint64_t media_temp = a2toint(hip.media_temp);
    stats->media_temp = media_temp;
    uint64_t controller_temp = a2toint(hip.controller_temp);
    stats->controller_temp = controller_temp;

    stats->max_media_temp = a2toint(hip.max_media_temp);
    stats->max_controller_temp = a2toint(hip.max_controller_temp);

    stats->vendor_data_size = a4toint(hip.vendor_data_size);
    stats->power_on_time = a8toint(hip.power_on_time);
    stats->uptime = a8toint(hip.uptime);
    stats->last_shutdown_time = a8toint(hip.last_shutdown_time);

    stats->health_status = hip.health_status;
    stats->percentage_remaining = hip.percentage_remaining;
    stats->percentage_used = hip.percentage_used;

    for (size_t i = 0; i <= HEALTH_INFO_VENDOR_SPECIFIC_DATA_SIZE; i++) {
        if (!((validation_flags >> i) & 0x1))
            continue;

        switch (i) {
        case 0: { // Health Status
            int health = 0;
            for (int n = 0; n <= HEALTH_STATUS_FATAL; n++) {
                if ((stats->health_status >> n) & 0x1)
                     health = n + 1;
            }
            stats->health_status = health;
        }   break;
        case 1: // Percentage Remaining
            stats->percentage_used = 100 - stats->percentage_remaining;
            break;
        case 3: {// Media Temperature
            double tmp_media_temp = (media_temp & TEMP_VALUE_MASK) * CELCIUS_CONV_VAL;
            if ((media_temp >> TEMP_SIGN_BIT_INDEX) & 0x1)
                tmp_media_temp = -tmp_media_temp;
            stats->media_temp = tmp_media_temp;
        }   break;
        case 4: {// Controller Temperature
            double tmp_controller_temp = (controller_temp & TEMP_VALUE_MASK) * CELCIUS_CONV_VAL;
            if ((controller_temp >> TEMP_SIGN_BIT_INDEX) & 0x1)
                tmp_controller_temp = -tmp_controller_temp;
            stats->controller_temp = tmp_controller_temp;
        }   break;
        case 11:
            if (stats->vendor_data_size > 0) {
                double tmp_media_temp = (media_temp & TEMP_VALUE_MASK) * CELCIUS_CONV_VAL;
                if ((media_temp >> TEMP_SIGN_BIT_INDEX) & 0x1)
                    tmp_media_temp = -tmp_media_temp;
                stats->media_temp = tmp_media_temp;
                double tmp_controller_temp = (controller_temp & TEMP_VALUE_MASK) * CELCIUS_CONV_VAL;
                if ((controller_temp >> TEMP_SIGN_BIT_INDEX) & 0x1)
                    tmp_controller_temp = -tmp_controller_temp;
                stats->controller_temp = tmp_controller_temp;
            } else {
                stats->power_on_time = 0;
                stats->uptime = 0;
                stats->last_shutdown_time = 0;
            }
            break;
        }
    }

    return 0;
}

static uint64_t calc_diff(uint64_t now, uint64_t prev)
{
    if (now < prev)
        return (0xFFFFFFFFFFFFFFFFULL - prev) + 1 + now;
    return now - prev;
}

static int dcpmm_read(void)
{
    for (size_t i  = 0; i < devices_count; i++) {

        device_stats_t stats = {0};
        int status = dcpmm_device_stats(devices[i].uid, &stats);
        if (status != 0)
            continue;

        char num[ITOA_MAX];
        uitoa(i, num);

        state_t states[] = {
            { .name = "normal",       .enabled = false },
            { .name = "non-critical", .enabled = false },
            { .name = "critical",     .enabled = false },
            { .name = "fatal",        .enabled = false },
        };
        state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };
        if (stats.health_status < 4)
            states[stats.health_status].enabled = true;

        metric_family_append(&fams[FAM_DCPMM_HEALTH_STATUS], VALUE_STATE_SET(set), NULL,
                             &(label_pair_const_t){.name="dimm", .value=num}, NULL);

        metric_family_append(&fams[FAM_DCPMM_LIFESPAN_REMAINING_RATIO],
                             VALUE_GAUGE((double)stats.percentage_remaining / 100.0), NULL,
                             &(label_pair_const_t){.name="dimm", .value=num}, NULL);
        metric_family_append(&fams[FAM_DCPMM_LIFESPAN_USED_RATIO],
                             VALUE_GAUGE((double)stats.percentage_used / 100.0), NULL,
                             &(label_pair_const_t){.name="dimm", .value=num}, NULL);
        metric_family_append(&fams[FAM_DCPMM_POWER_ON_TIME_SECONDS],
                             VALUE_GAUGE(stats.power_on_time), NULL,
                             &(label_pair_const_t){.name="dimm", .value=num}, NULL);
        metric_family_append(&fams[FAM_DCPMM_UPTIME_SECONDS],
                             VALUE_GAUGE(stats.uptime), NULL,
                             &(label_pair_const_t){.name="dimm", .value=num}, NULL);
        metric_family_append(&fams[FAM_DCPMM_LAST_SHUTDOWN_TIME_SECONDS],
                             VALUE_GAUGE(stats.last_shutdown_time), NULL,
                             &(label_pair_const_t){.name="dimm", .value=num}, NULL);
        metric_family_append(&fams[FAM_DCPMM_MEDIA_TEMPERATURE_CELSIUS],
                             VALUE_GAUGE(stats.media_temp), NULL,
                             &(label_pair_const_t){.name="dimm", .value=num}, NULL);
        metric_family_append(&fams[FAM_DCPMM_CONTROLLER_TEMPERATURE_CELSIUS],
                             VALUE_GAUGE(stats.controller_temp), NULL,
                             &(label_pair_const_t){.name="dimm", .value=num}, NULL);
        metric_family_append(&fams[FAM_DCPMM_MEDIA_MAX_TEMPERATURE_CELSIUS],
                             VALUE_GAUGE(stats.max_media_temp), NULL,
                             &(label_pair_const_t){.name="dimm", .value=num}, NULL);
        metric_family_append(&fams[FAM_DCPMM_CONTROLLER_MAX_TEMPERATURE_CELSIUS],
                             VALUE_GAUGE(stats.max_controller_temp), NULL,
                             &(label_pair_const_t){.name="dimm", .value=num}, NULL);



        if (devices_io_stats[i].poll_count > 0) {
            uint64_t diff_bytes_read = calc_diff(stats.bytes_read,
                                                 devices_io_stats[i].bytes_read);
            uint64_t diff_bytes_written = calc_diff(stats.bytes_written,
                                                    devices_io_stats[i].bytes_written);
            uint64_t diff_host_reads = calc_diff(stats.host_reads,
                                                 devices_io_stats[i].host_reads);
            uint64_t diff_host_writes = calc_diff(stats.host_writes,
                                                  devices_io_stats[i].host_writes);

            uint64_t total_bytes_read = 0ULL;
            uint64_t media_read = 0ULL;
            if (diff_bytes_read > diff_bytes_written) {
                total_bytes_read = (diff_bytes_read - diff_bytes_written) * 64;
                media_read       = (diff_bytes_read - diff_bytes_written) / 4;
            }

            uint64_t total_bytes_written = diff_bytes_written * 64;
            uint64_t media_write = diff_bytes_written / 4;

            double read_hit_ratio = 0.0;
            if (diff_host_reads > media_read) {
                read_hit_ratio = (double) (diff_host_reads - media_read) /
                                 (double) diff_host_reads;
            }

            double write_hit_ratio = 0.0;
            if (diff_host_writes > media_write) {
                write_hit_ratio = (double) (diff_host_writes - media_write) /
                                  (double) diff_host_writes;
            }

            devices_io_stats[i].total_bytes_read += total_bytes_read;
            devices_io_stats[i].total_bytes_written += total_bytes_written;
            devices_io_stats[i].media_read += media_read;
            devices_io_stats[i].media_write += media_write;

            metric_family_append(&fams[FAM_DCPMM_READ_BYTES],
                                 VALUE_COUNTER(devices_io_stats[i].total_bytes_read), NULL,
                                 &(label_pair_const_t){.name="dimm", .value=num}, NULL);
            metric_family_append(&fams[FAM_DCPMM_WRITTEN_BYTES],
                                 VALUE_COUNTER(devices_io_stats[i].total_bytes_read), NULL,
                                 &(label_pair_const_t){.name="dimm", .value=num}, NULL);
            metric_family_append(&fams[FAM_DCPMM_READ_64B_OPS],
                                 VALUE_COUNTER(stats.bytes_read), NULL,
                                 &(label_pair_const_t){.name="dimm", .value=num}, NULL);
            metric_family_append(&fams[FAM_DCPMM_WRITE_64B_OPS],
                                 VALUE_COUNTER(stats.bytes_written), NULL,
                                 &(label_pair_const_t){.name="dimm", .value=num}, NULL);
            metric_family_append(&fams[FAM_DCPMM_MEDIA_READ_OPS],
                                 VALUE_COUNTER(devices_io_stats[i].media_read), NULL,
                                 &(label_pair_const_t){.name="dimm", .value=num}, NULL);
            metric_family_append(&fams[FAM_DCPMM_MEDIA_WRITE_OPS],
                                 VALUE_COUNTER(devices_io_stats[i].media_write), NULL,
                                 &(label_pair_const_t){.name="dimm", .value=num}, NULL);
            metric_family_append(&fams[FAM_DCPMM_HOST_READS],
                                 VALUE_COUNTER(stats.host_reads), NULL,
                                 &(label_pair_const_t){.name="dimm", .value=num}, NULL);
            metric_family_append(&fams[FAM_DCPMM_HOST_WRITES],
                                 VALUE_COUNTER(stats.host_writes), NULL,
                                 &(label_pair_const_t){.name="dimm", .value=num}, NULL);
            metric_family_append(&fams[FAM_DCPMM_READ_HIT_RATIO],
                                 VALUE_GAUGE(read_hit_ratio), NULL,
                                 &(label_pair_const_t){.name="dimm", .value=num}, NULL);
            metric_family_append(&fams[FAM_DCPMM_WRITE_HIT_RATIO],
                                 VALUE_GAUGE(write_hit_ratio), NULL,
                                 &(label_pair_const_t){.name="dimm", .value=num}, NULL);


        }

        devices_io_stats[i].poll_count++;
        devices_io_stats[i].bytes_read = stats.bytes_read;
        devices_io_stats[i].bytes_written = stats.bytes_written;
        devices_io_stats[i].host_reads = stats.host_reads;
        devices_io_stats[i].host_writes = stats.host_writes;
    }

    plugin_dispatch_metric_family_array(fams, FAM_DCPMM_MAX, 0);

    return 0;
}

static int dcpmm_init(void)
{
    int status = nvm_get_number_of_devices(&devices_count);
    if (status != NVM_SUCCESS) {
        PLUGIN_ERROR("Obtaining the number of Intel Optane DIMMs failed!");
        return -1;
    }

    if (devices_count == 0) {
        PLUGIN_INFO("Intel Optane DIMMS are not available on this system.");
        return 0;
    }

    devices_io_stats = calloc(devices_count, sizeof(*devices_io_stats));
    if (devices_io_stats == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    devices = calloc(devices_count, sizeof(*devices));
    if (devices == NULL) {
        PLUGIN_ERROR("calloc failed.");
        free(devices_io_stats);
        return -1;
    }

    status = nvm_get_devices(devices, devices_count);
    if (status != NVM_SUCCESS) {
        PLUGIN_ERROR("nvm_get_devices failed");
        free(devices_io_stats);
        free(devices);
        return -1;
    }

    qsort(devices, devices_count, sizeof(*devices), device_cmp_topology);

    return 0;
}

static int dcpmm_shutdown(void)
{
    free(devices_io_stats);
    free(devices);
    return 0;
}

void module_register(void)
{
    plugin_register_init("dcpmm", dcpmm_init);
    plugin_register_read("dcpmm", dcpmm_read);
    plugin_register_shutdown("dcpmm", dcpmm_shutdown);
}
