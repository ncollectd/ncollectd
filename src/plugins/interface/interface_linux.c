// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2020  Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2008-2012 Sebastian Harl
// SPDX-FileCopyrightText: Copyright (C) 2009-2024 Manuel Sanmartín
// SPDX-FileCopyrightText: Copyright (C) 2013 Andreas Henriksson
// SPDX-FileCopyrightText: Copyright (C) 2013 Marc Fournier
// SPDX-FileCopyrightText: Copyright (C) 2020 Intel Corporation
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>
// SPDX-FileContributor: Sune Marcher <sm at flork.dk>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>
// SPDX-FileContributor: Andreas Henriksson <andreas at fatal.se>
// SPDX-FileContributor: Marc Fournier <marc.fournier at camptocamp.com>
// SPDX-FileContributor: Kamil Wiatrowski <kamilx.wiatrowski at intel.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"
#include "interface.h"

#include <asm/types.h>

#include <glob.h>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#ifdef HAVE_LINUX_GEN_STATS_H
#include <linux/gen_stats.h>
#endif
#ifdef HAVE_LINUX_PKT_SCHED_H
#include <linux/pkt_sched.h>
#endif

#include <libmnl/libmnl.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

/* One cannot include both. This sucks. */
#ifdef HAVE_LINUX_IF_H
#include <linux/if.h>
#elif defined(HAVE_NET_IF_H)
#include <net/if.h>
#endif

#ifdef HAVE_LINUX_NETDEVICE_H
#include <linux/netdevice.h>
#endif
#if defined(HAVE_IFADDRS_H)
#include <ifaddrs.h>
#endif

#define NETLINK_VF_DEFAULT_BUF_SIZE_KB 16

struct ir_link_stats_storage_s {
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_errors;
    uint64_t tx_errors;

    uint64_t rx_dropped;
    uint64_t tx_dropped;
    uint64_t multicast;
    uint64_t collisions;
    uint64_t rx_nohandler;

    uint64_t rx_length_errors;
    uint64_t rx_over_errors;
    uint64_t rx_crc_errors;
    uint64_t rx_frame_errors;
    uint64_t rx_fifo_errors;
    uint64_t rx_missed_errors;

    uint64_t tx_aborted_errors;
    uint64_t tx_carrier_errors;
    uint64_t tx_fifo_errors;
    uint64_t tx_heartbeat_errors;
    uint64_t tx_window_errors;

    uint64_t rx_compressed;
    uint64_t tx_compressed;
};

union ir_link_stats_u {
    struct rtnl_link_stats *stats32;
#ifdef HAVE_RTNL_LINK_STATS64
    struct rtnl_link_stats64 *stats64;
#endif
};

#ifdef HAVE_IFLA_VF_STATS
typedef struct vf_stats_s {
    struct ifla_vf_mac *vf_mac;
    uint32_t vlan;
    uint32_t qos;
    uint32_t spoofcheck;
    uint32_t link_state;
    uint32_t txrate;
    uint32_t min_txrate;
    uint32_t max_txrate;
    uint32_t rss_query_en;
    uint32_t trust;

    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t broadcast;
    uint64_t multicast;

#ifdef HAVE_IFLA_VF_STATS_RX_DROPPED
    uint64_t rx_dropped;
#endif
#ifdef HAVE_IFLA_VF_STATS_TX_DROPPED
    uint64_t tx_dropped;
#endif
} vf_stats_t;
#endif

static size_t nl_socket_buffer_size = NETLINK_VF_DEFAULT_BUF_SIZE_KB * 1024;

static struct mnl_socket *nl;
static char *path_proc_dev;
static char *path_proc_net_if_inet6;

extern bool collect_vf_stats;
extern exclist_t excl_device;
extern bool report_inactive;
extern metric_family_t fams[FAM_INTERFACE_MAX];

