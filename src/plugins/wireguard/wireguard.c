// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2015-2020 Jason A. Donenfeld
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Jason A. Donenfeld <Jason at zx2c4.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

// Based on the wireguard-tools

#include "plugin.h"
#include "libutils/common.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

#include <libmnl/libmnl.h>

#include <linux/wireguard.h>
#include <linux/if.h>
#include <linux/genetlink.h>
#include <linux/rtnetlink.h>

#define SOCKET_BUFFER_SIZE 8192
#define WG_KEY_LEN 32
#define WG_KEY_LEN_BASE64 ((((WG_KEY_LEN) + 2) / 3) * 4 + 1)

enum {
    FAM_WIREGUARD_SENT_BYTES,
    FAM_WIREGUARD_RECEIVED_BYTES,
    FAM_WIREGUARD_LATEST_HANDSHAKE_SECONDS,
    FAM_WIREGUARD_MAX,
};

static metric_family_t fams[FAM_WIREGUARD_MAX] = {
    [FAM_WIREGUARD_SENT_BYTES] = {
        .name = "system_wireguard_sent_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Bytes sent to the peer.",
    },
    [FAM_WIREGUARD_RECEIVED_BYTES] = {
        .name = "system_wireguard_received_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Bytes received from the peer.",
    },
    [FAM_WIREGUARD_LATEST_HANDSHAKE_SECONDS] = {
        .name = "system_wireguard_latest_handshake_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Seconds from the last handshake.",
    },
};

enum {
    WGPEER_HAS_ENDPOINT            = 1U << 1,
    WGPEER_HAS_PUBLIC_KEY          = 1U << 2,
    WGPEER_HAS_RX_BYTES            = 1U << 3,
    WGPEER_HAS_TX_BYTES            = 1U << 4,
    WGPEER_HAS_LAST_HANDSHAKE_TIME = 1U << 5
};

struct timespec64 {
    int64_t tv_sec;
    int64_t tv_nsec;
};

struct mnlg_socket {
    struct mnl_socket *nl;
    char *buf;
    uint16_t id;
    uint8_t version;
    unsigned int seq;
    unsigned int portid;
};

struct interface {
    const char *name;
    bool is_wireguard;
};

struct wgdevice {
    char name[IFNAMSIZ];
    uint32_t ifindex;
    uint16_t listen_port;
};

typedef struct wgpeer {
    uint32_t flags;
    uint8_t public_key[WG_KEY_LEN];
    union {
        struct sockaddr addr;
        struct sockaddr_in addr4;
        struct sockaddr_in6 addr6;
    } endpoint;
    struct timespec64 last_handshake_time;
    uint64_t rx_bytes, tx_bytes;
} wgpeer_t;

static inline void encode_base64(char dest[static 4], const uint8_t src[static 3])
{
    const uint8_t input[] = { (src[0] >> 2) & 63, ((src[0] << 4) | (src[1] >> 4)) & 63,
                              ((src[1] << 2) | (src[2] >> 6)) & 63, src[2] & 63 };

    for (unsigned int i = 0; i < 4; ++i)
        dest[i] = input[i] + 'A'
                           + (((25 - input[i]) >> 8) & 6)
                           - (((51 - input[i]) >> 8) & 75)
                           - (((61 - input[i]) >> 8) & 15)
                           + (((62 - input[i]) >> 8) & 3);
}

static void key_to_base64(char base64[static WG_KEY_LEN_BASE64],
                          const uint8_t key[static WG_KEY_LEN])
{
    unsigned int i;

    for (i = 0; i < WG_KEY_LEN / 3; ++i)
        encode_base64(&base64[i * 4], &key[i * 3]);
    encode_base64(&base64[i * 4], (const uint8_t[]){ key[i * 3 + 0], key[i * 3 + 1], 0 });
    base64[WG_KEY_LEN_BASE64 - 2] = '=';
    base64[WG_KEY_LEN_BASE64 - 1] = '\0';
}

static int mnlg_cb_noop(const struct nlmsghdr *nlh, void *data)
{
    (void)nlh;
    (void)data;
    return MNL_CB_OK;
}

