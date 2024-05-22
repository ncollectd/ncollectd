// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2020  Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2009-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Sune Marcher <sm at flork.dk>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/kstat.h"
#include "libutils/exclist.h"
#include "interface.h"

#ifdef HAVE_SYS_TYPES_H
#    include <sys/types.h>
#endif

/* One cannot include both. This sucks. */
#ifdef HAVE_LINUX_IF_H
#    include <linux/if.h>
#elif defined(HAVE_NET_IF_H)
#    include <net/if.h>
#endif

#ifdef HAVE_LINUX_NETDEVICE_H
#    include <linux/netdevice.h>
#endif

#ifdef HAVE_IFADDRS_H
#    include <ifaddrs.h>
#endif


extern exclist_t excl_interface;
extern bool report_inactive;
extern bool unique_name;
extern metric_family_t fams[FAM_INTERFACE_MAX];

#define MAX_NUMIF 256
static kstat_ctl_t *kc;
static kstat_t *ksp[MAX_NUMIF];
static size_t numif;

int interface_read(void)
{
    if (kc == NULL)
        return -1;

    if (kstat_chain_update(kc) < 0) {
        PLUGIN_ERROR("kstat_chain_update failed.");
        return -1;
    }

    for (size_t i = 0; i < numif; i++) {
        if (kstat_read(kc, ksp[i], NULL) == -1)
            continue;

        char iname[128];
        if (unique_name) {
            ssnprintf(iname, sizeof(iname), "%s_%d_%s",
                             ksp[i]->ks_module, ksp[i]->ks_instance, ksp[i]->ks_name);
        } else {
            sstrncpy(iname, ksp[i]->ks_name, sizeof(iname));
        }

        struct {
            int fam;
            char const *name;
            char const *fallback_name;
        } metrics[] = {
            {FAM_INTERFACE_RX_BYTES,   "rbytes64",   "rbytes"  },
            {FAM_INTERFACE_RX_PACKETS, "ipackets64", "ipackets"},
            {FAM_INTERFACE_RX_ERRORS,  "ierrors",    NULL      },
            {FAM_INTERFACE_TX_BYTES,   "obytes64",   "obytes"  },
            {FAM_INTERFACE_TX_PACKETS, "opackets64", "opackets"},
            {FAM_INTERFACE_TX_ERRORS,  "oerrors",    NULL      },
        };

        for (size_t j = 0; j < STATIC_ARRAY_SIZE(metrics); j++) {
            long long value = get_kstat_value(ksp[i], metrics[j].name);
            if ((value == -1) && (metrics[j].fallback_name != NULL))
                value = get_kstat_value(ksp[i], metrics[j].fallback_name);

            if (value == -1)
                continue;

            metric_family_append(&fams[metrics[j].fam], VALUE_COUNTER(value), NULL,
                                 &(label_pair_const_t){.name="interface", .value=iname}, NULL);
        }
    }

    plugin_dispatch_metric_family_array(fams, FAM_INTERFACE_MAX, 0);

    return 0;
}

int interface_init(void)
{
    kstat_t *ksp_chain;

    numif = 0;

    if (kc == NULL)
        kc = kstat_open();

    if (kc == NULL) {
        PLUGIN_ERROR("kstat_open failed.");
        return -1;
    }

    for (numif = 0, ksp_chain = kc->kc_chain; (numif < MAX_NUMIF) && (ksp_chain != NULL);
                    ksp_chain = ksp_chain->ks_next) {
        if (strncmp(ksp_chain->ks_class, "net", 3))
            continue;
        if (ksp_chain->ks_type != KSTAT_TYPE_NAMED)
            continue;
        if (kstat_read(kc, ksp_chain, NULL) == -1)
            continue;
        if (get_kstat_value(ksp_chain, "obytes") == -1LL)
            continue;
        ksp[numif++] = ksp_chain;
    }

    return 0;
}

int interface_shutdown(void)
{
    exclist_reset(&excl_interface);
    return 0;
}
