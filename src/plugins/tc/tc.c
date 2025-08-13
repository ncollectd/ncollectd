// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2007-2010 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2008-2012 Sebastian Harl
// SPDX-FileCopyrightText: Copyright (C) 2013 Andreas Henriksson
// SPDX-FileCopyrightText: Copyright (C) 2013 Marc Fournier
// SPDX-FileCopyrightText: Copyright (C) 2020 Intel Corporation
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>
// SPDX-FileContributor: Andreas Henriksson <andreas at fatal.se>
// SPDX-FileContributor: Marc Fournier <marc.fournier at camptocamp.com>
// SPDX-FileContributor: Kamil Wiatrowski <kamilx.wiatrowski at intel.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <regex.h>

#include <asm/types.h>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#ifdef HAVE_LINUX_GEN_STATS_H
#include <linux/gen_stats.h>
#endif
#ifdef HAVE_LINUX_PKT_SCHED_H
#include <linux/pkt_sched.h>
#endif

#include <glob.h>
#include <libmnl/libmnl.h>

#define NETLINK_DEFAULT_BUF_SIZE_KB 16

enum {
    FAM_TC_QDISC_BYTES,
    FAM_TC_QDISC_PACKETS,
    FAM_TC_QDISC_DROPS,
    FAM_TC_QDISC_CURRENT_QUEUE_LENGTH,
    FAM_TC_QDISC_BACKLOG,
    FAM_TC_QDISC_REQUEUES,
    FAM_TC_QDISC_OVERLIMITS,
    FAM_TC_CLASS_BYTES,
    FAM_TC_CLASS_PACKETS,
    FAM_TC_CLASS_DROPS,
    FAM_TC_FILTER_BYTES,
    FAM_TC_FILTER_PACKETS,
    FAM_TC_FILTER_DROPS,
    FAM_TC_MAX,
};

static metric_family_t fams[FAM_TC_MAX] = {
    [FAM_TC_QDISC_BYTES] = {
        .name = "system_tc_qdisc_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of seen bytes in this qdisc.",
    },
    [FAM_TC_QDISC_PACKETS] = {
        .name = "system_tc_qdisc_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of seen packets in this qdisc."
    },
    [FAM_TC_QDISC_DROPS] = {
        .name = "system_tc_qdisc_drops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of dropped packets in this qdisc.",
    },
    [FAM_TC_QDISC_CURRENT_QUEUE_LENGTH] = {
        .name = "system_tc_qdisc_current_queue_length",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of packets currently in queue in this qdisc to be sent."
    },
    [FAM_TC_QDISC_BACKLOG] = {
        .name = "system_tc_qdisc_backlog",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes currently in queue to be sent in this qdisc.",
    },
    [FAM_TC_QDISC_REQUEUES] = {
        .name = "system_tc_qdisc_requeues",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of packets dequeued, not transmitted, and requeued in this qdisc.",
    },
    [FAM_TC_QDISC_OVERLIMITS] = {
        .name = "system_tc_qdisc_overlimits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of overlimit packets in this qdisc.",
    },
    [FAM_TC_CLASS_BYTES] = {
        .name = "system_tc_class_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of seen bytes in this class.",
    },
    [FAM_TC_CLASS_PACKETS] = {
        .name = "system_tc_class_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of seen packets in this class."
    },
    [FAM_TC_CLASS_DROPS] = {
        .name = "system_tc_class_drops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of dropped packets in this class.",
    },
    [FAM_TC_FILTER_BYTES] = {
        .name = "system_tc_filter_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of seen bytes in this filter.",
    },
    [FAM_TC_FILTER_PACKETS] = {
        .name = "system_tc_filter_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of seen packets in this filter."
    },
    [FAM_TC_FILTER_DROPS] = {
        .name = "system_tc_filter_drops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of dropped packets in this filter.",
    },
};

typedef struct ir_ignorelist_s {
    char *device;
    regex_t *rdevice; /* regular expression device identification */
    char *type;
    char *inst;
    struct ir_ignorelist_s *next;
} ir_ignorelist_t;