static int mnlg_cb_error(const struct nlmsghdr *nlh, void *data)
{
    const struct nlmsgerr *err = mnl_nlmsg_get_payload(nlh);
    (void)data;

    if (nlh->nlmsg_len < mnl_nlmsg_size(sizeof(struct nlmsgerr))) {
        errno = EBADMSG;
        return MNL_CB_ERROR;
    }
    /* Netlink subsystems returns the errno value with different signess */
    if (err->error < 0)
        errno = -err->error;
    else
        errno = err->error;

    return err->error == 0 ? MNL_CB_STOP : MNL_CB_ERROR;
}

static int mnlg_cb_stop(const struct nlmsghdr *nlh, void *data)
{
    (void)data;
    if (nlh->nlmsg_flags & NLM_F_MULTI && nlh->nlmsg_len == mnl_nlmsg_size(sizeof(int))) {
        int error = *(int *)mnl_nlmsg_get_payload(nlh);
        /* Netlink subsystems returns the errno value with different signess */
        if (error < 0)
            errno = -error;
        else
            errno = error;
        return error == 0 ? MNL_CB_STOP : MNL_CB_ERROR;
    }
    return MNL_CB_STOP;
}

static mnl_cb_t mnlg_cb_array[] = {
    [NLMSG_NOOP]    = mnlg_cb_noop,
    [NLMSG_ERROR]   = mnlg_cb_error,
    [NLMSG_DONE]    = mnlg_cb_stop,
    [NLMSG_OVERRUN] = mnlg_cb_noop,
};

static void mnlg_socket_close(struct mnlg_socket *nlg)
{
    mnl_socket_close(nlg->nl);
}

static struct nlmsghdr *mnlg_msg_prepare(struct mnlg_socket *nlg, uint8_t cmd,
                                         uint16_t flags, uint16_t id, uint8_t version)
{
    struct nlmsghdr *nlh;
    struct genlmsghdr *genl;

    nlh = mnl_nlmsg_put_header(nlg->buf);
    nlh->nlmsg_type = id;
    nlh->nlmsg_flags = flags;
    /* coverity[Y2K38_SAFETY] */
    nlg->seq = time(NULL);
    nlh->nlmsg_seq = nlg->seq;

    genl = mnl_nlmsg_put_extra_header(nlh, sizeof(struct genlmsghdr));
    genl->cmd = cmd;
    genl->version = version;

    return nlh;
}

static int mnlg_socket_recv_run(struct mnlg_socket *nlg, mnl_cb_t data_cb, void *data)
{
    int err;

    do {
        err = mnl_socket_recvfrom(nlg->nl, nlg->buf, SOCKET_BUFFER_SIZE);
        if (err <= 0)
            break;
        err = mnl_cb_run2(nlg->buf, err, nlg->seq, nlg->portid,
                          data_cb, data, mnlg_cb_array, MNL_ARRAY_SIZE(mnlg_cb_array));
    } while (err > 0);

    return err;
}

static int mnlg_socket_send(struct mnlg_socket *nlg, const struct nlmsghdr *nlh)
{
    return mnl_socket_sendto(nlg->nl, nlh, nlh->nlmsg_len);
}

static int get_family_id_attr_cb(const struct nlattr *attr, void *data)
{
    const struct nlattr **tb = data;
    int type = mnl_attr_get_type(attr);

    if (mnl_attr_type_valid(attr, CTRL_ATTR_MAX) < 0)
        return MNL_CB_ERROR;

    if (type == CTRL_ATTR_FAMILY_ID && mnl_attr_validate(attr, MNL_TYPE_U16) < 0)
        return MNL_CB_ERROR;

    tb[type] = attr;
    return MNL_CB_OK;
}

static int get_family_id_cb(const struct nlmsghdr *nlh, void *data)
{
    uint16_t *p_id = data;
    struct nlattr *tb[CTRL_ATTR_MAX + 1] = { 0 };

    mnl_attr_parse(nlh, sizeof(struct genlmsghdr), get_family_id_attr_cb, tb);
    if (!tb[CTRL_ATTR_FAMILY_ID])
        return MNL_CB_ERROR;
    *p_id = mnl_attr_get_u16(tb[CTRL_ATTR_FAMILY_ID]);
    return MNL_CB_OK;
}

