// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2006-2018 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"
#include "libutils/exclist.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if.h>
#include <linux/wireless.h>
#include <linux/if_ether.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>
#include <linux/nl80211.h>
#include <libmnl/libmnl.h>

static struct mnl_socket *nl;
static uint16_t genl_id_nl80211;
static uint32_t nlmsg_seq;
static exclist_t excl_device;
static plugin_filter_t *filter;

#define ESSID_MAX_SIZE 32
struct wireless_dev_s;
typedef struct wireless_dev_s wireless_dev_t;

typedef struct {
    uint8_t mac[ETH_ALEN];
    double rx_bitrate;
    double tx_bitrate;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_pckts;
    uint64_t tx_pckts;
    uint64_t tx_retries;
    uint64_t tx_failed;
    uint64_t beacon_rx;
    uint64_t beacon_loss;
    uint64_t connected_time;
    double inactive_time;
    int signal_level;
    int quality;
} wireless_station_t;

typedef struct {
    char essid[ESSID_MAX_SIZE + 1];
    uint8_t bssid[ETH_ALEN];
    bool associated;
} wireless_bss_t;

struct wireless_dev_s {
    uint32_t index;
    char name[IF_NAMESIZE];
    double frequency;
    int channel;
    wireless_bss_t bss;
    wireless_dev_t *next;
};

static wireless_dev_t *wireless_dev_list;

enum {
    FAM_WIRELESS_INTERFACE_FREQUENCY_HERTZ,
    FAM_WIRELESS_INTERFACE_CHANNEL,
    FAM_WIRELESS_STATION_CONNECTED_SECONDS,
    FAM_WIRELESS_STATION_INACTIVE_SECONDS,
    FAM_WIRELESS_STATION_RX_BITRATE,
    FAM_WIRELESS_STATION_TX_BITRATE,
    FAM_WIRELESS_STATION_RX_BYTES,
    FAM_WIRELESS_STATION_TX_BYTES,
    FAM_WIRELESS_STATION_TX_PACKETS,
    FAM_WIRELESS_STATION_RX_PACKETS,
    FAM_WIRELESS_STATION_TX_RETRIES,
    FAM_WIRELESS_STATION_TX_FAILED,
    FAM_WIRELESS_STATION_SIGNAL_POWER_DBM,
    FAM_WIRELESS_STATION_SIGNAL_QUALITY,
    FAM_WIRELESS_STATION_BEACON_LOSS,
    FAM_WIRELESS_MAX
};

static metric_family_t fams[FAM_WIRELESS_MAX] = {
    [FAM_WIRELESS_INTERFACE_FREQUENCY_HERTZ] = {
        .name = "system_wireless_interface_frequency_hertz",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current frequency the WiFi interface is operating at, in hertz.",
    },
    [FAM_WIRELESS_INTERFACE_CHANNEL] = {
        .name = "system_wireless_interface_channel",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current Wireless LAN (WLAN) channel the interface is operating at.",
    },
    [FAM_WIRELESS_STATION_CONNECTED_SECONDS] = {
        .name = "system_wireless_station_connected_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of seconds a station has been connected to an access point.",
    },
    [FAM_WIRELESS_STATION_INACTIVE_SECONDS] = {
        .name = "system_wireless_station_inactive_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of seconds since any wireless activity has occurred on a station.",
    },
    [FAM_WIRELESS_STATION_RX_BITRATE] = {
        .name = "system_wireless_station_rx_bitrate",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current WiFi receive bitrate of a station, in bits per second.",
    },
    [FAM_WIRELESS_STATION_TX_BITRATE] = {
        .name = "system_wireless_station_tx_bitrate",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current WiFi transmit bitrate of a station, in bits per second.",
    },
    [FAM_WIRELESS_STATION_RX_BYTES] = {
        .name = "system_wireless_station_rx_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of bytes received by a WiFi station.",
    },
    [FAM_WIRELESS_STATION_TX_BYTES] = {
        .name = "system_wireless_station_tx_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of bytes transmitted by a WiFi station.",
    },
    [FAM_WIRELESS_STATION_TX_PACKETS] = {
        .name = "system_wireless_station_tx_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of packets transmitted by a station.",
    },
    [FAM_WIRELESS_STATION_RX_PACKETS] = {
        .name = "system_wireless_station_rx_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of packets received by a station.",
    },
    [FAM_WIRELESS_STATION_TX_RETRIES] = {
        .name = "system_wireless_station_tx_retries",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of times a station has had to retry while sending a packet.",
    },
    [FAM_WIRELESS_STATION_TX_FAILED] = {
        .name = "system_wireless_station_tx_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of times a station has failed to send a packet.",
    },
    [FAM_WIRELESS_STATION_SIGNAL_POWER_DBM] = {
        .name = "system_wireless_station_signal_dbm",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current WiFi signal strength, in decibel-milliwatts (dBm).",
    },
    [FAM_WIRELESS_STATION_SIGNAL_QUALITY] = {
        .name = "system_wireless_station_quality",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current WiFi signal quality, from 30 (-90 dBm) to 100 (-20 dBm).",
    },
    [FAM_WIRELESS_STATION_BEACON_LOSS] = {
        .name = "system_wireless_station_beacon_loss",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of times a station has detected a beacon loss.",
    }
};

