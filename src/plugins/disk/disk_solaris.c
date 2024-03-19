// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2012 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2009-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#include <kstat.h>

#include "disk.h"

extern exclist_t excl_disk;
extern char *conf_udev_name_attr;
extern metric_family_t fams[];

#define MAX_NUMDISK 1024
static kstat_ctl_t *kc;
static kstat_t *ksp[MAX_NUMDISK];
static int numdisk;

#if defined(HAVE_KSTAT_IO_T_WRITES) && defined(HAVE_KSTAT_IO_T_NWRITES) && defined(HAVE_KSTAT_IO_T_WTIME)
#    define KIO_ROCTETS reads
#    define KIO_WOCTETS writes
#    define KIO_ROPS nreads
#    define KIO_WOPS nwrites
#    define KIO_RTIME rtime
#    define KIO_WTIME wtime
#elif defined(HAVE_KSTAT_IO_T_NWRITTEN) && defined(HAVE_KSTAT_IO_T_WRITES) && defined(HAVE_KSTAT_IO_T_WTIME)
#    define KIO_ROCTETS nread
#    define KIO_WOCTETS nwritten
#    define KIO_ROPS reads
#    define KIO_WOPS writes
#    define KIO_RTIME rtime
#    define KIO_WTIME wtime
#endif

int disk_read(void)
{
    static kstat_io_t kio;

    if (kc == NULL)
        return -1;

    if (kstat_chain_update(kc) < 0) {
        PLUGIN_ERROR("kstat_chain_update failed.");
        return -1;
    }

    for (int i = 0; i < numdisk; i++) {

        if (kstat_read(kc, ksp[i], &kio) == -1)
            continue;

        if (strncmp(ksp[i]->ks_class, "disk", 4) == 0) {
            if (!exclist_match(&excl_disk, ksp[i]->ks_name))
                continue;

            metric_family_append(&fams[FAM_DISK_READ_BYTES], VALUE_COUNTER(kio.KIO_ROCTETS), NULL,
                                 &(label_pair_t){.name="device", .value=ksp[i]->ks_name}, NULL);

            metric_family_append(&fams[FAM_DISK_WRITE_BYTES], VALUE_COUNTER(kio.KIO_WOCTETS), NULL,
                                 &(label_pair_t){.name="device", .value=ksp[i]->ks_name}, NULL);

            metric_family_append(&fams[FAM_DISK_READ_OPS], VALUE_COUNTER(kio.KIO_ROPS), NULL,
                                 &(label_pair_t){.name="device", .value=ksp[i]->ks_name}, NULL);

            metric_family_append(&fams[FAM_DISK_WRITE_OPS], VALUE_COUNTER(kio.KIO_WOPS), NULL,
                                 &(label_pair_t){.name="device", .value=ksp[i]->ks_name}, NULL);

            metric_family_append(&fams[FAM_DISK_READ_TIME],
                                 VALUE_COUNTER_FLOAT64((double)kio.KIO_RTIME * 1e-9), NULL,
                                 &(label_pair_t){.name="device", .value=ksp[i]->ks_name}, NULL);

            metric_family_append(&fams[FAM_DISK_WRITE_TIME],
                                 VALUE_COUNTER_FLOAT64((double)kio.KIO_WTIME * 1e-9), NULL,
                                 &(label_pair_t){.name="device", .value=ksp[i]->ks_name}, NULL);

        } else if (strncmp(ksp[i]->ks_class, "partition", 9) == 0) {
            if (!exclist_match(&excl_disk, ksp[i]->ks_name) != 0)
                continue;

            metric_family_append(&fams[FAM_DISK_READ_BYTES], VALUE_COUNTER(kio.KIO_ROCTETS), NULL,
                                 &(label_pair_t){.name="device", .value=ksp[i]->ks_name}, NULL);

            metric_family_append(&fams[FAM_DISK_WRITE_BYTES], VALUE_COUNTER(kio.KIO_WOCTETS), NULL,
                                 &(label_pair_t){.name="device", .value=ksp[i]->ks_name}, NULL);

            metric_family_append(&fams[FAM_DISK_READ_OPS], VALUE_COUNTER(kio.KIO_ROPS), NULL,
                                 &(label_pair_t){.name="device", .value=ksp[i]->ks_name}, NULL);

            metric_family_append(&fams[FAM_DISK_WRITE_OPS], VALUE_COUNTER(kio.KIO_WOPS), NULL,
                                 &(label_pair_t){.name="device", .value=ksp[i]->ks_name}, NULL);

        }
    }

    plugin_dispatch_metric_family_array(fams, FAM_DISK_MAX, 0);
    return 0;
}

int disk_init(void)
{
    kstat_t *ksp_chain;


    if (kc == NULL)
        kc = kstat_open();

    if (kc == NULL) {
        PLUGIN_ERROR("kstat_open failed.");
        return -1;
    }

    numdisk = 0;
    for (ksp_chain = kc->kc_chain; (numdisk < MAX_NUMDISK) && (ksp_chain != NULL);
         ksp_chain = ksp_chain->ks_next) {

        if ((strncmp(ksp_chain->ks_class, "disk", 4) != 0 )&&
            (strncmp(ksp_chain->ks_class, "partition", 9) != 0))
            continue;

        if (ksp_chain->ks_type != KSTAT_TYPE_IO)
            continue;

        ksp[numdisk++] = ksp_chain;
    }

    return 0;
}

int disk_shutdown(void)
{
    exclist_reset(&excl_disk);

    return 0;
}