struct qos_stats {
    struct gnet_stats_basic *bs;
    struct gnet_stats_queue *qs;
};

static bool ir_ignorelist_invert = true;
static ir_ignorelist_t *ir_ignorelist_head;

static struct mnl_socket *nl;

static char **iflist;
static size_t iflist_len;

static size_t nl_socket_buffer_size = NETLINK_DEFAULT_BUF_SIZE_KB * 1024;

static int add_ignorelist(const char *dev, const char *type, const char *inst)
{
    ir_ignorelist_t *entry;

    entry = calloc(1, sizeof(*entry));
    if (entry == NULL)
        return -1;

    size_t len = strlen(dev);
    /* regex string is enclosed in "/.../" */
    if ((len > 2) && (dev[0] == '/') && dev[len - 1] == '/') {
        char *copy = strdup(dev + 1);
        if (copy == NULL) {
            free(entry);
            return -1;
        }
        copy[strlen(copy) - 1] = '\0';

        regex_t *re = calloc(1, sizeof(*re));
        if (re == NULL) {
            free(entry);
            free(copy);
            return -1;
        }

        int status = regcomp(re, copy, REG_EXTENDED);
        if (status != 0) {
            char errbuf[1024];
            (void)regerror(status, re, errbuf, sizeof(errbuf));
            PLUGIN_ERROR("add_ignorelist: regcomp for %s failed: %s", dev, errbuf);
            regfree(re);
            free(entry);
            free(copy);
            free(re);
            return -1;
        }

        entry->rdevice = re;
        free(copy);
    } else if (strcasecmp(dev, "all") != 0) {
        entry->device = strdup(dev);
        if (entry->device == NULL) {
            free(entry);
            return -1;
        }
    }

    entry->type = strdup(type);
    if (entry->type == NULL) {
        free(entry->device);
        free(entry->rdevice);
        free(entry);
        return -1;
    }

    if (inst != NULL) {
        entry->inst = strdup(inst);
        if (entry->inst == NULL) {
            free(entry->type);
            free(entry->device);
            free(entry->rdevice);
            free(entry);
            return -1;
        }
    }

    entry->next = ir_ignorelist_head;
    ir_ignorelist_head = entry;

    return 0;
}

/*
 * Checks whether a data set should be ignored. Returns `true' is the value
 * should be ignored, `false' otherwise.
 */
static int check_ignorelist(const char *dev, const char *type, const char *type_instance)
{
    assert((dev != NULL) && (type != NULL));

    if (ir_ignorelist_head == NULL)
        return ir_ignorelist_invert ? 0 : 1;

    for (ir_ignorelist_t *i = ir_ignorelist_head; i != NULL; i = i->next) {
        if (i->rdevice != NULL) {
            if (regexec(i->rdevice, dev, 0, NULL, 0))
                continue;
        } else if ((i->device != NULL) && (strcasecmp(i->device, dev) != 0)) {
            /* i->device == NULL    =>  match all devices */
            continue;
        }

        if (strcasecmp(i->type, type) != 0)
            continue;

        if ((i->inst != NULL) && (type_instance != NULL) &&
                (strcasecmp(i->inst, type_instance) != 0))
            continue;

#ifdef NCOLLECTD_DEBUG
        const char *device = i->device == NULL
                             ? (i->rdevice != NULL ? "(regexp)" : "(nil)")
                             : i->device;
        PLUGIN_DEBUG("check_ignorelist: "
                     "(dev = %s; type = %s; inst = %s) matched "
                     "(dev = %s; type = %s; inst = %s)",
                     dev, type, type_instance == NULL ? "(nil)" : type_instance, device,
                     i->type, i->inst == NULL ? "(nil)" : i->inst);
#endif

        return ir_ignorelist_invert ? 0 : 1;
    }

    return ir_ignorelist_invert;
}