#define NOISE_FLOOR_DBM -90
#define SIGNAL_MAX_DBM -20

// Based on NetworkManager/src/platform/wifi/wifi-utils-nl80211.c
static uint32_t nl80211_xbm_to_percent(int32_t xbm, int32_t divisor)
{
    xbm /= divisor;
    if (xbm < NOISE_FLOOR_DBM)
        xbm = NOISE_FLOOR_DBM;
    if (xbm > SIGNAL_MAX_DBM)
        xbm = SIGNAL_MAX_DBM;

    return 100 - 70 * (((float)SIGNAL_MAX_DBM - (float)xbm) /
                       ((float)SIGNAL_MAX_DBM - (float)NOISE_FLOOR_DBM));
}

// From iw/util.c
static int ieee80211_frequency_to_channel(int freq)
{
    if (freq < 1000)
        return 0;
    /* see 802.11-2007 17.3.8.3.2 and Annex J */
    if (freq == 2484)
        return 14;
    /* see 802.11ax D6.1 27.3.23.2 and Annex E */
    else if (freq == 5935)
        return 2;
    else if (freq < 2484)
        return (freq - 2407) / 5;
    else if (freq >= 4910 && freq <= 4980)
        return (freq - 4000) / 5;
    else if (freq < 5950)
        return (freq - 5000) / 5;
    else if (freq <= 45000) /* DMG band lower limit */
        /* see 802.11ax D6.1 27.3.23.2 */
        return (freq - 5950) / 5;
    else if (freq >= 58320 && freq <= 70200)
        return (freq - 56160) / 2160;
    else
        return 0;
}

static int nlmsg_errno(struct nlmsghdr *nlh, size_t sz)
{
    if (!mnl_nlmsg_ok(nlh, (int)sz)) {
        PLUGIN_ERROR("mnl_nlmsg_ok failed.");
        return EPROTO;
    }

    if (nlh->nlmsg_type != NLMSG_ERROR)
        return 0;

    struct nlmsgerr *nlerr = mnl_nlmsg_get_payload(nlh);
    /* (struct nlmsgerr).error holds a negative errno. */
    return nlerr->error * (-1);
}

static int get_family_id_attr_cb(const struct nlattr *attr, void *data)
{
    uint16_t type = mnl_attr_get_type(attr);
    if (type != CTRL_ATTR_FAMILY_ID)
        return MNL_CB_OK;

    if (mnl_attr_validate(attr, MNL_TYPE_U16) < 0) {
        PLUGIN_ERROR("mnl_attr_validate failed: %s", STRERRNO);
        return MNL_CB_ERROR;
    }

    uint16_t *ret_family_id = data;
    *ret_family_id = mnl_attr_get_u16(attr);
    return MNL_CB_STOP;
}