static void check_ignorelist_and_submit(const char *dev, struct ir_link_stats_storage_s *stats)
{
    label_pair_const_t interface_label = {.name="interface", .value=dev};

    metric_family_append(&fams[FAM_INTERFACE_RX_PACKETS],
                         VALUE_COUNTER(stats->rx_packets), NULL, &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_TX_PACKETS],
                         VALUE_COUNTER(stats->tx_packets), NULL, &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_RX_BYTES],
                         VALUE_COUNTER(stats->rx_bytes), NULL, &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_TX_BYTES],
                         VALUE_COUNTER(stats->tx_bytes), NULL, &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_RX_ERRORS],
                         VALUE_COUNTER(stats->rx_errors), NULL, &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_TX_ERRORS],
                         VALUE_COUNTER(stats->tx_errors), NULL, &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_RX_DROPPED],
                         VALUE_COUNTER(stats->rx_dropped), NULL, &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_TX_DROPPED],
                         VALUE_COUNTER(stats->tx_dropped), NULL, &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_MULTICAST],
                         VALUE_COUNTER(stats->multicast), NULL, &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_COLLISIONS],
                         VALUE_COUNTER(stats->collisions), NULL, &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_RX_NOHANDLER],
                         VALUE_COUNTER(stats->rx_nohandler), NULL, &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_RX_LENGTH_ERRORS],
                         VALUE_COUNTER(stats->rx_length_errors), NULL, &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_RX_OVER_ERRORS],
                         VALUE_COUNTER(stats->rx_over_errors), NULL, &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_RX_CRC_ERRORS],
                         VALUE_COUNTER(stats->rx_crc_errors), NULL, &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_RX_FRAME_ERRORS],
                         VALUE_COUNTER(stats->rx_frame_errors), NULL, &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_RX_FIFO_ERRORS],
                         VALUE_COUNTER(stats->rx_fifo_errors), NULL, &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_RX_MISSED_ERRORS],
                         VALUE_COUNTER(stats->rx_missed_errors), NULL, &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_TX_ABORTED_ERRORS],
                         VALUE_COUNTER(stats->tx_aborted_errors), NULL, &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_TX_CARRIER_ERRORS],
                         VALUE_COUNTER(stats->tx_carrier_errors), NULL, &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_TX_FIFO_ERRORS],
                         VALUE_COUNTER(stats->tx_fifo_errors), NULL, &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_TX_HEARTBEAT_ERRORS],
                         VALUE_COUNTER(stats->tx_heartbeat_errors), NULL, &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_TX_WINDOW_ERRORS],
                         VALUE_COUNTER(stats->tx_window_errors), NULL, &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_RX_COMPRESSED],
                         VALUE_COUNTER(stats->rx_compressed), NULL, &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_TX_COMPRESSED],
                         VALUE_COUNTER(stats->tx_compressed), NULL, &interface_label, NULL);
}

#define COPY_RTNL_LINK_VALUE(dst_stats, src_stats, value_name)          \
    (dst_stats)->value_name = (src_stats)->value_name

#define COPY_RTNL_LINK_STATS(dst_stats, src_stats)                      \
    COPY_RTNL_LINK_VALUE(dst_stats, src_stats, rx_packets);             \
    COPY_RTNL_LINK_VALUE(dst_stats, src_stats, tx_packets);             \
    COPY_RTNL_LINK_VALUE(dst_stats, src_stats, rx_bytes);               \
    COPY_RTNL_LINK_VALUE(dst_stats, src_stats, tx_bytes);               \
    COPY_RTNL_LINK_VALUE(dst_stats, src_stats, rx_errors);              \
    COPY_RTNL_LINK_VALUE(dst_stats, src_stats, tx_errors);              \
    COPY_RTNL_LINK_VALUE(dst_stats, src_stats, rx_dropped);             \
    COPY_RTNL_LINK_VALUE(dst_stats, src_stats, tx_dropped);             \
    COPY_RTNL_LINK_VALUE(dst_stats, src_stats, multicast);              \
    COPY_RTNL_LINK_VALUE(dst_stats, src_stats, collisions);             \
    COPY_RTNL_LINK_VALUE(dst_stats, src_stats, rx_length_errors);       \
    COPY_RTNL_LINK_VALUE(dst_stats, src_stats, rx_over_errors);         \
    COPY_RTNL_LINK_VALUE(dst_stats, src_stats, rx_crc_errors);          \
    COPY_RTNL_LINK_VALUE(dst_stats, src_stats, rx_frame_errors);        \
    COPY_RTNL_LINK_VALUE(dst_stats, src_stats, rx_fifo_errors);         \
    COPY_RTNL_LINK_VALUE(dst_stats, src_stats, rx_missed_errors);       \
    COPY_RTNL_LINK_VALUE(dst_stats, src_stats, tx_aborted_errors);      \
    COPY_RTNL_LINK_VALUE(dst_stats, src_stats, tx_carrier_errors);      \
    COPY_RTNL_LINK_VALUE(dst_stats, src_stats, tx_fifo_errors);         \
    COPY_RTNL_LINK_VALUE(dst_stats, src_stats, tx_heartbeat_errors);    \
    COPY_RTNL_LINK_VALUE(dst_stats, src_stats, tx_window_errors);       \
    COPY_RTNL_LINK_VALUE(dst_stats, src_stats, rx_compressed);          \
    COPY_RTNL_LINK_VALUE(dst_stats, src_stats, tx_compressed)

