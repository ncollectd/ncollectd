// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright 2020 NVIDIA Corporation
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Luke Yeager <lyeager at nvidia.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#include <ctype.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

enum {
    FAM_IB_STATE,
    FAM_IB_PHYSICAL_STATE,
    FAM_IB_RATE,
    FAM_IB_CAPABILITIES_MASK,
    FAM_IB_LOCAL_IDENTIFIER,
    FAM_IB_LOCAL_IDENTIFIER_MASK_COUNT,
    FAM_IB_SUBNET_MANAGER_LOCAL_IDENTIFIER,
    FAM_IB_SUBNET_MANAGER_SERVICE_LEVEL,
    FAM_IB_PORT_RCV_DATA,
    FAM_IB_PORT_XMIT_DATA,
    FAM_IB_PORT_RCV_PACKETS,
    FAM_IB_PORT_XMIT_PACKETS,
    FAM_IB_PORT_RCV_ERRORS,
    FAM_IB_PORT_RCV_REMOTE_PHYSICAL_ERRORS,
    FAM_IB_PORT_RCV_SWITCH_RELAY_ERRORS,
    FAM_IB_PORT_XMIT_DISCARDS,
    FAM_IB_PORT_RCV_CONSTRAINT_ERRORS,
    FAM_IB_PORT_XMIT_CONSTRAINT_ERRORS,
    FAM_IB_VL15_DROPPED,
    FAM_IB_LINK_ERROR_RECOVERY,
    FAM_IB_LINK_DOWNED,
    FAM_IB_SYMBOL_ERROR,
    FAM_IB_LOCAL_LINK_INTEGRITY_ERRORS,
    FAM_IB_EXCESSIVE_BUFFER_OVERRUN_ERRORS,
    FAM_IB_PORT_XMIT_WAIT,
    FAM_IB_UNICAST_RCV_PACKETS,
    FAM_IB_UNICAST_XMIT_PACKETS,
    FAM_IB_MULTICAST_RCV_PACKETS,
    FAM_IB_MULTICAST_XMIT_PACKETS,
    FAM_IB_MAX,
};