static int get_family_id_msg_cb(const struct nlmsghdr *nlh, void *data)
{
    return mnl_attr_parse(nlh, sizeof(struct genlmsghdr), get_family_id_attr_cb, data);
}

static int get_family_id(void)
{
    char buffer[MNL_SOCKET_BUFFER_SIZE];
    uint32_t seq = nlmsg_seq++;
    pid_t pid = getpid();

    struct nlmsghdr *nlh = mnl_nlmsg_put_header(buffer);
    *nlh = (struct nlmsghdr){
        .nlmsg_len = nlh->nlmsg_len,
        .nlmsg_type = GENL_ID_CTRL,
        .nlmsg_flags = NLM_F_REQUEST,
        .nlmsg_seq = seq,
        .nlmsg_pid = pid,
    };

    struct genlmsghdr *genh = mnl_nlmsg_put_extra_header(nlh, sizeof(*genh));
    *genh = (struct genlmsghdr){
        .cmd = CTRL_CMD_GETFAMILY,
        .version = 0x01,
    };

    mnl_attr_put_strz(nlh, CTRL_ATTR_FAMILY_NAME, NL80211_GENL_NAME);

    if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
        int status = errno;
        PLUGIN_ERROR("mnl_socket_sendto failed: %s", STRERROR(status));
        return -1;
    }

    genl_id_nl80211 = 0;
    while (true) {
        int status = mnl_socket_recvfrom(nl, buffer, sizeof(buffer));
        if (status < 0) {
            status = errno;
            PLUGIN_ERROR("mnl_socket_recvfrom failed: %s", STRERROR(status));
            return -1;
        } else if (status == 0) {
            break;
        }
        size_t buffer_size = (size_t)status;

        if ((status = nlmsg_errno((void *)buffer, buffer_size)) != 0) {
            PLUGIN_ERROR("CTRL_CMD_GETFAMILY(\"%s\"): %s", NL80211_GENL_NAME, STRERROR(status));
            return -1;
        }

        unsigned int port_id = mnl_socket_get_portid(nl);

        status = mnl_cb_run(buffer, buffer_size, seq, port_id,
                            get_family_id_msg_cb, &genl_id_nl80211);
        if (status < MNL_CB_STOP) {
            PLUGIN_ERROR("Parsing message failed.");
            return -1;
        } else if (status == MNL_CB_STOP) {
            break;
        }
    }

    if (genl_id_nl80211 == 0) {
        PLUGIN_ERROR("Netlink communication succeeded, but genl_id_nl80211 is still zero.");
        return -1;
    }

    return 0;
}

static int nl80211_cmd_get_interfaces_attr_cb(const struct nlattr *attr, void *data)
{
    wireless_dev_t *wdev = data;

    switch (mnl_attr_get_type(attr)) {
    case NL80211_ATTR_IFNAME:
        sstrncpy(wdev->name, mnl_attr_get_str(attr), sizeof(wdev->name));
        break;
    case NL80211_ATTR_IFINDEX:
        wdev->index = mnl_attr_get_u32(attr);
        break;
    case NL80211_ATTR_WIPHY_FREQ: {
        uint32_t frequency = mnl_attr_get_u32(attr);
        wdev->channel = ieee80211_frequency_to_channel(frequency);
        wdev->frequency = (double)frequency * 1e6;
    }   break;
    case NL80211_ATTR_SSID: {
        char essid[ESSID_MAX_SIZE + 1];
        size_t len = mnl_attr_get_payload_len(attr);
        if (len > (sizeof(essid) - 1))
            len = sizeof(essid) - 1;
        char *playload = mnl_attr_get_payload(attr);
        memcpy(essid, playload, len);
        essid[len] = '\0';
    }   break;
    }

    return MNL_CB_OK;
}