#ifdef HAVE_RTNL_LINK_STATS64
static void check_ignorelist_and_submit64(const char *dev, struct rtnl_link_stats64 *stats)
{
    struct ir_link_stats_storage_s s;

    COPY_RTNL_LINK_STATS(&s, stats);
#ifdef HAVE_STRUCT_RTNL_LINK_STATS64_RX_NOHANDLER
    COPY_RTNL_LINK_VALUE(&s, stats, rx_nohandler);
#endif

    check_ignorelist_and_submit(dev, &s);
}
#endif

static void check_ignorelist_and_submit32(const char *dev, struct rtnl_link_stats *stats)
{
    struct ir_link_stats_storage_s s;

    COPY_RTNL_LINK_STATS(&s, stats);
#ifdef HAVE_STRUCT_RTNL_LINK_STATS_RX_NOHANDLER
    COPY_RTNL_LINK_VALUE(&s, stats, rx_nohandler);
#endif

    check_ignorelist_and_submit(dev, &s);
}

#ifdef HAVE_IFLA_VF_STATS
static void vf_info_submit(const char *dev, vf_stats_t *vf_stats)
{
    if (vf_stats->vf_mac == NULL) {
        PLUGIN_ERROR("vf_info_submit: failed to get VF macaddress, "
                     "skipping VF for interface %s", dev);
        return;
    }

    label_pair_t pairs[2];
    label_set_t labels = {.num = STATIC_ARRAY_SIZE(pairs), .ptr = pairs };

    char buffer[512];
    uint8_t *mac = vf_stats->vf_mac->mac;
    ssnprintf(buffer, sizeof(buffer), "%02x:%02x:%02x:%02x:%02x:%02x",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    labels.ptr[0] = (label_pair_t){.name="mac", .value=buffer};

    ssnprintf(buffer, sizeof(buffer), "%u", vf_stats->vf_mac->vf);
    labels.ptr[1] = (label_pair_t){.name="vf_num", .value=buffer};

    label_pair_const_t dev_label = {.name="interface", .value=dev};

    metric_family_append(&fams[FAM_INTERFACE_VF_LINK_VLAN],
                         VALUE_GAUGE(vf_stats->vlan), &labels, &dev_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_VF_LINK_QOS],
                         VALUE_GAUGE(vf_stats->qos), &labels, &dev_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_VF_LINK_SPOOFCHECK],
                         VALUE_GAUGE(vf_stats->spoofcheck), &labels, &dev_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_VF_LINK_STATE],
                         VALUE_GAUGE(vf_stats->link_state), &labels, &dev_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_VF_LINK_TXRATE],
                         VALUE_GAUGE(vf_stats->txrate), &labels, &dev_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_VF_LINK_MIN_TXRATE],
                         VALUE_GAUGE(vf_stats->min_txrate), &labels, &dev_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_VF_LINK_MAX_TXRATE],
                         VALUE_GAUGE(vf_stats->max_txrate), &labels, &dev_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_VF_LINK_RSS_QUERY_EN],
                         VALUE_GAUGE(vf_stats->rss_query_en), &labels, &dev_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_VF_LINK_TRUST],
                         VALUE_GAUGE(vf_stats->trust), &labels, &dev_label, NULL);

    metric_family_append(&fams[FAM_INTERFACE_VF_RX_PACKETS],
                         VALUE_COUNTER(vf_stats->rx_packets), &labels, &dev_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_VF_TX_PACKETS],
                         VALUE_COUNTER(vf_stats->tx_packets), &labels, &dev_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_VF_RX_BYTES],
                         VALUE_COUNTER(vf_stats->rx_bytes), &labels, &dev_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_VF_TX_BYTES],
                         VALUE_COUNTER(vf_stats->tx_bytes), &labels, &dev_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_VF_BROADCAST],
                         VALUE_COUNTER(vf_stats->broadcast), &labels, &dev_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_VF_MULTICAST],
                         VALUE_COUNTER(vf_stats->multicast), &labels, &dev_label, NULL);

#ifdef HAVE_IFLA_VF_STATS_RX_DROPPED
    metric_family_append(&fams[FAM_INTERFACE_VF_RX_DROPPED],
                         VALUE_COUNTER(vf_stats->rx_dropped), &labels, &dev_label, NULL);
#endif
#ifdef HAVE_IFLA_VF_STATS_TX_DROPPED
    metric_family_append(&fams[FAM_INTERFACE_VF_TX_DROPPED],
                         VALUE_COUNTER(vf_stats->tx_dropped), &labels, &dev_label, NULL);
#endif

}