static int mnlg_socket_open(struct mnlg_socket *nlg, const char *family_name, uint8_t version)
{
    struct nlmsghdr *nlh;
    int ret;

    nlg->id = 0;
    nlg->nl = mnl_socket_open(NETLINK_GENERIC);
    if (!nlg->nl)
        return -errno;

    if (mnl_socket_bind(nlg->nl, 0, MNL_SOCKET_AUTOPID) < 0) {
        ret = -errno;
        mnl_socket_close(nlg->nl);
        return ret;
    }

    nlg->portid = mnl_socket_get_portid(nlg->nl);

    nlh = mnlg_msg_prepare(nlg, CTRL_CMD_GETFAMILY, NLM_F_REQUEST | NLM_F_ACK, GENL_ID_CTRL, 1);
    mnl_attr_put_strz(nlh, CTRL_ATTR_FAMILY_NAME, family_name);

    if (mnlg_socket_send(nlg, nlh) < 0) {
        ret = -errno;
        mnl_socket_close(nlg->nl);
        return ret;
    }

    errno = 0;
    if (mnlg_socket_recv_run(nlg, get_family_id_cb, &nlg->id) < 0) {
        errno = errno == ENOENT ? EPROTONOSUPPORT : errno;
        ret = errno ? -errno : -ENOSYS;
        mnl_socket_close(nlg->nl);
        return ret;
    }

    nlg->version = version;
    return 0;
}

static int parse_peer(const struct nlattr *attr, void *data)
{
    struct wgpeer *peer = data;

    switch (mnl_attr_get_type(attr)) {
    case WGPEER_A_PUBLIC_KEY:
        if (mnl_attr_get_payload_len(attr) == sizeof(peer->public_key)) {
            memcpy(peer->public_key, mnl_attr_get_payload(attr), sizeof(peer->public_key));
            peer->flags |= WGPEER_HAS_PUBLIC_KEY;
        }
        break;
    case WGPEER_A_ENDPOINT: {
        struct sockaddr *addr;

        if (mnl_attr_get_payload_len(attr) < sizeof(*addr))
            break;
        addr = mnl_attr_get_payload(attr);
        if ((addr->sa_family == AF_INET) &&
            (mnl_attr_get_payload_len(attr) == sizeof(peer->endpoint.addr4)))
            memcpy(&peer->endpoint.addr4, addr, sizeof(peer->endpoint.addr4));
        else if ((addr->sa_family == AF_INET6) &&
                 (mnl_attr_get_payload_len(attr) == sizeof(peer->endpoint.addr6)))
             memcpy(&peer->endpoint.addr6, addr, sizeof(peer->endpoint.addr6));

        peer->flags |= WGPEER_HAS_ENDPOINT;
        break;
    }
    case WGPEER_A_LAST_HANDSHAKE_TIME:
        if (mnl_attr_get_payload_len(attr) == sizeof(peer->last_handshake_time)) {
            memcpy(&peer->last_handshake_time, mnl_attr_get_payload(attr),
                                               sizeof(peer->last_handshake_time));
            peer->flags |= WGPEER_HAS_LAST_HANDSHAKE_TIME;
        }
        break;
    case WGPEER_A_RX_BYTES:
        if (!mnl_attr_validate(attr, MNL_TYPE_U64)) {
            peer->rx_bytes = mnl_attr_get_u64(attr);
            peer->flags |= WGPEER_HAS_RX_BYTES;
        }
        break;
    case WGPEER_A_TX_BYTES:
        if (!mnl_attr_validate(attr, MNL_TYPE_U64)) {
            peer->tx_bytes = mnl_attr_get_u64(attr);
            peer->flags |= WGPEER_HAS_TX_BYTES;
        }
        break;
    case WGPEER_A_ALLOWEDIPS:
        break;
    }

    return MNL_CB_OK;
}

