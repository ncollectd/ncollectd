// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2007 Sjoerd van der Berg
// SPDX-FileCopyrightText: Copyright (C) 2007-2010  Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2009 Marco Chiappero
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Sjoerd van der Berg <harekiet at users.sourceforge.net>
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Marco Chiappero <marco at absence.it>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <libiptc/libip6tc.h>
#include <libiptc/libiptc.h>

#ifdef HAVE_SYS_CAPABILITY_H
#include <sys/capability.h>
#endif

enum {
    FAM_IPTABLES_BYTES,
    FAM_IPTABLES_PACKETS,
    FAM_IP6TABLES_BYTES,
    FAM_IP6TABLES_PACKETS,
    FAM_IPTABLES_MAX,
};

static metric_family_t fams[FAM_IPTABLES_MAX] = {
    [FAM_IPTABLES_BYTES] = {
        .name = "system_iptables_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IPTABLES_PACKETS] = {
        .name = "system_iptables_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6TABLES_BYTES] = {
        .name = "system_ip6tables_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6TABLES_PACKETS] = {
        .name = "system_ip6tables_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
};

/*
 * iptc_handle_t was available before libiptc was officially available as a
 * shared library. Note, that when the shared lib was introduced, the API and
 * ABI have changed slightly:
 * 'iptc_handle_t' used to be 'struct iptc_handle *' and most functions used
 * 'iptc_handle_t *' as an argument. Now, most functions use 'struct
 * iptc_handle *' (thus removing one level of pointer indirection).
 *
 * HAVE_IPTC_HANDLE_T is used to determine which API ought to be used. While
 * this is somewhat hacky, I didn't find better way to solve that :-/
 * -tokkee
 */
#ifndef HAVE_IPTC_HANDLE_T
typedef struct iptc_handle iptc_handle_t;
#endif
#ifndef HAVE_IP6TC_HANDLE_T
typedef struct ip6tc_handle ip6tc_handle_t;
#endif

typedef enum {
    IPV4,
    IPV6
} protocol_version_t;

/*
 * Each table/chain combo that will be queried goes into this list
 */
#ifndef XT_TABLE_MAXNAMELEN
#define XT_TABLE_MAXNAMELEN 32
#endif
typedef struct {
    protocol_version_t ip_version;
    char table[XT_TABLE_MAXNAMELEN];
    char chain[XT_TABLE_MAXNAMELEN];
    union {
        int num;
        char *comment;
    } rule;
    enum { RTYPE_NUM, RTYPE_COMMENT, RTYPE_COMMENT_ALL } rule_type;
    char name[64];
} ip_chain_t;

static ip_chain_t **chain_list;
static int chain_num;

static int submit6_match(const struct ip6t_entry_match *match,
                         const struct ip6t_entry *entry,
                         const ip_chain_t *chain, int rule_num,
                         metric_family_t *fam_ip6t_bytes,
                         metric_family_t *fam_ip6t_packets)
{
    /* Select the rules to collect */
    if (chain->rule_type == RTYPE_NUM) {
        if (chain->rule.num != rule_num)
            return 0;
    } else {
        if (strcmp(match->u.user.name, "comment") != 0)
            return 0;
        if ((chain->rule_type == RTYPE_COMMENT) &&
            (strcmp(chain->rule.comment, (const char *)match->data) != 0))
            return 0;
    }

    char brule[21];
    const char *rule = NULL;
    if (chain->name[0] != '\0') {
        rule = chain->name;
    } else if (chain->rule_type == RTYPE_NUM) {
        ssnprintf(brule, sizeof(brule), "%i", chain->rule.num);
        rule = brule;
    } else {
        rule = (const char *)match->data;
    }

    metric_family_append(fam_ip6t_bytes, VALUE_COUNTER(entry->counters.bcnt), NULL,
                         &(label_pair_const_t){.name="table", .value=chain->table},
                         &(label_pair_const_t){.name="chain", .value=chain->chain},
                         &(label_pair_const_t){.name="rule", .value=rule},
                         NULL);

    metric_family_append(fam_ip6t_packets, VALUE_COUNTER(entry->counters.pcnt), NULL,
                         &(label_pair_const_t){.name="table", .value=chain->table},
                         &(label_pair_const_t){.name="chain", .value=chain->chain},
                         &(label_pair_const_t){.name="rule", .value=rule},
                         NULL);
    return 0;
}

/* This needs to return `int' for IPT_MATCH_ITERATE to work. */
static int submit_match(const struct ipt_entry_match *match,
                        const struct ipt_entry *entry, const ip_chain_t *chain,
                        int rule_num, metric_family_t *fam_ipt_bytes,
                        metric_family_t *fam_ipt_packets)
{
    /* Select the rules to collect */
    if (chain->rule_type == RTYPE_NUM) {
        if (chain->rule.num != rule_num)
            return 0;
    } else {
        if (strcmp(match->u.user.name, "comment") != 0)
            return 0;
        if ((chain->rule_type == RTYPE_COMMENT) &&
                (strcmp(chain->rule.comment, (const char *)match->data) != 0))
            return 0;
    }

    char brule[21];
    const char *rule = NULL;
    if (chain->name[0] != '\0') {
        rule = chain->name;
    } else if (chain->rule_type == RTYPE_NUM) {
        ssnprintf(brule, sizeof(brule), "%i", chain->rule.num);
        rule = brule;
    } else {
        rule = (const char *)match->data;
    }

    metric_family_append(fam_ipt_bytes, VALUE_COUNTER((uint64_t)entry->counters.bcnt), NULL,
                         &(label_pair_const_t){.name="table", .value=chain->table},
                         &(label_pair_const_t){.name="chain", .value=chain->chain},
                         &(label_pair_const_t){.name="rule", .value=rule},
                         NULL);

    metric_family_append(fam_ipt_packets, VALUE_COUNTER((uint64_t)entry->counters.pcnt), NULL,
                         &(label_pair_const_t){.name="table", .value=chain->table},
                         &(label_pair_const_t){.name="chain", .value=chain->chain},
                         &(label_pair_const_t){.name="rule", .value=rule},
                         NULL);

    return 0;
}

/* ipv6 submit_chain */
static void submit6_chain(ip6tc_handle_t *handle, ip_chain_t *chain,
                          metric_family_t *fam_ip6t_bytes,
                          metric_family_t *fam_ip6t_packets)
{

    /* Find first rule for chain and use the iterate macro */
    const struct ip6t_entry *entry = ip6tc_first_rule(chain->chain, handle);
    if (entry == NULL) {
        PLUGIN_DEBUG("ip6tc_first_rule failed: %s", ip6tc_strerror(errno));
        return;
    }

    int rule_num = 1;
    while (entry) {
        if (chain->rule_type == RTYPE_NUM) {
            submit6_match(NULL, entry, chain, rule_num, fam_ip6t_bytes, fam_ip6t_packets);
        } else {
            IP6T_MATCH_ITERATE(entry, submit6_match, entry, chain, rule_num,
                                      fam_ip6t_bytes, fam_ip6t_packets);
        }

        entry = ip6tc_next_rule(entry, handle);
        rule_num++;
    }
}

/* ipv4 submit_chain */
static void submit_chain(iptc_handle_t *handle, ip_chain_t *chain,
                         metric_family_t *fam_ipt_bytes,
                         metric_family_t *fam_ipt_packets)
{
    const struct ipt_entry *entry;
    int rule_num;

    /* Find first rule for chain and use the iterate macro */
    entry = iptc_first_rule(chain->chain, handle);
    if (entry == NULL) {
        PLUGIN_DEBUG("iptc_first_rule failed: %s", iptc_strerror(errno));
        return;
    }

    rule_num = 1;
    while (entry) {
        if (chain->rule_type == RTYPE_NUM) {
            submit_match(NULL, entry, chain, rule_num, fam_ipt_bytes, fam_ipt_packets);
        } else {
            IPT_MATCH_ITERATE(entry, submit_match, entry, chain, rule_num,
                                     fam_ipt_bytes, fam_ipt_packets);
        }

        entry = iptc_next_rule(entry, handle);
        rule_num++;
    }
}

static int iptables_read(void)
{
    int num_failures = 0;

    /* Init the iptc handle structure and query the correct table */
    for (int i = 0; i < chain_num; i++) {
        ip_chain_t *chain = chain_list[i];

        if (!chain) {
            PLUGIN_DEBUG("chain == NULL");
            continue;
        }

        if (chain->ip_version == IPV4) {
#ifdef HAVE_IPTC_HANDLE_T
            iptc_handle_t _handle;
            iptc_handle_t *handle = &_handle;

            *handle = iptc_init(chain->table);
#else
            iptc_handle_t *handle;
            handle = iptc_init(chain->table);
#endif

            if (!handle) {
                PLUGIN_ERROR("iptc_init (%s) failed: %s", chain->table, iptc_strerror(errno));
                num_failures++;
                continue;
            }

            submit_chain(handle, chain,
                         &fams[FAM_IPTABLES_BYTES], &fams[FAM_IPTABLES_PACKETS]);


            iptc_free(handle);
        } else if (chain->ip_version == IPV6) {
#ifdef HAVE_IP6TC_HANDLE_T
            ip6tc_handle_t _handle;
            ip6tc_handle_t *handle = &_handle;

            *handle = ip6tc_init(chain->table);
#else
            ip6tc_handle_t *handle;
            handle = ip6tc_init(chain->table);
#endif
            if (!handle) {
                PLUGIN_ERROR("ip6tc_init (%s) failed: %s", chain->table, ip6tc_strerror(errno));
                num_failures++;
                continue;
            }

            submit6_chain(handle, chain,
                          &fams[FAM_IP6TABLES_BYTES], &fams[FAM_IP6TABLES_PACKETS]);
            ip6tc_free(handle);
        } else
            num_failures++;
    }

    plugin_dispatch_metric_family_array(fams, FAM_IPTABLES_MAX, 0);

    return (num_failures < chain_num) ? 0 : -1;
}



static int iptables_config_rule(const config_item_t *ci, protocol_version_t ip_version)
{
    /*  Chain[6] <table> <chain> [<comment|num> [name]] */
    if (ci->values_num < 2) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires at least two arguments.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if ((ci->values[0].type != CONFIG_TYPE_STRING) ||
        (ci->values[1].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires at least two string arguments.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    ip_chain_t temp = {0};
    /* set IPv4 or IPv6 */
    temp.ip_version = ip_version;
    sstrncpy(temp.table, ci->values[0].value.string, sizeof(temp.table));
    sstrncpy(temp.chain, ci->values[1].value.string, sizeof(temp.chain));

    if (ci->values_num >= 3) {
        char *comment = NULL;
        int rule = 0;
        if (ci->values[2].type == CONFIG_TYPE_STRING) {
            comment = ci->values[2].value.string;
        } else if (ci->values[2].type == CONFIG_TYPE_NUMBER) {
            rule = (int)ci->values[2].value.number;
        } else {
            return -1;
        }

        if (comment != NULL)
            rule = atoi(comment);

        if (rule) {
            temp.rule.num = rule;
            temp.rule_type = RTYPE_NUM;
        } else if (comment != NULL){
            temp.rule.comment = strdup(comment);
            if (temp.rule.comment == NULL) {
                return -1;
            }
            temp.rule_type = RTYPE_COMMENT;
        }
    } else {
        temp.rule_type = RTYPE_COMMENT_ALL;
    }

    if (ci->values_num >= 4) {
        if (ci->values[3].type != CONFIG_TYPE_STRING) {
            PLUGIN_ERROR("The '%s' option in %s:%d requires that the "
                         "fourth argument to be a string.",
                         ci->key, cf_get_file(ci), cf_get_lineno(ci));
            free(temp.rule.comment);
            return -1;
        }
        sstrncpy(temp.name, ci->values[3].value.string, sizeof(temp.name));
    }

    ip_chain_t **list = realloc(chain_list, (chain_num + 1) * sizeof(ip_chain_t *));
    if (list == NULL) {
        PLUGIN_ERROR("realloc failed: %s", STRERRNO);
        free(temp.rule.comment);
        return 1;
    }

    chain_list = list;
    ip_chain_t *final = malloc(sizeof(*final));
    if (final == NULL) {
        PLUGIN_ERROR("malloc failed: %s", STRERRNO);
        free(temp.rule.comment);
        return 1;
    }
    memcpy(final, &temp, sizeof(temp));
    chain_list[chain_num] = final;
    chain_num++;

    PLUGIN_DEBUG("Chain #%i: table = %s; chain = %s;", chain_num, final->table, final->chain);
    return 0;
}

static int iptables_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "chain") == 0) {
            status = iptables_config_rule(child, IPV4);
        } else if (strcasecmp(child->key, "chain6") == 0) {
            status = iptables_config_rule(child, IPV6);
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

static int iptables_shutdown(void)
{
    for (int i = 0; i < chain_num; i++) {
        if ((chain_list[i] != NULL) && (chain_list[i]->rule_type == RTYPE_COMMENT))
            free(chain_list[i]->rule.comment);
        free(chain_list[i]);
    }
    free(chain_list);

    return 0;
}

static int iptables_init(void)
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
    plugin_register_config("iptables", iptables_config);
    plugin_register_init("iptables", iptables_init);
    plugin_register_read("iptables", iptables_read);
    plugin_register_shutdown("iptables", iptables_shutdown);
}