#define IFCOPY_VF_STAT_VALUE(attr, name, type_name)              \
    do {                                                         \
        if (mnl_attr_get_type(attr) == type_name) {              \
            if (mnl_attr_validate(attr, MNL_TYPE_U64) < 0) {     \
                PLUGIN_ERROR("vf_info_attr_cb: " #type_name      \
                            " mnl_attr_validate failed.");       \
                return MNL_CB_ERROR;                             \
            }                                                    \
            vf_stats->name = mnl_attr_get_u64(attr);             \
        }                                                        \
    } while (0)

static int vf_info_attr_cb(const struct nlattr *attr, void *args)
{
    vf_stats_t *vf_stats = (vf_stats_t *)args;

    /* skip unsupported attribute */
    if (mnl_attr_type_valid(attr, IFLA_VF_MAX) < 0) {
        return MNL_CB_OK;
    }

    uint16_t type = mnl_attr_get_type(attr);

    switch(type) {
    case IFLA_VF_MAC: {
        if (mnl_attr_validate2(attr, MNL_TYPE_UNSPEC, sizeof(*vf_stats->vf_mac)) < 0) {
            PLUGIN_ERROR("IFLA_VF_MAC mnl_attr_validate2 failed: %s", STRERRNO);
            return MNL_CB_ERROR;
        }

        vf_stats->vf_mac = (struct ifla_vf_mac *)mnl_attr_get_payload(attr);
        return MNL_CB_OK;
    }   break;
    case IFLA_VF_VLAN: {
        struct ifla_vf_vlan *vf_vlan;
        if (mnl_attr_validate2(attr, MNL_TYPE_UNSPEC, sizeof(*vf_vlan)) < 0) {
            PLUGIN_ERROR("IFLA_VF_VLAN mnl_attr_validate2 failed: %s", STRERRNO);
            return MNL_CB_ERROR;
        }

        vf_vlan = (struct ifla_vf_vlan *)mnl_attr_get_payload(attr);
        vf_stats->vlan = vf_vlan->vlan;
        vf_stats->qos = vf_vlan->qos;
        return MNL_CB_OK;
    }   break;
    case IFLA_VF_TX_RATE: {
        struct ifla_vf_tx_rate *vf_txrate;
        if (mnl_attr_validate2(attr, MNL_TYPE_UNSPEC, sizeof(*vf_txrate)) < 0) {
            PLUGIN_ERROR("IFLA_VF_TX_RATE mnl_attr_validate2 failed: %s", STRERRNO);
            return MNL_CB_ERROR;
        }

        vf_txrate = (struct ifla_vf_tx_rate *)mnl_attr_get_payload(attr);
        vf_stats->txrate = vf_txrate->rate;
        return MNL_CB_OK;
    }   break;
    case IFLA_VF_SPOOFCHK: {
        struct ifla_vf_spoofchk *vf_spoofchk;
        if (mnl_attr_validate2(attr, MNL_TYPE_UNSPEC, sizeof(*vf_spoofchk)) < 0) {
            PLUGIN_ERROR("IFLA_VF_SPOOFCHK mnl_attr_validate2 failed: %s", STRERRNO);
            return MNL_CB_ERROR;
        }

        vf_spoofchk = (struct ifla_vf_spoofchk *)mnl_attr_get_payload(attr);
        vf_stats->spoofcheck = vf_spoofchk->setting;
        return MNL_CB_OK;
    }   break;
    case IFLA_VF_LINK_STATE: {
        struct ifla_vf_link_state *vf_link_state;
        if (mnl_attr_validate2(attr, MNL_TYPE_UNSPEC, sizeof(*vf_link_state)) < 0) {
            PLUGIN_ERROR("IFLA_VF_LINK_STATE mnl_attr_validate2 failed: %s", STRERRNO);
            return MNL_CB_ERROR;
        }

        vf_link_state = (struct ifla_vf_link_state *)mnl_attr_get_payload(attr);
        vf_stats->link_state = vf_link_state->link_state;
        return MNL_CB_OK;
    }   break;
    case IFLA_VF_RATE: {
        struct ifla_vf_rate *vf_rate;
        if (mnl_attr_validate2(attr, MNL_TYPE_UNSPEC, sizeof(*vf_rate)) < 0) {
            PLUGIN_ERROR("IFLA_VF_RATE mnl_attr_validate2 failed: %s", STRERRNO);
            return MNL_CB_ERROR;
        }

        vf_rate = (struct ifla_vf_rate *)mnl_attr_get_payload(attr);
        vf_stats->min_txrate = vf_rate->min_tx_rate;
        vf_stats->max_txrate = vf_rate->max_tx_rate;
        return MNL_CB_OK;
    }   break;
    case IFLA_VF_RSS_QUERY_EN: {
        struct ifla_vf_rss_query_en *vf_rss_query_en;
        if (mnl_attr_validate2(attr, MNL_TYPE_UNSPEC, sizeof(*vf_rss_query_en)) < 0) {
            PLUGIN_ERROR(" IFLA_VF_RSS_QUERY_EN mnl_attr_validate2 failed: %s", STRERRNO);
            return MNL_CB_ERROR;
        }

        vf_rss_query_en = (struct ifla_vf_rss_query_en *)mnl_attr_get_payload(attr);
        vf_stats->rss_query_en = vf_rss_query_en->setting;
        return MNL_CB_OK;
    }   break;
    case IFLA_VF_TRUST: {
        struct ifla_vf_trust *vf_trust;
        if (mnl_attr_validate2(attr, MNL_TYPE_UNSPEC, sizeof(*vf_trust)) < 0) {
            PLUGIN_ERROR("IFLA_VF_TRUST mnl_attr_validate2 failed: %s", STRERRNO);
            return MNL_CB_ERROR;
        }

        vf_trust = (struct ifla_vf_trust *)mnl_attr_get_payload(attr);
        vf_stats->trust = vf_trust->setting;
        return MNL_CB_OK;
    }   break;
    case IFLA_VF_STATS: {
        if (mnl_attr_validate(attr, MNL_TYPE_NESTED) < 0) {
            PLUGIN_ERROR("IFLA_VF_STATS mnl_attr_validate failed.");
            return MNL_CB_ERROR;
        }

        struct nlattr *nested;
        mnl_attr_for_each_nested(nested, attr) {
            IFCOPY_VF_STAT_VALUE(nested, rx_packets, IFLA_VF_STATS_RX_PACKETS);
            IFCOPY_VF_STAT_VALUE(nested, tx_packets, IFLA_VF_STATS_TX_PACKETS);
            IFCOPY_VF_STAT_VALUE(nested, rx_bytes, IFLA_VF_STATS_RX_BYTES);
            IFCOPY_VF_STAT_VALUE(nested, tx_bytes, IFLA_VF_STATS_TX_BYTES);
            IFCOPY_VF_STAT_VALUE(nested, broadcast, IFLA_VF_STATS_BROADCAST);
            IFCOPY_VF_STAT_VALUE(nested, multicast, IFLA_VF_STATS_MULTICAST);
#ifdef HAVE_IFLA_VF_STATS_RX_DROPPED
            IFCOPY_VF_STAT_VALUE(nested, rx_dropped, IFLA_VF_STATS_RX_DROPPED);
#endif
#ifdef HAVE_IFLA_VF_STATS_TX_DROPPED
            IFCOPY_VF_STAT_VALUE(nested, tx_dropped, IFLA_VF_STATS_TX_DROPPED);
#endif
        }
        return MNL_CB_OK;
    }   break;
    }

    return MNL_CB_OK;
}
#endif