static int parse_peers(const struct nlattr *attr, void *data)
{
    struct wgdevice *device = data;
    struct wgpeer peer = {0};

    int ret = mnl_attr_parse_nested(attr, parse_peer, &peer);
    if (!ret)
        return ret;

    if (!(peer.flags & WGPEER_HAS_PUBLIC_KEY))
        return MNL_CB_ERROR;

    if (!(peer.flags & WGPEER_HAS_ENDPOINT))
        return MNL_CB_ERROR;

    metric_t m = {0};
    metric_label_set(&m, "interface", device->name);

    if ((peer.endpoint.addr.sa_family == AF_INET) || (peer.endpoint.addr.sa_family == AF_INET6)) {
        char buffer[64];

        const char *name = NULL;
        if (peer.endpoint.addr.sa_family == AF_INET)
            name = inet_ntop(AF_INET, &(peer.endpoint.addr4), buffer, sizeof(buffer));
        else if (peer.endpoint.addr.sa_family == AF_INET6)
            name = inet_ntop(AF_INET6, &(peer.endpoint.addr6), buffer, sizeof(buffer));
        if (name != NULL)
            metric_label_set(&m, "remote_ip", buffer);

        buffer[0] = '\0';
        if (peer.endpoint.addr.sa_family == AF_INET)
            ssnprintf(buffer, sizeof(buffer), "%uh", htons(peer.endpoint.addr4.sin_port));
        else if (peer.endpoint.addr.sa_family == AF_INET6)
            ssnprintf(buffer, sizeof(buffer), "%uh", htons(peer.endpoint.addr6.sin6_port));
        metric_label_set(&m, "remote_port", buffer);
    }

    char base64[WG_KEY_LEN_BASE64];
    key_to_base64(base64, peer.public_key);
    metric_label_set(&m, "public_key", base64);

    if (peer.flags & WGPEER_HAS_TX_BYTES) {
        m.value = VALUE_COUNTER(peer.tx_bytes);
        metric_family_metric_append(&fams[FAM_WIREGUARD_SENT_BYTES], m);
    }

    if (peer.flags & WGPEER_HAS_RX_BYTES) {
        m.value = VALUE_COUNTER(peer.rx_bytes);
        metric_family_metric_append(&fams[FAM_WIREGUARD_RECEIVED_BYTES], m);
    }

    if (peer.flags & WGPEER_HAS_LAST_HANDSHAKE_TIME) {
        m.value = VALUE_GAUGE((double)peer.last_handshake_time.tv_sec +
                              (double)peer.last_handshake_time.tv_nsec / 1000000000L);
        metric_family_metric_append(&fams[FAM_WIREGUARD_LATEST_HANDSHAKE_SECONDS], m);
    }

    metric_reset(&m, METRIC_TYPE_GAUGE);

    return MNL_CB_OK;
}

static int parse_device(const struct nlattr *attr, void *data)
{
    struct wgdevice *device = data;

    switch (mnl_attr_get_type(attr)) {
        case WGDEVICE_A_IFINDEX:
            if (!mnl_attr_validate(attr, MNL_TYPE_U32))
                device->ifindex = mnl_attr_get_u32(attr);
            break;
        case WGDEVICE_A_IFNAME:
            if (!mnl_attr_validate(attr, MNL_TYPE_STRING)) {
                strncpy(device->name, mnl_attr_get_str(attr), sizeof(device->name) - 1);
                device->name[sizeof(device->name) - 1] = '\0';
            }
            break;
        case WGDEVICE_A_LISTEN_PORT:
            if (!mnl_attr_validate(attr, MNL_TYPE_U16))
                device->listen_port = mnl_attr_get_u16(attr);
            break;
        case WGDEVICE_A_PEERS:
            return mnl_attr_parse_nested(attr, parse_peers, device);
    }

    return MNL_CB_OK;
}

static int read_device_cb(const struct nlmsghdr *nlh, void *data)
{
    return mnl_attr_parse(nlh, sizeof(struct genlmsghdr), parse_device, data);
}

