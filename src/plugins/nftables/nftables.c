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
#include <libnftnl/object.h>

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
    char *counter;
} nftables_counter_t;

static nftables_counter_t *counter_list;
static size_t counter_count;

static struct mnl_socket *nl;
static uint32_t portid;

static int counter_cb(const struct nlmsghdr *nlh, void *data)
{
    nftables_counter_t *r_info = data;

    struct nftnl_obj *nlo = nftnl_obj_alloc();
    if (nlo == NULL) {
        PLUGIN_ERROR("nftnl_obj_alloc failed\n");
        return 1;
    }

    if (nftnl_obj_nlmsg_parse(nlh, nlo) < 0) {
        PLUGIN_ERROR("nftnl_obj_nlmsg_parse failed\n");
        nftnl_obj_free(nlo);
        return 1;
    }

    uint32_t family = nftnl_obj_get_u32(nlo, NFTNL_OBJ_FAMILY);
    char *family_name = "unknown";
    switch (family) {
    case NFPROTO_IPV4:
        family_name = "ip";
        break;
    case NFPROTO_IPV6:
        family_name = "ip6";
        break;
    case NFPROTO_ARP:
        family_name = "arp";
        break;
    case NFPROTO_BRIDGE:
        family_name = "bridge";
        break;
    case NFPROTO_NETDEV:
        family_name = "netdev";
        break;
    case NFPROTO_INET:
        family_name = "inet";
        break;
    }

    const char *table = nftnl_obj_get_str(nlo, NFTNL_OBJ_TABLE);
    const char *name = nftnl_obj_get_str(nlo, NFTNL_OBJ_NAME);

    if ((table == NULL) || (name == NULL)) {
        nftnl_obj_free(nlo);
        return MNL_CB_OK;
    }

    if ((r_info->table != NULL) && (strcmp(r_info->table, table) != 0)) {
        nftnl_obj_free(nlo);
        return MNL_CB_OK;
    }

    if ((r_info->counter != NULL) && (strcmp(r_info->counter, name) != 0)) {
        nftnl_obj_free(nlo);
        return MNL_CB_OK;
    }

    metric_family_append(&fams[FAM_NFTABLES_BYTES],
                         VALUE_COUNTER(nftnl_obj_get_u64(nlo, NFTNL_OBJ_CTR_BYTES)), NULL,
                         &(label_pair_const_t){.name="family", family_name},
                         &(label_pair_const_t){.name="table", table},
                         &(label_pair_const_t){.name="counter", name}, NULL);
    metric_family_append(&fams[FAM_NFTABLES_PACKETS],
                         VALUE_COUNTER(nftnl_obj_get_u64(nlo, NFTNL_OBJ_CTR_PKTS)), NULL,
                         &(label_pair_const_t){.name="family", family_name},
                         &(label_pair_const_t){.name="table", table},
                         &(label_pair_const_t){.name="counter", name}, NULL);

    nftnl_obj_free(nlo);

    return MNL_CB_OK;
}

static int nftables_read(void)
{
    int num_failures = 0;
    char buf[MNL_SOCKET_BUFFER_SIZE];

    for (size_t i = 0; i < counter_count; i++) {
        nftables_counter_t *r_info = &counter_list[i];
        int family = r_info->family;
        char *table = r_info->table;
        char *counter = r_info->counter;

        struct nftnl_obj *n = nftnl_obj_alloc();
        if (n == NULL) {
            PLUGIN_ERROR("nftnl_obj_alloc failed\n");
            return 1;
        }

        /* coverity[Y2K38_SAFETY] */
        uint32_t seq = time(NULL);
        struct nlmsghdr *nlh = nftnl_nlmsg_build_hdr(buf, NFT_MSG_GETOBJ, family, NLM_F_DUMP, seq);

        if (table != NULL)
            nftnl_obj_set_str(n, NFTNL_OBJ_TABLE, table);
        if (counter != NULL)
            nftnl_obj_set_str(n, NFTNL_OBJ_NAME, counter);

        nftnl_obj_set_u32(n, NFTNL_OBJ_TYPE, NFT_OBJECT_COUNTER);

        nftnl_obj_nlmsg_build_payload(nlh, n);

        nftnl_obj_free(n);

        if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
            PLUGIN_ERROR("Error sending to mnl socket");
            num_failures++;
            continue;
        }

        ssize_t ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
        while (ret > 0) {
            ret = mnl_cb_run(buf, ret, seq, portid, counter_cb, r_info);
            if (ret <= 0)
                break;
            ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
        }

        if (ret == -1) {
            PLUGIN_ERROR("Error when reading from nl socket");
            num_failures++;
        }
    }

    plugin_dispatch_metric_family_array(fams, FAM_NFTABLES_MAX, 0);

    return num_failures;
}

