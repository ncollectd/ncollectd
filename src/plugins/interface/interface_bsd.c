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
#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>
#endif

/* Darwin/Mac OS X and possible other *BSDs */
#ifdef HAVE_STRUCT_IF_DATA
#define IFA_DATA if_data
#define IFA_RX_BYTES ifi_ibytes
#define IFA_TX_BYTES ifi_obytes
#define IFA_RX_PACKT ifi_ipackets
#define IFA_TX_PACKT ifi_opackets
#define IFA_RX_ERROR ifi_ierrors
#define IFA_TX_ERROR ifi_oerrors
    /* #endif HAVE_STRUCT_IF_DATA */

#elif defined(HAVE_STRUCT_NET_DEVICE_STATS)
#define IFA_DATA net_device_stats
#define IFA_RX_BYTES rx_bytes
#define IFA_TX_BYTES tx_bytes
#define IFA_RX_PACKT rx_packets
#define IFA_TX_PACKT tx_packets
#define IFA_RX_ERROR rx_errors
#define IFA_TX_ERROR tx_errors
#else
#error "No suitable type for `struct ifaddrs->ifa_data' found."
#endif

extern exclist_t excl_device;
extern bool report_inactive;
extern metric_family_t fams[FAM_INTERFACE_MAX];

int interface_read(void)
{
    struct ifaddrs *if_list = NULL;
    if (getifaddrs(&if_list) != 0)
        return -1;

    for (struct ifaddrs *if_ptr = if_list; if_ptr != NULL; if_ptr = if_ptr->ifa_next) {
        if ((if_ptr->ifa_addr == NULL) || (if_ptr->ifa_addr->sa_family != AF_LINK))
            continue;

        if (!exclist_match(&excl_device, if_ptr->ifa_name))
            continue;

        struct IFA_DATA *if_data = (struct IFA_DATA *)if_ptr->ifa_data;
        if (!report_inactive && (if_data->IFA_RX_PACKT == 0) && (if_data->IFA_TX_PACKT == 0))
            continue;

        metric_family_append(&fams[FAM_INTERFACE_RX_BYTES],
                             VALUE_COUNTER(if_data->IFA_RX_BYTES), NULL,
                             &(label_pair_const_t){.name="device", .value=if_ptr->ifa_name}, NULL);
        metric_family_append(&fams[FAM_INTERFACE_RX_PACKETS],
                             VALUE_COUNTER(if_data->IFA_RX_PACKT), NULL,
                             &(label_pair_const_t){.name="device", .value=if_ptr->ifa_name}, NULL);
        metric_family_append(&fams[FAM_INTERFACE_RX_ERRORS],
                             VALUE_COUNTER(if_data->IFA_RX_ERROR), NULL,
                             &(label_pair_const_t){.name="device", .value=if_ptr->ifa_name}, NULL);
        metric_family_append(&fams[FAM_INTERFACE_TX_BYTES],
                             VALUE_COUNTER(if_data->IFA_TX_BYTES), NULL,
                             &(label_pair_const_t){.name="device", .value=if_ptr->ifa_name}, NULL);
        metric_family_append(&fams[FAM_INTERFACE_TX_PACKETS],
                             VALUE_COUNTER(if_data->IFA_TX_PACKT), NULL,
                             &(label_pair_const_t){.name="device", .value=if_ptr->ifa_name}, NULL);
        metric_family_append(&fams[FAM_INTERFACE_TX_ERRORS],
                             VALUE_COUNTER(if_data->IFA_TX_ERROR), NULL,
                             &(label_pair_const_t){.name="device", .value=if_ptr->ifa_name}, NULL);
    }

    freeifaddrs(if_list);

    plugin_dispatch_metric_family_array(fams, FAM_INTERFACE_MAX, 0);

    return 0;
}

int interface_shutdown(void)
{
    exclist_reset(&excl_device);
    return 0;
}