static int link_filter_cb(const struct nlmsghdr *nlh, void *args __attribute__((unused)))
{
    struct ifinfomsg *ifm = mnl_nlmsg_get_payload(nlh);
    struct nlattr *attr;
    union ir_link_stats_u stats;

    if (nlh->nlmsg_type != RTM_NEWLINK) {
        PLUGIN_ERROR("Don't know how to handle type %i.", nlh->nlmsg_type);
        return MNL_CB_ERROR;
    }

    int64_t oper_state = 0;
    int64_t carrier = 0;
    int64_t carrier_up_count = -1;
    int64_t carrier_down_count = -1;

    /* Scan attribute list for device name. */
    const char *dev = NULL;
    mnl_attr_for_each(attr, nlh, sizeof(*ifm)) {
        uint16_t type = mnl_attr_get_type(attr);

        switch (type) {
        case IFLA_CARRIER:
            if (mnl_attr_validate(attr, MNL_TYPE_U8) < 0)
                PLUGIN_WARNING("mnl_attr_validate IFLA_CARRIER failed.");
            else
                carrier = mnl_attr_get_u8(attr);
            break;
#ifdef HAVE_IFLA_CARRIER_UP_COUNT
        case  IFLA_CARRIER_UP_COUNT:
            if (mnl_attr_validate(attr, MNL_TYPE_U32) < 0)
                PLUGIN_WARNING("mnl_attr_validate IFLA_CARRIER_UP_COUNT failed.");
            else
                carrier_up_count = mnl_attr_get_u32(attr);
            break;
#endif
#ifdef HAVE_IFLA_CARRIER_DOWN_COUNT
        case IFLA_CARRIER_DOWN_COUNT:
            if (mnl_attr_validate(attr, MNL_TYPE_U32) < 0)
                PLUGIN_WARNING("mnl_attr_validate IFLA_CARRIER_DOWN_COUNT failed.");
            else
                carrier_down_count = mnl_attr_get_u32(attr);
            break;
#endif
        case IFLA_OPERSTATE:
            if (mnl_attr_validate(attr, MNL_TYPE_U8) < 0)
                PLUGIN_WARNING("mnl_attr_validate IFLA_OPERSTATE failed.");
            else
                oper_state = mnl_attr_get_u8(attr);
            break;
        case IFLA_IFNAME:
            if (mnl_attr_validate(attr, MNL_TYPE_STRING) < 0) {
                PLUGIN_ERROR("IFLA_IFNAME mnl_attr_validate failed.");
                return MNL_CB_ERROR;
            }

            dev = mnl_attr_get_str(attr);
            break;
        }
    }

    if (dev == NULL) {
        PLUGIN_ERROR("device name is NULL");
        return MNL_CB_ERROR;
    }

    if ((strlen(dev) == 0) || !exclist_match(&excl_device, dev))
        return MNL_CB_OK;

    // ifi->ifi_flags & IFF_RUNNING XXX

    label_pair_const_t interface_label = {.name="interface", .value=dev};

    metric_family_append(&fams[FAM_INTERFACE_STATE_UP],
                         VALUE_GAUGE(oper_state == 6 ? 1.0 : 0.0), NULL,
                        &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_ADMIN_STATE_UP],
                         VALUE_GAUGE(ifm->ifi_flags & IFF_UP ? 1.0 : 0.0), NULL,
                        &interface_label, NULL);
    metric_family_append(&fams[FAM_INTERFACE_CARRIER],
                         VALUE_GAUGE(carrier), NULL, &interface_label, NULL);

    if (carrier_up_count > 0)
        metric_family_append(&fams[FAM_INTERFACE_CARRIER_UP],
                             VALUE_COUNTER(carrier_up_count), NULL, &interface_label, NULL);
    if (carrier_down_count > 0)
        metric_family_append(&fams[FAM_INTERFACE_CARRIER_DOWN],
                             VALUE_COUNTER(carrier_down_count), NULL, &interface_label, NULL);

