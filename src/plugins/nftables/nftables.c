// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2020 Jose M. Guisado
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Jose M. Guisado <guigom at riseup.net>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <linux/netfilter.h>
#include <linux/netfilter/nf_tables.h>

#include <libmnl/libmnl.h>
#include <libnftnl/expr.h>
#include <libnftnl/rule.h>
#include <libnftnl/udata.h>

enum {
    FAM_NFTABLES_BYTES,
    FAM_NFTABLES_PACKETS,
    FAM_NFTABLES_MAX,
};

static metric_family_t fams[FAM_NFTABLES_MAX] = {
    [FAM_NFTABLES_BYTES] = {
        .name = "system_nftables_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_NFTABLES_PACKETS] = {
        .name = "system_nftables_packets",
        .type = METRIC_TYPE_COUNTER,
    },
};

typedef struct {
    int family;
    char *table;
    char *chain;
    char *comment;
    uint64_t pkts, bytes;
} nftables_rule_t;

static nftables_rule_t *rule_list;
static int rule_count = 0;

static struct mnl_socket *nl;
static uint32_t portid;

static struct nftnl_rule *setup_rule(uint8_t family, const char *table, const char *chain)
{
    struct nftnl_rule *r;

    r = nftnl_rule_alloc();
    if (r == NULL) {
        PLUGIN_ERROR("error allocating nftnl_rule");
        return NULL;
    }

    if (table != NULL)
        nftnl_rule_set_str(r, NFTNL_RULE_TABLE, table);
    if (chain != NULL)
        nftnl_rule_set_str(r, NFTNL_RULE_CHAIN, chain);

    nftnl_rule_set_u32(r, NFTNL_RULE_FAMILY, family);

    return r;
}

static int parse_rule_udata_cb(const struct nftnl_udata *attr, void *data)
{
    unsigned char *value = nftnl_udata_get(attr);
    uint8_t type = nftnl_udata_type(attr);
    uint8_t len = nftnl_udata_len(attr);
    const struct nftnl_udata **tb = data;

    if (type == NFTNL_UDATA_RULE_COMMENT) {
        if (value[len - 1] != '\0')
            return -1;
    } else {
        return 0;
    }

    tb[type] = attr;
    return 0;
}

static char *nftnl_rule_get_comment(const struct nftnl_rule *nlr)
{
    const struct nftnl_udata *tb[NFTNL_UDATA_RULE_MAX + 1] = {0};
    const void *data;
    uint32_t len;

    if (!nftnl_rule_is_set(nlr, NFTNL_RULE_USERDATA))
        return NULL;

    data = nftnl_rule_get_data(nlr, NFTNL_RULE_USERDATA, &len);

    if (nftnl_udata_parse(data, len, parse_rule_udata_cb, tb) < 0)
        return NULL;

    if (!tb[NFTNL_UDATA_RULE_COMMENT])
        return NULL;

    return strdup(nftnl_udata_get(tb[NFTNL_UDATA_RULE_COMMENT]));
}

static int submit_cb(struct nftnl_expr *e, void *data)
{
    const char *name = nftnl_expr_get_str(e, NFTNL_EXPR_NAME);
    struct nftnl_rule *r = (struct nftnl_rule *)data;
    const char *table = nftnl_rule_get_str(r, NFTNL_RULE_TABLE);
    const char *chain = nftnl_rule_get_str(r, NFTNL_RULE_CHAIN);
    char *comment = nftnl_rule_get_comment(r);

    if (strcmp(name, "counter") == 0) {
        metric_family_append(&fams[FAM_NFTABLES_BYTES],
                             VALUE_COUNTER(nftnl_expr_get_u64(e, NFTNL_EXPR_CTR_BYTES)), NULL,
                             &(label_pair_const_t){.name="table", table},
                             &(label_pair_const_t){.name="chain", chain},
                             &(label_pair_const_t){.name="comment", comment}, NULL);
        metric_family_append(&fams[FAM_NFTABLES_PACKETS],
                             VALUE_COUNTER(nftnl_expr_get_u64(e, NFTNL_EXPR_CTR_PACKETS)), NULL,
                             &(label_pair_const_t){.name="table", table},
                             &(label_pair_const_t){.name="chain", chain},
                             &(label_pair_const_t){.name="comment", comment}, NULL);
    }

    free(comment);

    return MNL_CB_OK;
}

static int table_cb(const struct nlmsghdr *nlh, void *data)
{
    struct nftnl_rule *t;
    char *filter_comment = (char *)data; /* config filter comment */
    char *udata_comment = NULL;                  /* rule userdata comment */

    t = nftnl_rule_alloc();
    if (t == NULL) {
        PLUGIN_ERROR("Error allocating nftnl_rule");
        goto err;
    }

    if (nftnl_rule_nlmsg_parse(nlh, t) < 0) {
        PLUGIN_ERROR("Error parsing nlmsghdr");
        goto err_free;
    }

    udata_comment = nftnl_rule_get_comment(t);
    PLUGIN_DEBUG("table_cb | filter_comment: %s rule_comment: %s", filter_comment, udata_comment);
    if (strlen(filter_comment) > 0) {
        if (udata_comment && strcmp(udata_comment, filter_comment) == 0) {
            nftnl_expr_foreach(t, submit_cb, t);
        }
    } else if (udata_comment) {
        nftnl_expr_foreach(t, submit_cb, t);
    }

err_free:
    free(udata_comment);
    nftnl_rule_free(t);
err:
    return MNL_CB_OK;
}