static metric_family_t fams[FAM_IB_MAX] = {
    [FAM_IB_STATE] = {
        .name = "system_infiniband_state",
        .type = METRIC_TYPE_GAUGE,
        .help = "Port state (4 is \"Active\")."
    },
    [FAM_IB_PHYSICAL_STATE] = {
        .name = "system_infiniband_physical_state",
        .type = METRIC_TYPE_GAUGE,
        .help = "Port physical state (5 is \"LinkUp\").",
    },
    [FAM_IB_RATE] = {
        .name = "system_infiniband_rate",
        .type = METRIC_TYPE_GAUGE,
        .help = "Currently active extended link speed, in Gb/s.",
    },
    [FAM_IB_CAPABILITIES_MASK] = {
        .name = "system_infiniband_capabilities_mask",
        .type = METRIC_TYPE_GAUGE,
        .help = "Supported capabilities of this port.",
    },
    [FAM_IB_LOCAL_IDENTIFIER] = {
        .name = "system_infiniband_local_identifier",
        .type = METRIC_TYPE_GAUGE,
        .help = "The base LID (local identifier) of this port.",
    },
    [FAM_IB_LOCAL_IDENTIFIER_MASK_COUNT] = {
        .name = "system_infiniband_local_identifier_mask_count",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of low order bits of the LID to mask (for multipath).",
    },
    [FAM_IB_SUBNET_MANAGER_LOCAL_IDENTIFIER] = {
        .name = "system_infiniband_subnet_manager_local_identifier",
        .type = METRIC_TYPE_GAUGE,
        .help = "The LID of the master SM (subnet manager) that is managing this port.",
    },
    [FAM_IB_SUBNET_MANAGER_SERVICE_LEVEL] = {
        .name = "system_infiniband_subnet_manager_service_level",
        .type = METRIC_TYPE_GAUGE,
        .help = "The administrative SL (service level) of the master SM that is managing this port.",
    },
    [FAM_IB_PORT_RCV_DATA] = {
        .name = "system_infiniband_port_rcv_data",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of data octets, divided by 4, received on all VLs at the port.",
    },
    [FAM_IB_PORT_XMIT_DATA] = {
        .name = "system_infiniband_port_xmit_data",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of data octets, divided by 4, received on all VLs at the port.",
    },
    [FAM_IB_PORT_RCV_PACKETS] = {
        .name = "system_infiniband_port_rcv_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of packets, including packets containing errors, "
                "and excluding link packets, received from all VLs on the port.",
    },
    [FAM_IB_PORT_XMIT_PACKETS] = {
        .name = "system_infiniband_port_xmit_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of packets, including packets containing errors, "
                "and excluding link packets, received from all VLs on the port.",
    },
    [FAM_IB_PORT_RCV_ERRORS] = {
        .name = "system_infiniband_port_rcv_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of packets containing an error that were received on the port.",
    },
    [FAM_IB_PORT_RCV_REMOTE_PHYSICAL_ERRORS] = {
        .name = "system_infiniband_port_rcv_remote_physical_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of packets marked with the EBP delimiter received on the port.",
    },
    [FAM_IB_PORT_RCV_SWITCH_RELAY_ERRORS] = {
        .name = "system_infiniband_port_rcv_switch_relay_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of packets received on the port that were discarded "
                "because they could not be forwarded by the switch relay.",
    },
    [FAM_IB_PORT_XMIT_DISCARDS] = {
        .name = "system_infiniband_port_xmit_discards",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of outbound packets discarded by the port "
                "because the port is down or congested.",
    },
    [FAM_IB_PORT_RCV_CONSTRAINT_ERRORS] = {
        .name = "system_infiniband_port_rcv_constraint_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of packets not transmitted from the switch physical port",
    },
    [FAM_IB_PORT_XMIT_CONSTRAINT_ERRORS] = {
        .name = "system_infiniband_port_xmit_constraint_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of packets received on the switch physical port that are discarded.",
    },
    [FAM_IB_VL15_DROPPED] = {
        .name = "system_infiniband_VL15_dropped",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of incoming VL15 packets dropped due to resource limitations "
                "(e.g., lack of buffers) in the port.",
    },
    [FAM_IB_LINK_ERROR_RECOVERY] = {
        .name = "system_infiniband_link_error_recovery",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times the Port Training state machine has successfully "
                "completed the link error recovery process.",
    },
    [FAM_IB_LINK_DOWNED] = {
        .name = "system_infiniband_link_downed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times the Port Training state machine has failed "
                "the link error recovery process and downed the link.",
    },
    [FAM_IB_SYMBOL_ERROR] = {
        .name = "system_infiniband_symbol_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of minor link errors detected on one or more physical lanes.",
    },
    [FAM_IB_LOCAL_LINK_INTEGRITY_ERRORS] = {
        .name = "system_infiniband_local_link_integrity_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times that the count of local physical errors exceeded "
                "the threshold specified by LocalPhyErrors.",
    },
    [FAM_IB_EXCESSIVE_BUFFER_OVERRUN_ERRORS] = {
        .name = "system_infiniband_excessive_buffer_overrun_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times that OverrunErrors consecutive flow control update "
                "periods occurred, each having at least one overrun error.",
    },
    [FAM_IB_PORT_XMIT_WAIT] = {
        .name = "system_infiniband_port_xmit_wait",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ticks during which the port selected by PortSelect "
                "had data to transmit but no data was sent during the entire tick.",
    },
    [FAM_IB_UNICAST_RCV_PACKETS] = {
        .name = "system_infiniband_unicast_rcv_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of unicast packets, including unicast packets containing errors.",
    },
    [FAM_IB_UNICAST_XMIT_PACKETS] = {
        .name = "system_infiniband_unicast_xmit_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of unicast packets transmitted on all VLs from the port. "
                "This may include unicast packets with errors.",
    },
    [FAM_IB_MULTICAST_RCV_PACKETS] = {
        .name = "system_infiniband_multicast_rcv_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of multicast packets, including multicast packets containing errors.",
    },
    [FAM_IB_MULTICAST_XMIT_PACKETS] = {
        .name = "system_infiniband_multicast_xmit_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of multicast packets transmitted on all VLs from the port. "
                "This may include multicast packets with errors.",
    },
};