static int nl80211_cmd_get_interfaces_msg_cb(const struct nlmsghdr *nlh,
                                             __attribute__((unused)) void *data)
{
    wireless_dev_t *wdev = calloc(1, sizeof(*wdev));
    if (wdev == NULL) {
        PLUGIN_ERROR("malloc failed.");
        return MNL_CB_ERROR;
    }
    wdev->next = wireless_dev_list;
    wireless_dev_list = wdev;

    return mnl_attr_parse(nlh, sizeof(struct genlmsghdr), nl80211_cmd_get_interfaces_attr_cb, wdev);
}

static int nl80211_cmd_get_interfaces(void)
{
    char buffer[MNL_SOCKET_BUFFER_SIZE];
    uint32_t seq = nlmsg_seq++;
    pid_t pid = getpid();

    struct nlmsghdr *nlh = mnl_nlmsg_put_header(buffer);
    *nlh = (struct nlmsghdr){
        .nlmsg_len = nlh->nlmsg_len,
        .nlmsg_type = genl_id_nl80211,
        .nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_DUMP,
        .nlmsg_seq = seq,
        .nlmsg_pid = pid,
    };

    struct genlmsghdr *genl = mnl_nlmsg_put_extra_header(nlh, sizeof(struct genlmsghdr));
    genl->cmd = NL80211_CMD_GET_INTERFACE;
    genl->version = 1;

    if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
        int status = errno;
        PLUGIN_ERROR("mnl_socket_sendto failed: %s", STRERROR(status));
        return -1;
    }

    while (true) {
        int status = mnl_socket_recvfrom(nl, buffer, sizeof(buffer));
        if (status < 0) {
            status = errno;
            PLUGIN_ERROR("mnl_socket_recvfrom failed: %s", STRERROR(status));
            return -1;
        } else if (status == 0) {
            break;
        }
        size_t buffer_size = (size_t)status;

        status = nlmsg_errno((void *)buffer, buffer_size);
        if (status != 0) {
            PLUGIN_ERROR("NL80211_CMD_GET_INTERFACE failed: %s", STRERROR(status));
            return -1;
        }

        unsigned int port_id = mnl_socket_get_portid(nl);

        status = mnl_cb_run(buffer, buffer_size, seq, port_id,
                            nl80211_cmd_get_interfaces_msg_cb, NULL);
        if (status < MNL_CB_STOP) {
            PLUGIN_ERROR("Parsing message failed.");
            return -1;
        } else if (status == MNL_CB_STOP) {
            break;
        }
    }

    return 0;
}

static uint8_t *find_ie(uint8_t *buf, size_t len, uint8_t ie)
{
    while (len >= 2) {
        if (len < (size_t)(2+buf[1]))
            break;
        if (buf[0] == ie)
            return buf;
        buf += buf[1]+2;
        len -= buf[1]+2;
    }

    return NULL;
}

static int nl80211_cmd_get_scan_attr_cb(const struct nlattr *attr, void *data)
{
    wireless_bss_t *wbss = data;

    switch(mnl_attr_get_type(attr)) {
    case NL80211_ATTR_BSS: {
        if (mnl_attr_validate(attr, MNL_TYPE_NESTED) < 0) {
            PLUGIN_ERROR("mnl_attr_validate(NL80211_ATTR_BSS) failed.");
            return MNL_CB_ERROR;
        }

        struct nlattr *nested;
        mnl_attr_for_each_nested(nested, attr) {
            switch (mnl_attr_get_type(nested)) {
            case NL80211_BSS_STATUS: {
                uint32_t status = mnl_attr_get_u32(nested);
                if (status == NL80211_BSS_STATUS_ASSOCIATED ||
                    status == NL80211_BSS_STATUS_AUTHENTICATED ||
                    status == NL80211_BSS_STATUS_IBSS_JOINED) {
                    wbss->associated = true;
                }
            }   break;
            case NL80211_BSS_BSSID: {
                uint8_t *bss_payload = mnl_attr_get_payload(nested);
                if (bss_payload != NULL)
                    memcpy(wbss->bssid, bss_payload, sizeof(wbss->bssid));
            }   break;
            case NL80211_BSS_INFORMATION_ELEMENTS: {
                uint16_t payload_len = mnl_attr_get_payload_len(nested);
                uint8_t *payload = mnl_attr_get_payload(nested);
                uint8_t *ie = find_ie(payload,payload_len,0);
                if (ie) {
                    uint8_t l = ie[1];
                    if (l >= sizeof(wbss->essid))
                        l = sizeof(wbss->essid)-1;
                    memcpy(wbss->essid, ie+2, l);
                    wbss->essid[l] = 0;
                }
            }   break;
            }
        }
    }   break;
    }

    return MNL_CB_OK;
}