#ifdef HAVE_IFLA_VF_STATS
    uint32_t num_vfs = 0;
    if (collect_vf_stats) {
        mnl_attr_for_each(attr, nlh, sizeof(*ifm)) {
            if (mnl_attr_get_type(attr) != IFLA_NUM_VF)
                continue;

            if (mnl_attr_validate(attr, MNL_TYPE_U32) < 0) {
                PLUGIN_ERROR("IFLA_NUM_VF mnl_attr_validate failed.");
                return MNL_CB_ERROR;
            }

            num_vfs = mnl_attr_get_u32(attr);
            break;
        }
    }
#endif

    bool stats_done = false;
#ifdef HAVE_RTNL_LINK_STATS64
    mnl_attr_for_each(attr, nlh, sizeof(*ifm)) {
        if (mnl_attr_get_type(attr) != IFLA_STATS64)
            continue;

        uint16_t attr_len = mnl_attr_get_payload_len(attr);
        if (attr_len < sizeof(*stats.stats64)) {
            PLUGIN_ERROR("IFLA_STATS64 attribute has insufficient data.");
            return MNL_CB_ERROR;
        }
        stats.stats64 = mnl_attr_get_payload(attr);

        check_ignorelist_and_submit64(dev, stats.stats64);

        stats_done = true;
        break;
    }
#endif

    if (stats_done == false) {
        mnl_attr_for_each(attr, nlh, sizeof(*ifm)) {
            if (mnl_attr_get_type(attr) != IFLA_STATS)
                continue;

            uint16_t attr_len = mnl_attr_get_payload_len(attr);
            if (attr_len < sizeof(*stats.stats32)) {
                PLUGIN_ERROR("IFLA_STATS attribute has insufficient data.");
                return MNL_CB_ERROR;
            }
            stats.stats32 = mnl_attr_get_payload(attr);

            check_ignorelist_and_submit32(dev, stats.stats32);

#ifdef NCOLLECTD_DEBUG
            stats_done = true;
#endif
            break;
        }
    }

#ifdef NCOLLECTD_DEBUG
    if (stats_done == false)
        PLUGIN_DEBUG("No statistics for interface %s.", dev);
#endif

