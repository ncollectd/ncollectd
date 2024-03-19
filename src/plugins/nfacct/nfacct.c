// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <libmnl/libmnl.h>
#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nfnetlink_acct.h>

#ifdef HAVE_SYS_CAPABILITY_H
#include <sys/capability.h>
#endif

enum {
    FAM_NFACCT_BYTES,
    FAM_NFACCT_PACKETS,
    FAM_NFACCT_QUOTA_BYTES,
    FAM_NFACCT_QUOTA_PACKETS,
    FAM_NFACCT_OVER_QUOTA,
    FAM_NFACCT_MAX,
};

static metric_family_t fams[FAM_NFACCT_MAX] = {
    [FAM_NFACCT_BYTES] = {
        .name = "system_nfacct_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFACCT_PACKETS] = {
        .name = "system_nfacct_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFACCT_QUOTA_BYTES] = {
        .name = "system_nfacct_quota_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_NFACCT_QUOTA_PACKETS] = {
        .name = "system_nfacct_quota_packets",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_NFACCT_OVER_QUOTA] = {
        .name = "system_nfacct_over_quota",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
};

static int nfacct_nlmsg_parse_attr_cb(const struct nlattr *attr, void *data)
{
    const struct nlattr **tb = data;

    if (mnl_attr_type_valid(attr, NFACCT_MAX) < 0)
        return MNL_CB_OK;

    int type = mnl_attr_get_type(attr);
    switch(type) {
    case NFACCT_NAME:
        if (mnl_attr_validate(attr, MNL_TYPE_STRING) < 0) {
            PLUGIN_ERROR("mnl_attr_validate");
            return MNL_CB_ERROR;
        }
        break;
    case NFACCT_PKTS:
    case NFACCT_BYTES:
        if (mnl_attr_validate(attr, MNL_TYPE_U64) < 0) {
            PLUGIN_ERROR("mnl_attr_validate");
            return MNL_CB_ERROR;
        }
        break;
    }

    tb[type] = attr;

    return MNL_CB_OK;
}

static int nfacct_read_cb(const struct nlmsghdr *nlh, __attribute__((unused)) void *data)
{
    struct nlattr *tb[NFACCT_MAX+1] = {0};
//  struct nfgenmsg *nfg = mnl_nlmsg_get_payload(nlh);

    mnl_attr_parse(nlh, sizeof(struct nfgenmsg), nfacct_nlmsg_parse_attr_cb, tb);
    if (!tb[NFACCT_NAME] && !tb[NFACCT_PKTS] && !tb[NFACCT_BYTES])
        return MNL_CB_OK;

    const char *name = mnl_attr_get_str(tb[NFACCT_NAME]);
    uint64_t pkts = be64toh(mnl_attr_get_u64(tb[NFACCT_PKTS]));
    uint64_t bytes = be64toh(mnl_attr_get_u64(tb[NFACCT_BYTES]));

    metric_family_append(&fams[FAM_NFACCT_BYTES], VALUE_COUNTER(bytes), NULL,
                         &(label_pair_const_t){.name="name", .value=name}, NULL);

    metric_family_append(&fams[FAM_NFACCT_PACKETS], VALUE_COUNTER(pkts), NULL,
                         &(label_pair_const_t){.name="name", .value=name}, NULL);

    if (tb[NFACCT_FLAGS] && tb[NFACCT_QUOTA]) {
        uint32_t flags = be32toh(mnl_attr_get_u32(tb[NFACCT_FLAGS]));
        uint64_t quota = be64toh(mnl_attr_get_u64(tb[NFACCT_QUOTA]));

        if (flags) {
            if (flags & NFACCT_F_QUOTA_BYTES) {
                metric_family_append(&fams[FAM_NFACCT_QUOTA_BYTES], VALUE_GAUGE(quota), NULL,
                                     &(label_pair_const_t){.name="name", .value=name}, NULL);
            } else if (flags & NFACCT_F_QUOTA_PKTS) {
                metric_family_append(&fams[FAM_NFACCT_QUOTA_PACKETS], VALUE_GAUGE(quota), NULL,
                                     &(label_pair_const_t){.name="name", .value=name}, NULL);
            }


            metric_family_append(&fams[FAM_NFACCT_OVER_QUOTA],
                                 VALUE_GAUGE(flags & NFACCT_F_OVERQUOTA ? 1 : 0),
                                 NULL, &(label_pair_const_t){.name="name", .value=name}, NULL);

        }
    }

    return MNL_CB_OK;
}

static int nfacct_read(void)
{
    /* coverity[Y2K38_SAFETY] */
    unsigned int seq = time(NULL);

    char buf[MNL_SOCKET_BUFFER_SIZE];
    struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);
    nlh->nlmsg_type = (NFNL_SUBSYS_ACCT << 8) | NFNL_MSG_ACCT_GET;
    nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    nlh->nlmsg_seq = seq;

    struct nfgenmsg *nfh = mnl_nlmsg_put_extra_header(nlh, sizeof(struct nfgenmsg));
    nfh->nfgen_family = AF_UNSPEC;
    nfh->version = NFNETLINK_V0;
    nfh->res_id = 0;

    struct nlattr *nest = mnl_attr_nest_start(nlh, NFACCT_FILTER);
    mnl_attr_put_u32(nlh, NFACCT_FILTER_MASK, htonl(NFACCT_F_QUOTA_BYTES | NFACCT_F_QUOTA_PKTS));
    mnl_attr_put_u32(nlh, NFACCT_FILTER_VALUE, htonl(0));
    mnl_attr_nest_end(nlh, nest);

    struct mnl_socket *nl = mnl_socket_open(NETLINK_NETFILTER);
    if (nl == NULL) {
        PLUGIN_ERROR("mnl_socket_open");
        return -1;
    }

    if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
        PLUGIN_ERROR("mnl_socket_bind");
        mnl_socket_close(nl);
        return -1;
    }

    unsigned int portid = mnl_socket_get_portid(nl);

    if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
        PLUGIN_ERROR("mnl_socket_send");
        mnl_socket_close(nl);
        return -1;
    }

    int ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
    while (ret > 0) {
        ret = mnl_cb_run(buf, ret, seq, portid, nfacct_read_cb, NULL);
        if (ret <= 0)
            break;
        ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
    }
    if (ret == -1) {
        PLUGIN_ERROR("error");
    }

    mnl_socket_close(nl);

    plugin_dispatch_metric_family_array(fams, FAM_NFACCT_MAX, 0);

    return 0;
}

static int nfacct_init(void)
{
#if defined(HAVE_SYS_CAPABILITY_H) && defined(CAP_NET_ADMIN)
    if (plugin_check_capability(CAP_NET_ADMIN) != 0) {
        if (getuid() == 0)
            PLUGIN_WARNING("Running ncollectd as root, but the "
                           "CAP_NET_ADMIN capability is missing. The plugin's read "
                           "function will probably fail. Is your init system dropping "
                           "capabilities?");
        else
            PLUGIN_WARNING("ncollectd doesn't have the CAP_NET_ADMIN "
                           "capability. If you don't want to run ncollectd as root, try "
                           "running \"setcap cap_net_admin=ep\" on the ncollectd binary.");
    }
#endif
    return 0;
}

void module_register(void)
{
    plugin_register_init("nfacct", nfacct_init);
    plugin_register_read("nfacct", nfacct_read);
}