static int nftables_read(void)
{
    int num_failures = 0;
    uint32_t seq;
    struct nlmsghdr *nlh;
    char buf[MNL_SOCKET_BUFFER_SIZE];

    for (int i = 0; i < rule_count; i++) {
        struct nftnl_rule *r;
        nftables_rule_t *r_info = &rule_list[i];
        int family = r_info->family;
        char *table = r_info->table;
        char *chain = r_info->chain;
        char *comment = r_info->comment;

        /* coverity[Y2K38_SAFETY] */
        seq = time(NULL);
        nlh = nftnl_rule_nlmsg_build_hdr(buf, NFT_MSG_GETRULE, family, NLM_F_DUMP, seq);

        r = setup_rule(family, table, chain);

        if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
            PLUGIN_ERROR("nftables plugin: Error sending to mnl socket");
            num_failures++;
            continue;
        }

        ssize_t ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
        PLUGIN_DEBUG("Rule counters from table: %s chain: %s | ret: %ld", table, chain, ret);
        while (ret > 0) {
            ret = mnl_cb_run(buf, ret, seq, portid, table_cb, comment);
            if (ret <= 0)
                break;
            ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
        }
        if (ret == -1) {
            PLUGIN_ERROR("Error when reading from nl socket");
            num_failures++;
        }

        nftnl_rule_free(r);
    }

    return num_failures;
}

static int nftables_config_rule(int family, char *table, char *chain, char *comment)
{
    nftables_rule_t *list = realloc(rule_list, (rule_count + 1) * sizeof(nftables_rule_t));
    if (list == NULL) {
        PLUGIN_ERROR("list realloc failed: %s", STRERRNO);
        return -1;
    }

    rule_list = list;
    rule_list[rule_count].family = family;
    rule_list[rule_count].table = strdup(table);
    rule_list[rule_count].chain = strdup(chain);
    rule_list[rule_count].comment = strdup(comment);
    rule_count++;

    return 0;
}

/*
 * rule [ip|ip6|arp|birdge|netdev|inet] table chain comment
 */
static int nftables_config(config_item_t *ci)
{
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "rule") != 0) {
            PLUGIN_ERROR("The configuration option \"%s\" is not allowed here.", child->key);
            return -1;
        }

        if (!((child->values_num == 3) || (child->values_num == 4))) {
            PLUGIN_ERROR("Invalid number of arguments for option `%s'.", child->key);
            return -1;
        }

        if (child->values_num >= 3) {
            if ((child->values[0].type != CONFIG_TYPE_STRING) ||
                (child->values[1].type != CONFIG_TYPE_STRING) ||
                (child->values[2].type != CONFIG_TYPE_STRING)) {
                PLUGIN_ERROR("The `%s' option requires three or four string arguments.", child->key);
            }
        }
        if (child->values_num == 4) {
            if (child->values[3].type != CONFIG_TYPE_STRING) {
                PLUGIN_ERROR("The `%s' option requires four string arguments.", child->key);
            }
        }

        int family;
        if (strcasecmp(ci->values[0].value.string, "ip") == 0) {
            family = NFPROTO_IPV4;
        } else if (strcasecmp(ci->values[0].value.string, "ip6") == 0) {
            family = NFPROTO_IPV6;
        } else if (strcasecmp(ci->values[0].value.string, "arp") == 0) {
            family = NFPROTO_ARP;
        } else if (strcasecmp(ci->values[0].value.string, "bridge") == 0) {
            family = NFPROTO_BRIDGE;
        } else if (strcasecmp(ci->values[0].value.string, "netdev") == 0) {
            family = NFPROTO_NETDEV;
        } else if (strcasecmp(ci->values[0].value.string, "inet") == 0) {
            family = NFPROTO_INET;
        } else {
            PLUGIN_ERROR("Unknown family: %s", ci->values[0].value.string);
            return -1;
        }

        char *table = ci->values[1].value.string;
        char *chain = ci->values[2].value.string;
        char *comment = "";
        if (child->values_num == 4) {
            comment = ci->values[3].value.string;
        }

        int status = nftables_config_rule(family, table, chain, comment);
        if (status != 0) {
            return -1;
        }
    }

    return 0;
}

static int nftables_init(void)
{
    nl = mnl_socket_open(NETLINK_NETFILTER);

    for (int i = 0; i < rule_count; i++) {
        PLUGIN_DEBUG("rule_list[%d] => family: %d table: %s chain: %s comment: %s", i,
                      rule_list[i].family, rule_list[i].table, rule_list[i].chain,
                      rule_list[i].comment);
    }

    if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
        PLUGIN_ERROR("error calling mnl_socket_bind");
        return -1;
    }

    portid = mnl_socket_get_portid(nl);
    PLUGIN_DEBUG("mnl socket bind, portid: %u", portid);

    return 0;
}

static int nftables_shutdown(void)
{
    mnl_socket_close(nl);

    for (int i = 0; i < rule_count; i++) {
        free(rule_list[i].table);
        free(rule_list[i].chain);
        free(rule_list[i].comment);
    }
    free(rule_list);

    return 0;
}

void module_register(void)
{
    plugin_register_config("nftables", nftables_config);
    plugin_register_init("nftables", nftables_init);
    plugin_register_read("nftables", nftables_read);
    plugin_register_shutdown("nftables", nftables_shutdown);
}