static int update_iflist(struct ifinfomsg *msg, const char *dev)
{
    /* Update the `iflist'. It's used to know which interfaces exist and query
     * them later for qdiscs and classes. */
    if ((msg->ifi_index >= 0) && ((size_t)msg->ifi_index >= iflist_len)) {
        char **temp;

        temp = realloc(iflist, (msg->ifi_index + 1) * sizeof(char *));
        if (temp == NULL) {
            PLUGIN_ERROR("update_iflist: realloc failed.");
            return -1;
        }

        memset(temp + iflist_len, '\0', (msg->ifi_index + 1 - iflist_len) * sizeof(char *));
        iflist = temp;
        iflist_len = msg->ifi_index + 1;
    }
    if ((iflist[msg->ifi_index] == NULL) || (strcmp(iflist[msg->ifi_index], dev) != 0)) {
        if (iflist[msg->ifi_index] != NULL)
            free(iflist[msg->ifi_index]);
        iflist[msg->ifi_index] = strdup(dev);
    }

    return 0;
}

static int link_filter_cb(const struct nlmsghdr *nlh, void *args __attribute__((unused)))
{
    struct ifinfomsg *ifm = mnl_nlmsg_get_payload(nlh);
    struct nlattr *attr;
    const char *dev = NULL;

    if (nlh->nlmsg_type != RTM_NEWLINK) {
        PLUGIN_ERROR("Don't know how to handle type %i.", nlh->nlmsg_type);
        return MNL_CB_ERROR;
    }

    /* Scan attribute list for device name. */
    mnl_attr_for_each(attr, nlh, sizeof(*ifm)) {
        if (mnl_attr_get_type(attr) != IFLA_IFNAME)
            continue;

        if (mnl_attr_validate(attr, MNL_TYPE_STRING) < 0) {
            PLUGIN_ERROR("IFLA_IFNAME mnl_attr_validate failed.");
            return MNL_CB_ERROR;
        }

        dev = mnl_attr_get_str(attr);
        if (update_iflist(ifm, dev) < 0)
            return MNL_CB_ERROR;
        break;
    }

    return MNL_CB_OK;
}

#ifdef HAVE_TCA_STATS2
static int qos_attr_cb(const struct nlattr *attr, void *data)
{
    struct qos_stats *q_stats = (struct qos_stats *)data;

    /* skip unsupported attribute in user-space */
    if (mnl_attr_type_valid(attr, TCA_STATS_MAX) < 0)
        return MNL_CB_OK;

    if (mnl_attr_get_type(attr) == TCA_STATS_BASIC) {
        if (mnl_attr_validate2(attr, MNL_TYPE_UNSPEC, sizeof(*q_stats->bs)) < 0) {
            PLUGIN_ERROR("TCA_STATS_BASIC mnl_attr_validate2 failed: %s", STRERRNO);
            return MNL_CB_ERROR;
        }
        q_stats->bs = mnl_attr_get_payload(attr);
        return MNL_CB_OK;
    }

    if (mnl_attr_get_type(attr) == TCA_STATS_QUEUE) {
        if (mnl_attr_validate2(attr, MNL_TYPE_UNSPEC, sizeof(*q_stats->qs)) < 0) {
            PLUGIN_ERROR("TCA_STATS_QUEUE mnl_attr_validate2 failed.");
            return MNL_CB_ERROR;
        }
        q_stats->qs = mnl_attr_get_payload(attr);
        return MNL_CB_OK;
    }

    return MNL_CB_OK;
}
#endif