static struct {
    bool strip;
    int fam;
    char *filename;
} ib_files[] = {
    { true,  FAM_IB_STATE,                           "state"                                    },
    { true,  FAM_IB_PHYSICAL_STATE,                  "phys_state"                               },
    { true,  FAM_IB_RATE,                            "rate"                                     },
    { false, FAM_IB_CAPABILITIES_MASK,               "cap_mask"                                 },
    { false, FAM_IB_LOCAL_IDENTIFIER,                "lid"                                      },
    { false, FAM_IB_LOCAL_IDENTIFIER_MASK_COUNT,     "lid_mask_count"                           },
    { false, FAM_IB_SUBNET_MANAGER_LOCAL_IDENTIFIER, "sm_lid"                                   },
    { false, FAM_IB_SUBNET_MANAGER_SERVICE_LEVEL,    "sm_sl"                                    },
    { false, FAM_IB_PORT_RCV_DATA,                   "counters/port_rcv_data"                   },
    { false, FAM_IB_PORT_XMIT_DATA,                  "counters/port_xmit_data"                  },
    { false, FAM_IB_PORT_RCV_PACKETS,                "counters/port_rcv_packets"                },
    { false, FAM_IB_PORT_XMIT_PACKETS,               "counters/port_xmit_packets"               },
    { false, FAM_IB_PORT_RCV_ERRORS,                 "counters/port_rcv_errors"                 },
    { false, FAM_IB_PORT_RCV_REMOTE_PHYSICAL_ERRORS, "counters/port_rcv_remote_physical_errors" },
    { false, FAM_IB_PORT_RCV_SWITCH_RELAY_ERRORS,    "counters/port_rcv_switch_relay_errors"    },
    { false, FAM_IB_PORT_XMIT_DISCARDS,              "counters/port_xmit_discards"              },
    { false, FAM_IB_PORT_RCV_CONSTRAINT_ERRORS,      "counters/port_rcv_constraint_errors"      },
    { false, FAM_IB_PORT_XMIT_CONSTRAINT_ERRORS,     "counters/port_xmit_constraint_errors"     },
    { false, FAM_IB_VL15_DROPPED,                    "counters/VL15_dropped"                    },
    { false, FAM_IB_LINK_ERROR_RECOVERY,             "counters/link_error_recovery"             },
    { false, FAM_IB_LINK_DOWNED,                     "counters/link_downed"                     },
    { false, FAM_IB_SYMBOL_ERROR,                    "counters/symbol_error"                    },
    { false, FAM_IB_LOCAL_LINK_INTEGRITY_ERRORS,     "counters/local_link_integrity_errors"     },
    { false, FAM_IB_EXCESSIVE_BUFFER_OVERRUN_ERRORS, "counters/excessive_buffer_overrun_errors" },
    { false, FAM_IB_PORT_XMIT_WAIT,                  "counters/port_xmit_wait"                  },
    { false, FAM_IB_UNICAST_RCV_PACKETS,             "counters/unicast_rcv_packets"             },
    { false, FAM_IB_UNICAST_XMIT_PACKETS,            "counters/unicast_xmit_packets"            },
    { false, FAM_IB_MULTICAST_RCV_PACKETS,           "counters/multicast_rcv_packets"           },
    { false, FAM_IB_MULTICAST_XMIT_PACKETS,          "counters/multicast_xmit_packets"          },
};


static char *path_sys_infiniband;
static char *path_sys_glob;
static exclist_t excl_port;
static const int device_tok_idx = 3, port_tok_idx = 5;

