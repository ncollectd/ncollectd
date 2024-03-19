// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2009,2010  Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor:  Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"
#include "plugins/protocols/flags.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

static exclist_t excl_value;
static uint64_t protocols_flags =
    COLLECT_IP  | COLLECT_ICMP  | COLLECT_UDP  | COLLECT_UDPLITE  |
    COLLECT_IP6 | COLLECT_ICMP6 | COLLECT_UDP6 | COLLECT_UDPLITE6 |
    COLLECT_TCP | COLLECT_MPTCP | COLLECT_SCTP ;

static cf_flags_t protocols_flags_list[] = {
    {"ip",       COLLECT_IP       },
    {"icmp",     COLLECT_ICMP     },
    {"udp",      COLLECT_UDP      },
    {"udplite",  COLLECT_UDPLITE  },
    {"udplite6", COLLECT_UDPLITE6 },
    {"ip6",      COLLECT_IP6      },
    {"icmp6",    COLLECT_ICMP6    },
    {"udp6",     COLLECT_UDP6     },
    {"tcp",      COLLECT_TCP      },
    {"mptcp",    COLLECT_MPTCP    },
    {"sctp",     COLLECT_SCTP     },
};


int netstat_init(void);
int netstat_shutdown(void);
int netstat_read(uint64_t flags, exclist_t *excl_value);

int snmp_init(void);
int snmp_shutdown(void);
int snmp_read(uint64_t flags, exclist_t *excl_value);

int snmp6_init(void);
int snmp6_shutdown(void);
int snmp6_read(uint64_t flags, exclist_t *excl_value);

int sctp_init(void);
int sctp_shutdown(void);
int sctp_read(uint64_t flags, exclist_t *excl_value);

static int protocols_read(void)
{
    netstat_read(protocols_flags, &excl_value);
    snmp_read(protocols_flags, &excl_value);
    snmp6_read(protocols_flags, &excl_value);
    sctp_read(protocols_flags, &excl_value);

    return 0;
}

static int protocols_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "key") == 0) {
            status = cf_util_exclist(child, &excl_value);
        } else if (strcasecmp(child->key, "collect") == 0) {
            status = cf_util_get_flags(child, protocols_flags_list,
                                       STATIC_ARRAY_SIZE(protocols_flags_list), &protocols_flags);
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

static int protocols_init(void)
{
    netstat_init();
    snmp_init();
    snmp6_init();
    sctp_init();

    return 0;
}

static int protocols_shutdown(void)
{
    exclist_reset(&excl_value);

    netstat_shutdown();
    snmp_shutdown();
    snmp6_shutdown();
    sctp_shutdown();

    return 0;
}

void module_register(void)
{
    plugin_register_init("protocols", protocols_init);
    plugin_register_config("protocols", protocols_config);
    plugin_register_read("protocols", protocols_read);
    plugin_register_shutdown("protocols", protocols_shutdown);
}
