// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

enum {
    FAM_FCHOST_PORT_ONLINE,
    FAM_FCHOST_TX_FRAMES,
    FAM_FCHOST_TX_BYTES,
    FAM_FCHOST_RX_FRAMES,
    FAM_FCHOST_RX_BYTES,
    FAM_FCHOST_LIP,
    FAM_FCHOST_NOS,
    FAM_FCHOST_ERROR_FRAMES,
    FAM_FCHOST_DUMPED_FRAMES,
    FAM_FCHOST_LINK_FAILURE,
    FAM_FCHOST_LOSS_OF_SYNC,
    FAM_FCHOST_LOSS_OF_SIGNAL,
    FAM_FCHOST_PRIMITIVE_SEQUENCE_PROTOCOL_ERROR,
    FAM_FCHOST_INVALID_TX_WORD,
    FAM_FCHOST_INVALID_CRC,
    FAM_FCHOST_FCP_INPUT_REQUESTS,
    FAM_FCHOST_FCP_OUTPUT_REQUESTS,
    FAM_FCHOST_FCP_CONTROL_REQUESTS,
    FAM_FCHOST_FCP_INPUT_BYTES,
    FAM_FCHOST_FCP_OUTPUT_BYTES,
    FAM_FCHOST_MAX,
};

static metric_family_t fams[FAM_FCHOST_MAX] = {
    [FAM_FCHOST_PORT_ONLINE] = {
        .name = "system_fchost_port_online",
        .type = METRIC_TYPE_GAUGE,
        .help = "If the port status is online.",
    },
    [FAM_FCHOST_TX_FRAMES] = {
        .name = "system_fchost_tx_frames",
        .type = METRIC_TYPE_COUNTER,
        .help = "Transmitted FC frames.",
    },
    [FAM_FCHOST_TX_BYTES] = {
        .name = "system_fchost_tx_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Transmitted FC bytes.",
    },
    [FAM_FCHOST_RX_FRAMES] = {
        .name = "system_fchost_rx_frames",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received FC frames."
    },
    [FAM_FCHOST_RX_BYTES] = {
        .name = "system_fchost_rx_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received FC bytes.",
    },
    [FAM_FCHOST_LIP] = {
        .name = "system_fchost_lip",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of LIP sequences.",
    },
    [FAM_FCHOST_NOS] = {
        .name = "system_fchost_nos",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of NOS sequences.",
    },
    [FAM_FCHOST_ERROR_FRAMES] = {
        .name = "system_fchost_error_frames",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of frames that are received in error.",
    },
    [FAM_FCHOST_DUMPED_FRAMES] = {
        .name = "system_fchost_dumped_frames",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of frames that are lost because of lack of host resources.",
    },
    [FAM_FCHOST_LINK_FAILURE] = {
        .name = "system_fchost_link_failure",
        .type = METRIC_TYPE_COUNTER,
        .help = "Link failure count.",
    },
    [FAM_FCHOST_LOSS_OF_SYNC] = {
        .name = "system_fchost_loss_of_sync",
        .type = METRIC_TYPE_COUNTER,
        .help = "Loss of synchronization count.",
    },
    [FAM_FCHOST_LOSS_OF_SIGNAL] = {
        .name = "system_fchost_loss_of_signal",
        .type = METRIC_TYPE_COUNTER,
        .help = "Loss of signal count.",
    },
    [FAM_FCHOST_PRIMITIVE_SEQUENCE_PROTOCOL_ERROR] = {
        .name = "system_fchost_primitive_sequence_protocol_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Primitive sequence protocol error count.",
    },
    [FAM_FCHOST_INVALID_TX_WORD] = {
        .name = "system_fchost_invalid_tx_word",
        .type = METRIC_TYPE_COUNTER,
        .help = "Invalid transmission word count.",
    },
    [FAM_FCHOST_INVALID_CRC] = {
        .name = "system_fchost_invalid_crc",
        .type = METRIC_TYPE_COUNTER,
        .help = "Invalid CRC count.",
    },
    [FAM_FCHOST_FCP_INPUT_REQUESTS] = {
        .name = "system_fchost_fcp_input_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of FCP operations with data input.",
    },
    [FAM_FCHOST_FCP_OUTPUT_REQUESTS] = {
        .name = "system_fchost_fcp_output_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of FCP operations with data output.",
    },
    [FAM_FCHOST_FCP_CONTROL_REQUESTS] = {
        .name = "system_fchost_fcp_control_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of FCP operations without data movement.",
    },
    [FAM_FCHOST_FCP_INPUT_BYTES] = {
        .name = "system_fchost_fcp_input_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Bytes of FCP data input.",
    },
    [FAM_FCHOST_FCP_OUTPUT_BYTES] = {
        .name = "system_fchost_fcp_output_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Bytes of FCP data output.",
    }
};

typedef struct {
    char *file;
    int shift;
    int fam;
} fchost_stats_t;