static int nl80211_cmd_get_scan_msg_cb(const struct nlmsghdr *nlh, void *data)
{
    wireless_dev_t *wdev = data;

    if (wdev->bss.associated)
        return MNL_CB_OK;

    memset(&wdev->bss, 0, sizeof(wdev->bss));

    return mnl_attr_parse(nlh, sizeof(struct genlmsghdr),
                               nl80211_cmd_get_scan_attr_cb, &wdev->bss);
}

static int nl80211_cmd_get_scan(wireless_dev_t *wdev)
{
    char buffer[MNL_SOCKET_BUFFER_SIZE];
    uint32_t seq = nlmsg_seq++;

    pid_t pid = getpid();

    struct nlmsghdr *nlh = mnl_nlmsg_put_header(buffer);
    *nlh = (struct nlmsghdr){
        .nlmsg_len = nlh->nlmsg_len,
        .nlmsg_type = genl_id_nl80211,
        .nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP | NLM_F_ACK,
        .nlmsg_seq = seq,
        .nlmsg_pid = pid,
    };

    struct genlmsghdr *genl = mnl_nlmsg_put_extra_header(nlh, sizeof(struct genlmsghdr));
    genl->cmd = NL80211_CMD_GET_SCAN;
    genl->version = 1;

    mnl_attr_put_u32(nlh, NL80211_ATTR_IFINDEX, wdev->index);

    if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
        int status = errno;
        PLUGIN_ERROR("mnl_socket_sendto failed: %s", STRERROR(status));
        return -1;
    }

    while (true) {
        int status = mnl_socket_recvfrom(nl, buffer, sizeof(buffer));
        if (status < 0) {
            status = errno;
            PLUGIN_ERROR("mnl_socket_recvfrom failed: %s", STRERROR(status));
            return -1;
        } else if (status == 0) {
            break;
        }
        size_t buffer_size = (size_t)status;

        status = nlmsg_errno((void *)buffer, buffer_size);
        if (status != 0) {
            PLUGIN_ERROR("NL80211_CMD_GET_SCAN failed: %s", STRERROR(status));
            return -1;
        }

        unsigned int port_id = mnl_socket_get_portid(nl);

        status = mnl_cb_run(buffer, buffer_size, seq, port_id,
                            nl80211_cmd_get_scan_msg_cb, wdev);
        if (status < MNL_CB_STOP) {
            PLUGIN_ERROR("Parsing message failed.");
            return -1;
        } else if (status == MNL_CB_STOP) {
            break;
        }
    }

    return 0;
}