static int nftables_append_counter(int family, char *table, char *counter)
{
    nftables_counter_t *list = realloc(counter_list,
                                       (counter_count + 1) * sizeof(nftables_counter_t));
    if (list == NULL) {
        PLUGIN_ERROR("list realloc failed: %s", STRERRNO);
        return -1;
    }

    counter_list = list;
    counter_list[counter_count].family = family;
    if (table != NULL)
        counter_list[counter_count].table = strdup(table);
    else
        counter_list[counter_count].table = NULL;
    if (counter != NULL)
        counter_list[counter_count].counter = strdup(counter);
    else
        counter_list[counter_count].counter = NULL;

    counter_count++;

    return 0;
}

static int nftables_config_counter(config_item_t *ci)
{
    if ((ci->values_num != 1) && (ci->values_num != 2) && (ci->values_num != 3)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires one, two or three string arguments.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if (ci->values[0].type != CONFIG_TYPE_STRING) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires a string argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if (ci->values_num == 2) {
        if (ci->values[1].type != CONFIG_TYPE_STRING) {
            PLUGIN_ERROR("The '%s' option in %s:%d requires a string arguments.",
                         ci->key, cf_get_file(ci), cf_get_lineno(ci));
            return -1;
        }
    } else if (ci->values_num == 3) {
        if ((ci->values[1].type != CONFIG_TYPE_STRING) ||
            (ci->values[2].type != CONFIG_TYPE_STRING)) {
            PLUGIN_ERROR("The '%s' option in %s:%d requires a string arguments.",
                         ci->key, cf_get_file(ci), cf_get_lineno(ci));
            return -1;
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
        PLUGIN_ERROR("The '%s' option in %s:%d has a unknown family: '%s', "
                     "must be ip, ip6, arp, birdge, netdev or inet.",
                      ci->key, cf_get_file(ci), cf_get_lineno(ci), ci->values[0].value.string);
        return -1;
    }

    char *table = NULL;
    char *counter = NULL;

    if (ci->values_num == 2)
        table = ci->values[1].value.string;

    if (ci->values_num == 3) {
        table = ci->values[1].value.string;
        counter = ci->values[2].value.string;
    }

    int status = nftables_append_counter(family, table, counter);
    if (status != 0)
        return -1;

    return 0;
}

static int nftables_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("counter", child->key) == 0) {
            status = nftables_config_counter(child);
        } else {
            PLUGIN_ERROR("The configuration option '%s' in %s:%d is not allowed here.",
                         child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}


static int nftables_init(void)
{
    nl = mnl_socket_open(NETLINK_NETFILTER);

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

    for (size_t i = 0; i < counter_count; i++) {
        free(counter_list[i].table);
        free(counter_list[i].counter);
    }

    free(counter_list);

    return 0;
}

void module_register(void)
{
    plugin_register_config("nftables", nftables_config);
    plugin_register_init("nftables", nftables_init);
    plugin_register_read("nftables", nftables_read);
    plugin_register_shutdown("nftables", nftables_shutdown);
}