static int qos_filter_cb(const struct nlmsghdr *nlh, void *args)
{
    struct tcmsg *tm = mnl_nlmsg_get_payload(nlh);
    struct nlattr *attr;

    int wanted_ifindex = *((int *)args);

    /* char *type_instance; */
    const char *tc_type;
    char tc_inst[DATA_MAX_NAME_LEN];

    bool stats_submitted = false;

    metric_family_t *fam_tc_bytes = NULL;
    metric_family_t *fam_tc_packets = NULL;
    metric_family_t *fam_tc_dropped = NULL;

    if (nlh->nlmsg_type == RTM_NEWQDISC) {
        fam_tc_bytes = &fams[FAM_TC_QDISC_BYTES];
        fam_tc_packets = &fams[FAM_TC_QDISC_PACKETS];
        fam_tc_dropped = &fams[FAM_TC_QDISC_DROPS];
        tc_type = "qdisc";
    } else if (nlh->nlmsg_type == RTM_NEWTCLASS) {
        fam_tc_bytes = &fams[FAM_TC_CLASS_BYTES];
        fam_tc_packets = &fams[FAM_TC_CLASS_PACKETS];
        fam_tc_dropped = &fams[FAM_TC_CLASS_DROPS];
        tc_type = "class";
    } else if (nlh->nlmsg_type == RTM_NEWTFILTER) {
        fam_tc_bytes = &fams[FAM_TC_FILTER_BYTES];
        fam_tc_packets = &fams[FAM_TC_FILTER_PACKETS];
        fam_tc_dropped = &fams[FAM_TC_FILTER_DROPS];
        tc_type = "filter";
    } else {
        PLUGIN_ERROR("Don't know how to handle type %i.", nlh->nlmsg_type);
        return MNL_CB_ERROR;
    }

    if (tm->tcm_ifindex != wanted_ifindex) {
        PLUGIN_DEBUG("Got %s for interface #%i, " "but expected #%i.",
                                 tc_type, tm->tcm_ifindex, wanted_ifindex);
        return MNL_CB_OK;
    }

    if ((tm->tcm_ifindex >= 0) && ((size_t)tm->tcm_ifindex >= iflist_len)) {
        PLUGIN_ERROR("tm->tcm_ifindex = %i >= iflist_len = %" PRIsz, tm->tcm_ifindex, iflist_len);
        return MNL_CB_ERROR;
    }

    const char *dev = iflist[tm->tcm_ifindex];
    if (dev == NULL) {
        PLUGIN_ERROR("iflist[%i] == NULL", tm->tcm_ifindex);
        return MNL_CB_ERROR;
    }

    const char *kind = NULL;
    mnl_attr_for_each(attr, nlh, sizeof(*tm)) {
        if (mnl_attr_get_type(attr) != TCA_KIND)
            continue;

        if (mnl_attr_validate(attr, MNL_TYPE_STRING) < 0) {
            PLUGIN_ERROR("TCA_KIND mnl_attr_validate failed.");
            return MNL_CB_ERROR;
        }

        kind = mnl_attr_get_str(attr);
        break;
    }

    if (kind == NULL) {
        PLUGIN_ERROR("kind is NULL");
        return -1;
    }

    uint32_t numberic_id = tm->tcm_handle;
    if (strcmp(tc_type, "filter") == 0)
        numberic_id = tm->tcm_parent;

    ssnprintf(tc_inst, sizeof(tc_inst), "%s-%x:%x",
                       kind, numberic_id >> 16, numberic_id & 0x0000FFFF);

    PLUGIN_DEBUG("got %s for %s (%i).", tc_type, dev, tm->tcm_ifindex);

    if (check_ignorelist(dev, tc_type, tc_inst))
        return MNL_CB_OK;

#if HAVE_TCA_STATS2
    mnl_attr_for_each(attr, nlh, sizeof(*tm)) {
        struct qos_stats q_stats = {0};

        if (mnl_attr_get_type(attr) != TCA_STATS2)
            continue;

        if (mnl_attr_validate(attr, MNL_TYPE_NESTED) < 0) {
            PLUGIN_ERROR("TCA_STATS2 mnl_attr_validate failed.");
            return MNL_CB_ERROR;
        }

        mnl_attr_parse_nested(attr, qos_attr_cb, &q_stats);

        if (q_stats.bs != NULL || q_stats.qs != NULL) {
            if (q_stats.bs != NULL) {
                metric_family_append(fam_tc_bytes, VALUE_COUNTER(q_stats.bs->bytes), NULL,
                                     &(label_pair_const_t){.name="device", .value=dev},
                                     &(label_pair_const_t){.name="kind", .value=tc_inst}, NULL);
                metric_family_append(fam_tc_packets, VALUE_COUNTER(q_stats.bs->packets), NULL,
                                     &(label_pair_const_t){.name="device", .value=dev},
                                     &(label_pair_const_t){.name="kind", .value=tc_inst}, NULL);
            }
            if (q_stats.qs != NULL) {
                metric_family_append(fam_tc_dropped, VALUE_COUNTER(q_stats.qs->drops), NULL,
                                     &(label_pair_const_t){.name="device", .value=dev},
                                     &(label_pair_const_t){.name="kind", .value=tc_inst}, NULL);
            }
        }

        break;
    }
#endif

#if HAVE_TCA_STATS
    mnl_attr_for_each(attr, nlh, sizeof(*tm)) {
        struct tc_stats *ts = NULL;

        if (mnl_attr_get_type(attr) != TCA_STATS)
            continue;

        if (mnl_attr_validate2(attr, MNL_TYPE_UNSPEC, sizeof(*ts)) < 0) {
            PLUGIN_ERROR("TCA_STATS mnl_attr_validate2 failed: %s", STRERRNO);
            return MNL_CB_ERROR;
        }
        ts = mnl_attr_get_payload(attr);

        if (!stats_submitted && ts != NULL) {
            metric_family_append(fam_tc_bytes, VALUE_COUNTER(ts->bytes), NULL,
                                 &(label_pair_const_t){.name="device", .value=dev},
                                 &(label_pair_const_t){.name="kind", .value=tc_inst}, NULL);
            metric_family_append(fam_tc_packets, VALUE_COUNTER(ts->packets), NULL,
                                 &(label_pair_const_t){.name="device", .value=dev},
                                 &(label_pair_const_t){.name="kind", .value=tc_inst}, NULL);
            metric_family_append(fam_tc_dropped, VALUE_COUNTER(ts->drops), NULL,
                                 &(label_pair_const_t){.name="device", .value=dev},
                                 &(label_pair_const_t){.name="kind", .value=tc_inst}, NULL);
        }

        break;
    }
#endif

#if !(HAVE_TCA_STATS && HAVE_TCA_STATS2)
    PLUGIN_DEBUG("Have neither TCA_STATS2 nor TCA_STATS.");
#endif

    return MNL_CB_OK;
}