static int nl80211_cmd_get_station_attr_cb(const struct nlattr *attr, void *data)
{
    wireless_station_t *wst = data;

    switch (mnl_attr_get_type(attr)) {
    case NL80211_ATTR_MAC: {
         uint8_t *mac_payload = mnl_attr_get_payload(attr);
         if (mac_payload != NULL)
            memcpy(wst->mac, mac_payload, sizeof(wst->mac));
    }   break;
    case NL80211_ATTR_STA_INFO:
        if (mnl_attr_validate(attr, MNL_TYPE_NESTED) < 0) {
            PLUGIN_ERROR("mnl_attr_validate(NL80211_ATTR_STA_INFO) failed.");
            return MNL_CB_ERROR;
        }
        struct nlattr *attr_sta_info_rx_bitrate = NULL;
        struct nlattr *attr_sta_info_tx_bitrate = NULL;
        struct nlattr *nested;
        mnl_attr_for_each_nested(nested, attr) {
            switch (mnl_attr_get_type(nested)) {
            case NL80211_STA_INFO_RX_BITRATE:
                attr_sta_info_rx_bitrate = nested;
                break;
            case NL80211_STA_INFO_TX_BITRATE:
                attr_sta_info_tx_bitrate = nested;
                break;
            case NL80211_STA_INFO_RX_BYTES64:
                wst->rx_bytes = mnl_attr_get_u64(nested);
                break;
            case NL80211_STA_INFO_TX_BYTES64:
                wst->tx_bytes = mnl_attr_get_u64(nested);
                break;
            case NL80211_STA_INFO_RX_PACKETS:
                wst->rx_pckts = mnl_attr_get_u32(nested);
                break;
            case NL80211_STA_INFO_TX_PACKETS:
                wst->tx_pckts = mnl_attr_get_u32(nested);
                break;
            case NL80211_STA_INFO_TX_RETRIES:
                wst->tx_retries = mnl_attr_get_u32(nested);
                break;
            case NL80211_STA_INFO_TX_FAILED:
                wst->tx_failed = mnl_attr_get_u32(nested);
                break;
            case NL80211_STA_INFO_BEACON_RX:
                wst->beacon_rx =  mnl_attr_get_u64(nested);
                break;
            case NL80211_STA_INFO_BEACON_LOSS:
                wst->beacon_loss = mnl_attr_get_u32(nested);
                break;
            case NL80211_STA_INFO_CONNECTED_TIME:
                wst->connected_time = mnl_attr_get_u32(nested);
                break;
            case NL80211_STA_INFO_INACTIVE_TIME:
                wst->inactive_time = (double)mnl_attr_get_u32(nested) / 1000.0;
                break;
            case NL80211_STA_INFO_SIGNAL: {
                int8_t signal = mnl_attr_get_u8(nested);
                wst->signal_level = signal;
                wst->quality = nl80211_xbm_to_percent(signal, 1);
            }   break;
            }
        }

        if (attr_sta_info_rx_bitrate != NULL) {
            if (mnl_attr_validate(attr_sta_info_rx_bitrate, MNL_TYPE_NESTED) < 0) {
                PLUGIN_ERROR("mnl_attr_validate(NL80211_STA_INFO_RX_BITRATE) failed.");
                return MNL_CB_ERROR;
            }

            mnl_attr_for_each_nested(nested, attr_sta_info_rx_bitrate) {
                switch (mnl_attr_get_type(nested)) {
                case NL80211_RATE_INFO_BITRATE:
                    wst->rx_bitrate = (double)mnl_attr_get_u16(nested) * 100.0 * 1000.0;
                    break;
                }
            }
        }

        if (attr_sta_info_tx_bitrate != NULL) {
            if (mnl_attr_validate(attr_sta_info_tx_bitrate, MNL_TYPE_NESTED) < 0) {
                PLUGIN_ERROR("mnl_attr_validate(NL80211_STA_INFO_TX_BITRATE) failed.");
                return MNL_CB_ERROR;
            }

            mnl_attr_for_each_nested(nested, attr_sta_info_tx_bitrate) {
                switch (mnl_attr_get_type(nested)) {
                case NL80211_RATE_INFO_BITRATE:
                    wst->tx_bitrate = (double)mnl_attr_get_u16(nested) * 100.0 * 1000.0;
                    break;
                }
            }
        }

        break;
    }

    return MNL_CB_OK;
}

static char *mac2str(uint8_t mac[ETH_ALEN], char smac[ETH_ALEN*3+1])
{
    strbuf_t buf = STRBUF_CREATE_FIXED(smac, ETH_ALEN*3+1);
    for (size_t i = 0; i < ETH_ALEN; i++) {
        if (i != 0)
            strbuf_putchar(&buf, ':');
        strbuf_puthex(&buf, mac[i]);
    }

    return smac;
}