static int ib_parse_glob_port(char *path, char **device, char **port)
{
    char *tok, *saveptr = NULL;
    int j = 0;
    *device = NULL;
    *port = NULL;
    tok = strtok_r(path, "/", &saveptr);
    while (tok != NULL) {
        if (j == device_tok_idx)
            *device = tok;
        else if (j == port_tok_idx) {
            *port = tok;
            break;
        }
        j++;
        tok = strtok_r(NULL, "/", &saveptr);
    }
    return (*device != NULL && *port != NULL) ? 0 : 1;
}

/**
 * For further reading on the available sysfs files, see:
 * - Linux: ./Documentation/infiniband/sysfs.txt
 *   https://www.kernel.org/doc/Documentation/ABI/stable/sysfs-class-infiniband
 * For further reading on the meaning of each counter, see the InfiniBand
 *   Architecture Specification, sections 14.2.5.6 and 16.1.3.5.
 **/
static int ib_read_port(const char *device, const char *port)
{
    char path[PATH_MAX];

    for (size_t i = 0; i < STATIC_ARRAY_SIZE(ib_files); i++) {

        if (snprintf(path, sizeof(path), "%s/%s/ports/%s/%s",
                     path_sys_infiniband, device, port, ib_files[i].filename) < 0)
            continue;

        char buffer[256];
        ssize_t len = read_text_file_contents(path, buffer, sizeof(buffer));
        if (len < 0)
            continue;

        char *ptr = strntrim(buffer, len);
        if (ib_files[i].strip) {
         /*
            * Used to parse files like this:
            * rate:       "100 Gb/sec"
            * state:      "4: ACTIVE"
            * phys_state: "5: LinkUp"
            */
            while(*ptr != '\0') {
                if (!isdigit(*ptr)) {
                    *(ptr++) = '\0';
                    break;
                }
                ptr++;
            }
        }

        value_t value = {0};
        if (fams[ib_files[i].fam].type == METRIC_TYPE_GAUGE) {
            double raw = 0;
            strtodouble(ptr, &raw);
            value = VALUE_GAUGE(raw);
        } else if (fams[ib_files[i].fam].type == METRIC_TYPE_COUNTER) {
            uint64_t raw = 0;
            strtouint(ptr, &raw);
            value = VALUE_COUNTER(raw);
        } else {
            continue;
        }

        metric_family_append(&fams[ib_files[i].fam], value, NULL,
                             &(label_pair_const_t){.name="device", .value=device},
                             &(label_pair_const_t){.name="port", .value=port}, NULL);
    }

    return 0;
}

static int infiniband_read(void)
{
    int rc = 0;
    glob_t g;
    char port_name[255];

    if (glob(path_sys_glob, GLOB_NOSORT, NULL, &g) == 0) {
        for (size_t i = 0; i < g.gl_pathc; ++i) {
            char *device = NULL, *port = NULL;
            if (ib_parse_glob_port(g.gl_pathv[i], &device, &port) == 0) {
                snprintf(port_name, sizeof(port_name), "%s:%s", device, port);
                if (exclist_match(&excl_port, port_name))
                    rc &= ib_read_port(device, port);
            }
        }
    }

    globfree(&g);
    return rc;
}

static int infiniband_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "port") == 0) {
            status = cf_util_exclist(child, &excl_port);
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

static int infiniband_init(void)
{
    path_sys_infiniband = plugin_syspath("class/infiniband");
    if (path_sys_infiniband == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    path_sys_glob = plugin_syspath("class/infiniband/*/ports/*/state");
    if (path_sys_glob == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }
    return 0;
}

static int infiniband_shutdown(void)
{
    free(path_sys_infiniband);
    free(path_sys_glob);
    exclist_reset(&excl_port);
    return 0;
}

void module_register(void)
{
    plugin_register_config("infiniband", infiniband_config);
    plugin_register_init("infiniband", infiniband_init);
    plugin_register_read("infiniband", infiniband_read);
    plugin_register_shutdown("infiniband", infiniband_shutdown);
}
