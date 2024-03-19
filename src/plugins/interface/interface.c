// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2020  Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2009-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Sune Marcher <sm at flork.dk>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"
#include "interface.h"

metric_family_t fams[FAM_INTERFACE_MAX] = {
    [FAM_INTERFACE_STATE_UP] = {
        .name = "system_interface_state_up",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_INTERFACE_ADMIN_STATE_UP] = {
        .name = "system_interface_admin_state_up",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_INTERFACE_CARRIER] = {
        .name = "system_interface_carrier",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_INTERFACE_CARRIER_UP] = {
        .name = "system_interface_carrier_up",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_CARRIER_DOWN] = {
        .name = "system_interface_carrier_down",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_RX_PACKETS] = {
        .name = "system_interface_rx_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_RX_BYTES] = {
        .name = "system_interface_rx_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_RX_DROPPED] = {
        .name = "system_interface_rx_dropped",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_RX_ERRORS] = {
        .name = "system_interface_rx_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_RX_NOHANDLER] = {
        .name = "system_interface_rx_nohandler",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_RX_COMPRESSED] = {
        .name = "system_interface_rx_compressed",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_RX_LENGTH_ERRORS] = {
        .name = "system_interface_rx_length_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_RX_OVER_ERRORS] = {
        .name = "system_interface_rx_over_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_RX_CRC_ERRORS] = {
        .name = "system_interface_rx_crc_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_RX_FRAME_ERRORS] = {
        .name = "system_interface_rx_frame_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_RX_FIFO_ERRORS] = {
        .name = "system_interface_rx_fifo_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_RX_MISSED_ERRORS] = {
        .name = "system_interface_rx_missed_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_TX_PACKETS] = {
        .name = "system_interface_tx_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_TX_BYTES] = {
        .name = "system_interface_tx_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_TX_DROPPED] = {
        .name = "system_interface_tx_dropped",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_TX_ERRORS] = {
        .name = "system_interface_tx_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_TX_COMPRESSED] = {
        .name = "system_interface_tx_compressed",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_MULTICAST] = {
        .name = "system_interface_multicast",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_COLLISIONS] = {
        .name = "system_interface_collisions",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_TX_ABORTED_ERRORS] = {
        .name = "system_interface_tx_aborted_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_TX_CARRIER_ERRORS] = {
        .name = "system_interface_tx_carrier_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_TX_FIFO_ERRORS] = {
        .name = "system_interface_tx_fifo_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_TX_HEARTBEAT_ERRORS] = {
        .name = "system_interface_tx_heartbeat_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_TX_WINDOW_ERRORS] = {
        .name = "system_interface_tx_window_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_VF_LINK_VLAN] = {
        .name = "system_interface_vf_link_vlan",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_INTERFACE_VF_LINK_QOS] = {
        .name = "system_interface_vf_link_qos",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_INTERFACE_VF_LINK_SPOOFCHECK] = {
        .name = "system_interface_vf_link_spoofcheck",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_INTERFACE_VF_LINK_STATE] = {
        .name = "system_interface_vf_link_state",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_INTERFACE_VF_LINK_TXRATE] = {
        .name = "system_interface_vf_link_txrate",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_INTERFACE_VF_LINK_MIN_TXRATE] = {
        .name = "system_interface_vf_link_min_txrate",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_INTERFACE_VF_LINK_MAX_TXRATE] = {
        .name = "system_interface_vf_link_max_txrate",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_INTERFACE_VF_LINK_RSS_QUERY_EN] = {
        .name = "system_interface_vf_link_rss_query_en",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_INTERFACE_VF_LINK_TRUST] = {
        .name = "system_interface_vf_link_trust",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_INTERFACE_VF_RX_PACKETS] = {
        .name = "system_interface_vf_rx_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_VF_TX_PACKETS] = {
        .name = "system_interface_vf_tx_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_VF_RX_BYTES] = {
        .name = "system_interface_vf_rx_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_VF_TX_BYTES] = {
        .name = "system_interface_vf_tx_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_VF_BROADCAST] = {
        .name = "system_interface_vf_broadcast",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_VF_MULTICAST] = {
        .name = "system_interface_vf_multicast",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_VF_RX_DROPPED] = {
        .name = "system_interface_vf_rx_dropped",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_INTERFACE_VF_TX_DROPPED] = {
        .name = "system_interface_vf_tx_dropped",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
};

exclist_t excl_device;
bool report_inactive = true;
bool collect_vf_stats = false;
bool unique_name = false;

static int interface_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "interface") == 0) {
            status = cf_util_exclist(child, &excl_device);
        } else if (strcasecmp(child->key, "report-inactive") == 0) {
            status = cf_util_get_boolean(child, &report_inactive);
        } else if (strcasecmp(child->key, "unique-name") == 0) {
#ifdef HAVE_LIBKSTAT
            status = cf_util_get_boolean(child, &unique_name);
#else
            PLUGIN_WARNING("the 'unique-name' option is only valid on Solaris.");
#endif
        } else if (strcasecmp(child->key, "collect-vf-stats") == 0) {
#ifdef HAVE_IFLA_VF_STATS
            status = cf_util_get_boolean(child, &collect_vf_stats);
#else
            PLUGIN_WARNING("VF statistics not supported on this system.");
#endif
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

int interface_read(void);

__attribute__(( weak ))
int interface_init(void)
{
    return 0;
}

__attribute__(( weak ))
int interface_shutdown(void)
{
    exclist_reset(&excl_device);
    return 0;
}

void module_register(void)
{
    plugin_register_config("interface", interface_config);
    plugin_register_init("interface", interface_init);
    plugin_register_shutdown("interface", interface_shutdown);
    plugin_register_read("interface", interface_read);
}