static int nl80211_cmd_get_station_msg_cb(const struct nlmsghdr *nlh, void *data)
{
    wireless_dev_t *wdev = data;
    wireless_station_t wst = {0};

    int status = mnl_attr_parse(nlh, sizeof(struct genlmsghdr),
                                     nl80211_cmd_get_station_attr_cb, &wst);
    if (status != MNL_CB_OK)
        return status;

    if (memcmp(wst.mac, wdev->bss.bssid, sizeof(wst.mac)) != 0)
        return MNL_CB_OK;

    char mac[ETH_ALEN*3+1];

    label_set_t labels = {
        .num = 2,
        .ptr = (label_pair_t[]){
            {.name = "bssid",  .value = mac2str(wst.mac, mac) },
            {.name = "device", .value = wdev->name            },
            {.name = "ssid",   .value = wdev->bss.essid       }
        }
    };

    metric_family_append(&fams[FAM_WIRELESS_STATION_CONNECTED_SECONDS],
                         VALUE_COUNTER(wst.connected_time), &labels, NULL);
    metric_family_append(&fams[FAM_WIRELESS_STATION_INACTIVE_SECONDS],
                         VALUE_GAUGE(wst.inactive_time), &labels, NULL);
    metric_family_append(&fams[FAM_WIRELESS_STATION_RX_BITRATE],
                         VALUE_GAUGE(wst.rx_bitrate), &labels, NULL);
    metric_family_append(&fams[FAM_WIRELESS_STATION_TX_BITRATE],
                         VALUE_GAUGE(wst.tx_bitrate), &labels, NULL);
    metric_family_append(&fams[FAM_WIRELESS_STATION_RX_BYTES],
                         VALUE_COUNTER(wst.rx_bytes), &labels, NULL);
    metric_family_append(&fams[FAM_WIRELESS_STATION_TX_BYTES],
                         VALUE_COUNTER(wst.tx_bytes), &labels, NULL);
    metric_family_append(&fams[FAM_WIRELESS_STATION_RX_PACKETS],
                         VALUE_COUNTER(wst.rx_pckts), &labels, NULL);
    metric_family_append(&fams[FAM_WIRELESS_STATION_TX_PACKETS],
                         VALUE_COUNTER(wst.tx_pckts), &labels, NULL);
    metric_family_append(&fams[FAM_WIRELESS_STATION_TX_RETRIES],
                         VALUE_COUNTER(wst.tx_retries), &labels, NULL);
    metric_family_append(&fams[FAM_WIRELESS_STATION_TX_FAILED],
                         VALUE_COUNTER(wst.tx_failed), &labels, NULL);
    metric_family_append(&fams[FAM_WIRELESS_STATION_SIGNAL_POWER_DBM],
                         VALUE_GAUGE(wst.signal_level), &labels, NULL);
    metric_family_append(&fams[FAM_WIRELESS_STATION_SIGNAL_QUALITY],
                         VALUE_GAUGE(wst.quality), &labels, NULL);
    metric_family_append(&fams[FAM_WIRELESS_STATION_BEACON_LOSS],
                         VALUE_COUNTER(wst.beacon_loss), &labels, NULL);

    return MNL_CB_OK;
}

