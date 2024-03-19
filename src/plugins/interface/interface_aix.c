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

#if HAVE_PERFSTAT
#include <libperfstat.h>
#include <sys/protosw.h>
#endif

extern exclist_t excl_device;
extern bool report_inactive;
extern metric_family_t fams[FAM_INTERFACE_MAX];

int interface_read(void)
{
    int num = perfstat_netinterface(NULL, NULL, sizeof(perfstat_netinterface_t), 0);
    if (num < 0) {
        PLUGIN_WARNING("perfstat_netinterface: %s", STRERRNO);
        return -1;
    }
    if (num == 0)
        return 0;

    perfstat_netinterface_t *ifstat = calloc(num, sizeof(perfstat_netinterface_t));
    if (ifstat == NULL)
        return errno;

    perfstat_id_t id = { .name = {0} };
    int status = perfstat_netinterface(&id, ifstat, sizeof(perfstat_netinterface_t), num);
    if (status < 0) {
        PLUGIN_ERROR("perfstat_netinterface (interfaces=%d): %s", num, STRERRNO);
        free(ifstat);
        return -1;
    }

    for (int i = 0; i < num; i++) {
        if (!report_inactive && (ifstat[i].ipackets == 0) && (ifstat[i].opackets == 0))
            continue;

        metric_family_append(&fams[FAM_INTERFACE_RX_BYTES],
                             VALUE_COUNTER(ifstat[i].ibytes), NULL,
                             &(label_pair_const_t){.name="device", .value=ifstat[i].name}, NULL);
        metric_family_append(&fams[FAM_INTERFACE_RX_PACKETS],
                             VALUE_COUNTER(ifstat[i].ipackets), NULL,
                             &(label_pair_const_t){.name="device", .value=ifstat[i].name}, NULL);
        metric_family_append(&fams[FAM_INTERFACE_RX_ERRORS],
                             VALUE_COUNTER(ifstat[i].ierrors), NULL,
                             &(label_pair_const_t){.name="device", .value=ifstat[i].name}, NULL);
        metric_family_append(&fams[FAM_INTERFACE_TX_BYTES],
                             VALUE_COUNTER(ifstat[i].obytes), NULL,
                             &(label_pair_const_t){.name="device", .value=ifstat[i].name}, NULL);
        metric_family_append(&fams[FAM_INTERFACE_TX_PACKETS],
                             VALUE_COUNTER(ifstat[i].opackets), NULL,
                             &(label_pair_const_t){.name="device", .value=ifstat[i].name}, NULL);
        metric_family_append(&fams[FAM_INTERFACE_TX_ERRORS],
                             VALUE_COUNTER(ifstat[i].oerrors), NULL,
                             &(label_pair_const_t){.name="device", .value=ifstat[i].name}, NULL);
    }

    free(ifstat);

    plugin_dispatch_metric_family_array(fams, FAM_INTERFACE_MAX, 0);

    return 0;
}

int interface_shutdown(void)
{
    exclist_reset(&excl_device);
    return 0;
}