static fchost_stats_t fchost_stats[] = {
    { "statistics/tx_frames",                    0, FAM_FCHOST_TX_FRAMES                         },
    { "statistics/tx_words",                     2, FAM_FCHOST_TX_BYTES                          },
    { "statistics/rx_frames",                    0, FAM_FCHOST_RX_FRAMES                         },
    { "statistics/rx_words",                     2, FAM_FCHOST_RX_BYTES                          },
    { "statistics/lip_count",                    0, FAM_FCHOST_LIP                               },
    { "statistics/nos_count",                    0, FAM_FCHOST_NOS                               },
    { "statistics/error_frames",                 0, FAM_FCHOST_ERROR_FRAMES                      },
    { "statistics/dumped_frames",                0, FAM_FCHOST_DUMPED_FRAMES                     },
    { "statistics/link_failure_count",           0, FAM_FCHOST_LINK_FAILURE                      },
    { "statistics/loss_of_sync_count",           0, FAM_FCHOST_LOSS_OF_SYNC                      },
    { "statistics/loss_of_signal_count",         0, FAM_FCHOST_LOSS_OF_SIGNAL                    },
    { "statistics/prim_seq_protocol_err_count",  0, FAM_FCHOST_PRIMITIVE_SEQUENCE_PROTOCOL_ERROR },
    { "statistics/invalid_tx_word_count",        0, FAM_FCHOST_INVALID_TX_WORD                   },
    { "statistics/invalid_crc_count",            0, FAM_FCHOST_INVALID_CRC                       },
    { "statistics/fcp_input_requests",           0, FAM_FCHOST_FCP_INPUT_REQUESTS                },
    { "statistics/fcp_output_requests",          0, FAM_FCHOST_FCP_OUTPUT_REQUESTS               },
    { "statistics/fcp_control_requests",         0, FAM_FCHOST_FCP_CONTROL_REQUESTS              },
    { "statistics/fcp_input_megabytes",         10, FAM_FCHOST_FCP_INPUT_BYTES                   },
    { "statistics/fcp_output_megabytes",        10, FAM_FCHOST_FCP_OUTPUT_BYTES                  },
};

static size_t fchost_stats_size = STATIC_ARRAY_SIZE(fchost_stats);

static char *path_sys_fchost = NULL;

static int fchost_read_hosts(int dir_fd, const char *path, const char *filename,
                             __attribute__((unused)) void *ud)
{
    struct stat statbuf;
    int status = fstatat(dir_fd, filename, &statbuf, 0);
    if (status != 0) {
        PLUGIN_ERROR("stat (%s) in %s failed: %s.", filename, path, STRERRNO);
        return -1;
    }

    if (S_ISDIR(statbuf.st_mode)) {
        int dir_host_fd = openat(dir_fd, filename, O_RDONLY | O_DIRECTORY);
        if (dir_host_fd < 0) {
            PLUGIN_ERROR("open (%s) in %s failed: %s.", filename, path, STRERRNO);
            return -1;
        }

        char port_name[256];
        status = read_file_at(dir_host_fd, "port_name", port_name, sizeof(port_name));
        if (status != 0) {
            PLUGIN_ERROR("open 'port_name' in %s failed: %s.", path, STRERRNO);
            return -1;
        }

        double fchost_port_state = 0;
        char port_state[64];
        status = read_file_at(dir_host_fd, "port_state", port_state, sizeof(port_state));
        if (status != 0) {
            PLUGIN_ERROR("open 'port_state' in %s failed: %s.", path, STRERRNO);
        } else {
            if (strcasecmp(port_state, "Online") == 0)
                fchost_port_state = 1;
        }

        metric_family_append(&fams[FAM_FCHOST_PORT_ONLINE], VALUE_GAUGE(fchost_port_state), NULL,
                             &(label_pair_const_t){.name="host", .value=filename},
                             &(label_pair_const_t){.name="port_name", .value=port_name},
                             NULL);

        for (size_t i = 0; i < fchost_stats_size; i++) {
            fchost_stats_t *fcs = &fchost_stats[i];
            uint64_t value = 0;
            ssize_t fstatus = filetouint_at(dir_host_fd, fcs->file, &value);
            if ((fstatus == 0) && (value != UINT64_MAX)) {
                if (fcs->shift > 0)
                    value <<= fcs->shift;

                metric_family_append(&fams[fcs->fam], VALUE_COUNTER(value), NULL,
                                     &(label_pair_const_t){.name="host", .value=filename},
                                     &(label_pair_const_t){.name="port_name", .value=port_name},
                                     NULL);
            }
        }

        close(dir_host_fd);
    }

    return 0;
}

static int fchost_read(void)
{
    int status = walk_directory(path_sys_fchost, fchost_read_hosts, NULL, 0);

    plugin_dispatch_metric_family_array(fams, FAM_FCHOST_MAX, 0);

    if (status != 0)
        return -1;

    return 0;
}

static int fchost_init(void)
{
    path_sys_fchost = plugin_syspath("class/fc_host");
    if (path_sys_fchost == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    return 0;
}

static int fchost_shutdown(void)
{
    free(path_sys_fchost);
    return 0;
}

void module_register(void)
{
    plugin_register_init("fchost", fchost_init);
    plugin_register_read("fchost", fchost_read);
    plugin_register_shutdown("fchost", fchost_shutdown);
}