static int kernel_get_device(const char *iface)
{
    struct wgdevice device = {0};
    char mnlg_buffer[SOCKET_BUFFER_SIZE];
    struct mnlg_socket nlg = {0};
    nlg.buf = mnlg_buffer;

try_again:;
    int ret = 0;

    ret = mnlg_socket_open(&nlg, WG_GENL_NAME, WG_GENL_VERSION);
    if (ret != 0)
        return -1;

    struct nlmsghdr *nlh = mnlg_msg_prepare(&nlg, WG_CMD_GET_DEVICE,
                                                  NLM_F_REQUEST | NLM_F_ACK | NLM_F_DUMP,
                                                  nlg.id, nlg.version);
    mnl_attr_put_strz(nlh, WGDEVICE_A_IFNAME, iface);
    if (mnlg_socket_send(&nlg, nlh) < 0) {
        ret = -errno;
        goto out;
    }
    errno = 0;
    if (mnlg_socket_recv_run(&nlg, read_device_cb, &device) < 0) {
            ret = errno ? -errno : -EINVAL;
            goto out;
    }

    // TODO coalesce peers
out:
    mnlg_socket_close(&nlg);
    if (ret) {
        if (ret == -EINTR)
            goto try_again;
    }
    errno = -ret;
    return ret;
}

static int parse_linkinfo(const struct nlattr *attr, void *data)
{
    struct interface *interface = data;

    if (mnl_attr_get_type(attr) == IFLA_INFO_KIND && !strcmp(WG_GENL_NAME, mnl_attr_get_str(attr)))
        interface->is_wireguard = true;

    return MNL_CB_OK;
}

static int parse_infomsg(const struct nlattr *attr, void *data)
{
    struct interface *interface = data;

    if (mnl_attr_get_type(attr) == IFLA_LINKINFO)
        return mnl_attr_parse_nested(attr, parse_linkinfo, data);
    else if (mnl_attr_get_type(attr) == IFLA_IFNAME)
        interface->name = mnl_attr_get_str(attr);

    return MNL_CB_OK;
}

static int read_devices_cb(const struct nlmsghdr *nlh, __attribute__((unused)) void *data)
{
    struct interface interface = { 0 };

    int ret = mnl_attr_parse(nlh, sizeof(struct ifinfomsg), parse_infomsg, &interface);
    if (ret != MNL_CB_OK)
        return ret;

    if (interface.name && interface.is_wireguard)
        kernel_get_device(interface.name);

    if (nlh->nlmsg_type != NLMSG_DONE)
        return MNL_CB_OK + 1;

    return MNL_CB_OK;
}

static int wireguard_read(void)
{
    char *rtnl_buffer[SOCKET_BUFFER_SIZE];

    struct mnl_socket *nl = mnl_socket_open(NETLINK_ROUTE);
    if (!nl)
        return -errno;

    if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
        mnl_socket_close(nl);
        return -errno;
    }
    /* coverity[Y2K38_SAFETY] */
    unsigned int seq = time(NULL);
    unsigned int portid = mnl_socket_get_portid(nl);
    struct nlmsghdr *nlh = mnl_nlmsg_put_header(rtnl_buffer);
    nlh->nlmsg_type = RTM_GETLINK;
    nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_DUMP;
    nlh->nlmsg_seq = seq;
    struct ifinfomsg *ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(*ifm));
    ifm->ifi_family = AF_UNSPEC;
    size_t message_len = nlh->nlmsg_len;

    if (mnl_socket_sendto(nl, rtnl_buffer, message_len) < 0) {
        mnl_socket_close(nl);
        return -errno;
    }
    while (true) {
        ssize_t len = mnl_socket_recvfrom(nl, rtnl_buffer, SOCKET_BUFFER_SIZE);
        if (len < 0) {
            // return -errno;
            break;
        }
        len = mnl_cb_run(rtnl_buffer, len, seq, portid, read_devices_cb, NULL);
        if (len < 0) {
            /* Netlink returns NLM_F_DUMP_INTR if the set of all tunnels changed
             * during the dump. That's unfortunate, but is pretty common on busy
             * systems that are adding and removing tunnels all the time. Rather
             * than retrying, potentially indefinitely, we just work with the
             * partial results. */
            if (errno != EINTR) {
                // return -errno;
                break;
            }
        }
        if (len != MNL_CB_OK + 1)
            break;
    }

    plugin_dispatch_metric_family_array(fams, FAM_WIREGUARD_MAX, 0);

    mnl_socket_close(nl);
    return 0;
}

void module_register(void)
{
    plugin_register_read("wireguard", wireguard_read);
}