static int tc_read(void)
{
    char buf[nl_socket_buffer_size];
    struct nlmsghdr *nlh;
    struct rtgenmsg *rt;
    int ret;

    static const int type_id[] = {RTM_GETQDISC, RTM_GETTCLASS, RTM_GETTFILTER};
    static const char *type_name[] = {"qdisc", "class", "filter"};

    unsigned int portid = mnl_socket_get_portid(nl);

    nlh = mnl_nlmsg_put_header(buf);
    nlh->nlmsg_type = RTM_GETLINK;
    nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    unsigned int seq = time(NULL);
    nlh->nlmsg_seq = seq;
    rt = mnl_nlmsg_put_extra_header(nlh, sizeof(*rt));
    rt->rtgen_family = AF_PACKET;

    if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
        PLUGIN_ERROR("rtnl_wilddump_request failed.");
        return -1;
    }

    ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
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

    /* `link_filter_cb' will update `iflist' which is used here to iterate over all interfaces. */
    for (size_t ifindex = 1; ifindex < iflist_len; ifindex++) {
        struct tcmsg *tm;

        if (iflist[ifindex] == NULL)
            continue;

        for (size_t type_index = 0; type_index < STATIC_ARRAY_SIZE(type_id); type_index++) {
            if (check_ignorelist(iflist[ifindex], type_name[type_index], NULL)) {
                PLUGIN_DEBUG("check_ignorelist (%s, %s, (nil)) == TRUE",
                             iflist[ifindex], type_name[type_index]);
                continue;
            }

            PLUGIN_DEBUG("querying %s from %s (%" PRIsz ").",
                         type_name[type_index], iflist[ifindex], ifindex);

            nlh = mnl_nlmsg_put_header(buf);
            nlh->nlmsg_type = type_id[type_index];
            nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
            /* coverity[Y2K38_SAFETY] */
            nlh->nlmsg_seq = seq = time(NULL);
            tm = mnl_nlmsg_put_extra_header(nlh, sizeof(*tm));
            tm->tcm_family = AF_PACKET;
            tm->tcm_ifindex = ifindex;

            if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
                PLUGIN_ERROR("mnl_socket_sendto failed.");
                continue;
            }

            ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
            while (ret > 0) {
                ret = mnl_cb_run(buf, ret, seq, portid, qos_filter_cb, &ifindex);
                if (ret <= MNL_CB_STOP)
                    break;
                ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
            }
            if (ret < 0) {
                PLUGIN_ERROR("mnl_socket_recvfrom failed: %s", STRERRNO);
                continue;
            }
        }
    }

    plugin_dispatch_metric_family_array(fams, FAM_TC_MAX, 0);

    return 0;
}