#ifdef HAVE_IFLA_VF_STATS
    if (num_vfs == 0)
        return MNL_CB_OK;

    /* Get VFINFO list. */
    mnl_attr_for_each(attr, nlh, sizeof(*ifm)) {
        if (mnl_attr_get_type(attr) != IFLA_VFINFO_LIST)
            continue;

        if (mnl_attr_validate(attr, MNL_TYPE_NESTED) < 0) {
            PLUGIN_ERROR("IFLA_VFINFO_LIST mnl_attr_validate failed.");
            return MNL_CB_ERROR;
        }

        struct nlattr *nested;
        mnl_attr_for_each_nested(nested, attr) {
            if (mnl_attr_get_type(nested) != IFLA_VF_INFO) {
                continue;
            }

            if (mnl_attr_validate(nested, MNL_TYPE_NESTED) < 0) {
                PLUGIN_ERROR("IFLA_VF_INFO mnl_attr_validate failed.");
                return MNL_CB_ERROR;
            }

            vf_stats_t vf_stats = {0};
            if (mnl_attr_parse_nested(nested, vf_info_attr_cb, &vf_stats) == MNL_CB_ERROR)
                return MNL_CB_ERROR;

            vf_info_submit(dev, &vf_stats);
        }
        break;
    }
#endif

    return MNL_CB_OK;
}

static int interface_read_netlink(void)
{
    unsigned int portid = mnl_socket_get_portid(nl);

    char buf[nl_socket_buffer_size];
    struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
    nlh->nlmsg_type = RTM_GETLINK;
    nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    /* coverity[Y2K38_SAFETY] */
    unsigned int seq = time(NULL);
    nlh->nlmsg_seq = seq;
    struct rtgenmsg *rt = mnl_nlmsg_put_extra_header(nlh, sizeof(*rt));
    rt->rtgen_family = AF_PACKET;

#ifdef HAVE_IFLA_VF_STATS
    if (collect_vf_stats &&
        mnl_attr_put_u32_check(nlh, sizeof(buf), IFLA_EXT_MASK, RTEXT_FILTER_VF) == 0) {
        PLUGIN_ERROR("FAILED to set RTEXT_FILTER_VF");
        return -1;
    }
#endif

    if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
        PLUGIN_ERROR("rtnl_wilddump_request failed.");
        return -1;
    }

    int ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
    while (ret > 0) {
        ret = mnl_cb_run(buf, ret, seq, portid, link_filter_cb, NULL);
        if (ret <= MNL_CB_STOP)
            break;
        ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
    }

    if (ret < 0) {
        PLUGIN_ERROR("mnl_socket_recvfrom failed: %s", STRERRNO);
        return (-1);
    }

    plugin_dispatch_metric_family_array(fams, FAM_INTERFACE_MAX, 0);

    return 0;
}

static int interface_read_proc(void)
{
    FILE *fh = fopen(path_proc_dev, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Cannot open '%s': %s", path_proc_dev, STRERRNO);
        return errno;
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *dummy = strchr(buffer, ':');
        if (dummy == NULL)
            continue;
        dummy[0] = 0;
        dummy++;

        char *device = buffer;
        while (device[0] == ' ')
            device++;

        if ((strlen(device) == 0) || !exclist_match(&excl_device, device))
            continue;

        char *fields[24];
        int numfields = strsplit(dummy, fields, STATIC_ARRAY_SIZE(fields));

        if (numfields < 16)
            continue;

        uint64_t rx = (uint64_t)atoll(fields[1]);
        uint64_t tx = (uint64_t)atoll(fields[9]);

        if (!report_inactive && (rx == 0) && (tx == 0))
            continue;

        label_pair_const_t device_label = {.name="device", .value=device};

        metric_family_append(&fams[FAM_INTERFACE_RX_BYTES],
                             VALUE_COUNTER(atoll(fields[0])), NULL, &device_label, NULL);
        metric_family_append(&fams[FAM_INTERFACE_RX_PACKETS],
                             VALUE_COUNTER(atoll(fields[1])), NULL, &device_label, NULL);
        metric_family_append(&fams[FAM_INTERFACE_RX_ERRORS],
                             VALUE_COUNTER(atoll(fields[2])), NULL, &device_label, NULL);
        metric_family_append(&fams[FAM_INTERFACE_RX_DROPPED],
                             VALUE_COUNTER(atoll(fields[3])), NULL, &device_label, NULL);
        metric_family_append(&fams[FAM_INTERFACE_RX_FIFO_ERRORS],
                             VALUE_COUNTER(atoll(fields[4])), NULL, &device_label, NULL);
        metric_family_append(&fams[FAM_INTERFACE_RX_FRAME_ERRORS],
                             VALUE_COUNTER(atoll(fields[5])), NULL, &device_label, NULL);
        metric_family_append(&fams[FAM_INTERFACE_RX_COMPRESSED],
                             VALUE_COUNTER(atoll(fields[6])), NULL, &device_label, NULL);
        metric_family_append(&fams[FAM_INTERFACE_MULTICAST],
                             VALUE_COUNTER(atoll(fields[7])), NULL, &device_label, NULL);
        metric_family_append(&fams[FAM_INTERFACE_TX_BYTES],
                             VALUE_COUNTER(atoll(fields[8])), NULL, &device_label, NULL);
        metric_family_append(&fams[FAM_INTERFACE_TX_PACKETS],
                             VALUE_COUNTER(atoll(fields[9])), NULL, &device_label, NULL);
        metric_family_append(&fams[FAM_INTERFACE_TX_ERRORS],
                             VALUE_COUNTER(atoll(fields[10])), NULL, &device_label, NULL);
        metric_family_append(&fams[FAM_INTERFACE_TX_DROPPED],
                             VALUE_COUNTER(atoll(fields[11])), NULL, &device_label, NULL);
        metric_family_append(&fams[FAM_INTERFACE_TX_FIFO_ERRORS],
                             VALUE_COUNTER(atoll(fields[12])), NULL, &device_label, NULL);
        metric_family_append(&fams[FAM_INTERFACE_COLLISIONS],
                             VALUE_COUNTER(atoll(fields[13])), NULL, &device_label, NULL);
        metric_family_append(&fams[FAM_INTERFACE_TX_CARRIER_ERRORS],
                             VALUE_COUNTER(atoll(fields[14])), NULL, &device_label, NULL);
        metric_family_append(&fams[FAM_INTERFACE_TX_COMPRESSED],
                             VALUE_COUNTER(atoll(fields[15])), NULL, &device_label, NULL);
    }

    fclose(fh);

    return 0;
}

