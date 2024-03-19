// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2012 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2009-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#include <sys/iostat.h>
#include <sys/sysctl.h>

#include "disk.h"

extern exclist_t excl_disk;
extern metric_family_t fams[];

static struct io_sysctl *drives = NULL;
static size_t ndrive = 0;

int disk_read(void)
{
    int mib[3];
    size_t size, i, nndrive;

    /* figure out number of drives */
    mib[0] = CTL_HW;
    mib[1] = HW_IOSTATS;
    mib[2] = sizeof(struct io_sysctl);
    if (sysctl(mib, 3, NULL, &size, NULL, 0) == -1) {
        PLUGIN_ERROR("sysctl for ndrives failed");
        return -1;
    }
    nndrive = size / sizeof(struct io_sysctl);

    if (size == 0) {
        PLUGIN_ERROR("no drives found");
        return -1;
    }
    /* number of drives changed, reallocate buffer */
    if (nndrive != ndrive) {
        drives = (struct io_sysctl *)realloc(drives, size);
        if (drives == NULL) {
            PLUGIN_ERROR("memory allocation failure");
            return -1;
        }
        ndrive = nndrive;
    }

    /* get stats for all drives */
    mib[0] = CTL_HW;
    mib[1] = HW_IOSTATS;
    mib[2] = sizeof(struct io_sysctl);
    if (sysctl(mib, 3, drives, &size, NULL, 0) == -1) {
        PLUGIN_ERROR("sysctl for drive stats failed");
        return -1;
    }

    for (i = 0; i < ndrive; i++) {
        if (drives[i].type != IOSTAT_DISK)
            continue;
        if (!exclist_match(&excl_disk, drives[i].name))
            continue;

        metric_family_append(&fams[FAM_DISK_READ_BYTES], VALUE_COUNTER(drives[i].rbytes), NULL,
                             &(label_pair_const_t){.name="device", .value=drives[i].name}, NULL);

        metric_family_append(&fams[FAM_DISK_WRITE_BYTES], VALUE_COUNTER(drives[i].wbytes), NULL,
                             &(label_pair_const_t){.name="device", .value=drives[i].name}, NULL);

        metric_family_append(&fams[FAM_DISK_READ_OPS], VALUE_COUNTER(drives[i].rxfer), NULL,
                             &(label_pair_const_t){.name="device", .value=drives[i].name}, NULL);

        metric_family_append(&fams[FAM_DISK_WRITE_OPS], VALUE_COUNTER(drives[i].wxfer), NULL,
                             &(label_pair_const_t){.name="device", .value=drives[i].name}, NULL);

        metric_family_append(&fams[FAM_DISK_IO_TIME],
                             VALUE_COUNTER_FLOAT64((double)drives[i].time_sec +
                                                   (double)drives[i].time_usec * 1e-6), NULL,
                             &(label_pair_const_t){.name="device", .value=drives[i].name}, NULL);
    }

    plugin_dispatch_metric_family_array(fams, FAM_DISK_MAX, 0);
    return 0;
}

int disk_init(void)
{
    int mib[3];
    /* figure out number of drives */
    mib[0] = CTL_HW;
    mib[1] = HW_IOSTATS;
    mib[2] = sizeof(struct io_sysctl);

    size_t size;
    if (sysctl(mib, 3, NULL, &size, NULL, 0) == -1) {
        PLUGIN_ERROR("sysctl for ndrives failed");
        return -1;
    }
    ndrive = size / sizeof(struct io_sysctl);

    if (size == 0) {
        PLUGIN_ERROR("no drives found");
        return -1;
    }

    drives = (struct io_sysctl *)malloc(size);
    if (drives == NULL) {
        PLUGIN_ERROR("memory allocation failure");
        return -1;
    }

    return 0;
}

int disk_shutdown(void)
{
    if (drives != NULL)
        free(drives);

    exclist_reset(&excl_disk);

    return 0;
}