static int ir_config_ignorelist(config_item_t *ci, const char *key, int args)
{
    char *arg1 = NULL;
    char *arg2 = NULL;

    switch(args) {
    case 1:
        if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
            PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                         ci->key, cf_get_file(ci), cf_get_lineno(ci));
            return -1;
        }
        arg1 = ci->values[0].value.string;
        break;
    case 2:
        if ((ci->values_num != 2) || (ci->values[0].type != CONFIG_TYPE_STRING) ||
                                     (ci->values[1].type != CONFIG_TYPE_STRING)) {
            PLUGIN_ERROR("The '%s' option in %s:%d requires exactly two string argument.",
                         ci->key, cf_get_file(ci), cf_get_lineno(ci));
            return -1;
        }
        arg1 = ci->values[0].value.string;
        arg2 = ci->values[1].value.string;
        break;
    default:
        return -1;
    }

    return add_ignorelist(arg1, key, arg2);
}

static int tc_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "interface") == 0) {
            status = ir_config_ignorelist(child, "interface", 1);
        } else if (strcasecmp(child->key, "qdisc") == 0) {
            status = ir_config_ignorelist(child, "qdisc", 2);
        } else if (strcasecmp(child->key, "class") == 0) {
            status = ir_config_ignorelist(child, "class", 2);
        } else if (strcasecmp(child->key, "filter") == 0) {
            status = ir_config_ignorelist(child, "filter", 2);
        } else if (strcasecmp(child->key, "ignore-selected") == 0) {
            status = cf_util_get_boolean(child, &ir_ignorelist_invert);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0)
        return -1;

    return 0;
}

static int tc_init(void)
{
    nl = mnl_socket_open(NETLINK_ROUTE);
    if (nl == NULL) {
        PLUGIN_ERROR("mnl_socket_open failed.");
        return -1;
    }

    if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
        PLUGIN_ERROR("mnl_socket_bind failed.");
        return -1;
    }

    return 0;
}

static int tc_shutdown(void)
{
    if (nl) {
        mnl_socket_close(nl);
        nl = NULL;
    }

    ir_ignorelist_t *next = NULL;
    for (ir_ignorelist_t *i = ir_ignorelist_head; i != NULL; i = next) {
        next = i->next;
        if (i->rdevice != NULL) {
            regfree(i->rdevice);
            free(i->rdevice);
        }
        free(i->inst);
        free(i->type);
        free(i->device);
        free(i);
    }
    ir_ignorelist_head = NULL;

    for (size_t ifindex = 0; ifindex < iflist_len; ifindex++) {
        free(iflist[ifindex]);
    }
    free(iflist);
    iflist_len = 0;
    iflist = NULL;


    return 0;
}

void module_register(void)
{
    plugin_register_config("tc", tc_config);
    plugin_register_init("tc", tc_init);
    plugin_register_read("tc", tc_read);
    plugin_register_shutdown("tc", tc_shutdown);
}