int interface_read(void)
{
    if (nl != NULL)
        interface_read_netlink();
    else
        interface_read_proc();

    plugin_dispatch_metric_family_array(fams, FAM_INTERFACE_MAX, 0);

    return 0;
}

static size_t interface_get_buffer_size(void)
{
    if (collect_vf_stats == false) {
        return MNL_SOCKET_BUFFER_SIZE;
    }

    glob_t g;
    unsigned int max_num = 0;
    if (glob("/sys/class/net/*/device/sriov_totalvfs", GLOB_NOSORT, NULL, &g)) {
        PLUGIN_ERROR("glob failed");
        /* using default value */
        return NETLINK_VF_DEFAULT_BUF_SIZE_KB * 1024;
    }

    for (size_t i = 0; i < g.gl_pathc; i++) {
        char buf[16];
        ssize_t len;
        int num = 0;
        int fd = open(g.gl_pathv[i], O_RDONLY);
        if (fd < 0) {
            PLUGIN_WARNING("failed to open `%s.`", g.gl_pathv[i]);
            continue;
        }

        if ((len = read(fd, buf, sizeof(buf) - 1)) <= 0) {
            PLUGIN_WARNING("failed to read `%s.`", g.gl_pathv[i]);
            close(fd);
            continue;
        }
        buf[len] = '\0';

        if (sscanf(buf, "%d", &num) != 1) {
            PLUGIN_WARNING("failed to read number from `%s.`", buf);
            close(fd);
            continue;
        }

        if (num > (int)max_num)
            max_num = num;

        close(fd);
    }
    globfree(&g);
    PLUGIN_DEBUG("max sriov_totalvfs = %u", max_num);

    unsigned int mp = NETLINK_VF_DEFAULT_BUF_SIZE_KB;
    /* allign to power of two, buffer size should be at least totalvfs/2 kb */
    while (mp < max_num / 2)
        mp *= 2;

    return mp * 1024;
}

int interface_init(void)
{
    path_proc_dev = plugin_procpath("net/dev");
    if (path_proc_dev == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    path_proc_net_if_inet6 = plugin_procpath("net/if_inet6");
    if (path_proc_net_if_inet6 == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    nl = mnl_socket_open(NETLINK_ROUTE);
    if (nl == NULL) {
        PLUGIN_ERROR("mnl_socket_open failed.");
        return 0;
    } else {
        if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
            PLUGIN_ERROR("mnl_socket_bind failed.");
            mnl_socket_close(nl);
            nl = NULL;
            return 0;
        }
    }

    nl_socket_buffer_size = interface_get_buffer_size();
    PLUGIN_DEBUG("buffer size = %zu", nl_socket_buffer_size);

    return 0;
}

int interface_shutdown(void)
{
    free(path_proc_dev);
    free(path_proc_net_if_inet6);

    exclist_reset(&excl_device);

    if (nl) {
        mnl_socket_close(nl);
        nl = NULL;
    }

    return 0;
}