static int nl80211_cmd_get_station(wireless_dev_t *wdev)
{
    char buffer[MNL_SOCKET_BUFFER_SIZE];
    uint32_t seq = nlmsg_seq++;

    pid_t pid = getpid();

    struct nlmsghdr *nlh = mnl_nlmsg_put_header(buffer);
    *nlh = (struct nlmsghdr){
        .nlmsg_len = nlh->nlmsg_len,
        .nlmsg_type = genl_id_nl80211,
        .nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP | NLM_F_ACK,
        .nlmsg_seq = seq,
        .nlmsg_pid = pid,
    };

    struct genlmsghdr *genl = mnl_nlmsg_put_extra_header(nlh, sizeof(struct genlmsghdr));
    genl->cmd = NL80211_CMD_GET_STATION;
    genl->version = 1;

    mnl_attr_put_u32(nlh, NL80211_ATTR_IFINDEX, wdev->index);
    mnl_attr_put(nlh, NL80211_ATTR_MAC, ETH_ALEN, wdev->bss.bssid);

    if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
        int status = errno;
        PLUGIN_ERROR("mnl_socket_sendto failed: %s", STRERROR(status));
        return -1;
    }

    while (true) {
        int status = mnl_socket_recvfrom(nl, buffer, sizeof(buffer));
        if (status < 0) {
            status = errno;
            PLUGIN_ERROR("mnl_socket_recvfrom failed: %s", STRERROR(status));
            return -1;
        } else if (status == 0) {
            break;
        }
        size_t buffer_size = (size_t)status;

        status = nlmsg_errno((void *)buffer, buffer_size);
        if (status != 0) {
            PLUGIN_ERROR("NL80211_CMD_GET_STATION failed: %s", STRERROR(status));
            return -1;
        }

        unsigned int port_id = mnl_socket_get_portid(nl);

        status = mnl_cb_run(buffer, buffer_size, seq, port_id,
                            nl80211_cmd_get_station_msg_cb, wdev);
        if (status < MNL_CB_STOP) {
            PLUGIN_ERROR("Parsing message failed.");
            return -1;
        } else if (status == MNL_CB_STOP) {
            break;
        }
    }

    return 0;
}

static int wireless_read(void)
{
    if (nl == NULL)
        return -1;

    wireless_dev_t *wdev = wireless_dev_list;
    while (wdev != NULL) {
        wireless_dev_t *next = wdev->next;
        free(wdev);
        wdev = next;
    }
    wireless_dev_list = NULL;

    nl80211_cmd_get_interfaces();

    for (wdev = wireless_dev_list; wdev != NULL; wdev = wdev->next) {
        if (wdev->index == 0)
            continue;
        if (wdev->name[0] == '\0')
            continue;
        if (!exclist_match(&excl_device, wdev->name))
            continue;

        nl80211_cmd_get_scan(wdev);

        if (!wdev->bss.associated)
            continue;

        nl80211_cmd_get_station(wdev);

        metric_family_append(&fams[FAM_WIRELESS_INTERFACE_FREQUENCY_HERTZ],
                             VALUE_GAUGE(wdev->frequency), NULL,
                             &LABEL_PAIR_CONST("device", wdev->name), NULL);
        metric_family_append(&fams[FAM_WIRELESS_INTERFACE_CHANNEL],
                             VALUE_GAUGE(wdev->channel), NULL,
                             &LABEL_PAIR_CONST("device", wdev->name), NULL);
    }

    plugin_dispatch_metric_family_array_filtered(fams, FAM_WIRELESS_MAX, filter, 0);

    return 0;
}

static int wireless_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "device") == 0) {
            status = cf_util_exclist(child, &excl_device);
        } else if (strcasecmp(child->key, "filter") == 0) {
            status = plugin_filter_configure(child, &filter);
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

static int wireless_init(void)
{
    nl = mnl_socket_open(NETLINK_GENERIC);
    if (nl == NULL) {
        PLUGIN_ERROR("mnl_socket_open failed.");
        return -1;
    }

    int status = mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID);
    if (status < 0) {
        PLUGIN_ERROR("mnl_socket_bind failed.");
        mnl_socket_close(nl);
        nl = NULL;
        return -1;
    }

    status = get_family_id();
    if (status != 0) {
        mnl_socket_close(nl);
        nl = NULL;
        return -1;
    }

    return 0;
}

static int wireless_shutdown(void)
{
    if (nl != NULL)
        mnl_socket_close(nl);

    wireless_dev_t *wdev = wireless_dev_list;
    while (wdev != NULL) {
        wireless_dev_t *next = wdev->next;
        free(wdev);
        wdev = next;
    }
    wireless_dev_list = NULL;

    exclist_reset(&excl_device);

    plugin_filter_free(filter);

    return 0;
}

void module_register(void)
{
    plugin_register_init("wireless", wireless_init);
    plugin_register_config("wireless", wireless_config);
    plugin_register_read("wireless", wireless_read);
    plugin_register_shutdown("wireless", wireless_shutdown);
}
