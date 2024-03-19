// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2005-2012  Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2009-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#include <libperfstat.h>
#include <sys/protosw.h>

/* XINTFRAC was defined in libperfstat.h somewhere between AIX 5.3 and 6.1 */
#ifndef XINTFRAC
#    include <sys/systemcfg.h>
#    define XINTFRAC ((double)(_system_configuration.Xint)/(double)(_system_configuration.Xfrac))
#endif

#ifndef HTIC2SEC
#    define HTIC2SEC(x)  (((double)(x) * XINTFRAC) * 1e-9)
#endif

#include "disk.h"

extern exclist_t excl_disk;
extern char *conf_udev_name_attr;
extern metric_family_t fams[];

static int pnumdisk;
static perfstat_disk_t *stat_disk;

typedef struct diskstats {
    char *name;
    unsigned int poll_count;
    uint64_t read_ops;
    uint64_t write_ops;
    uint64_t read_time;
    uint64_t write_time;
    double avg_read_time;
    double avg_write_time;
    struct diskstats *next;
} diskstats_t;

static diskstats_t *disklist;

static double disk_calc_time_incr(int64_t delta_time, int64_t delta_ops)
{
    double interval = CDTIME_T_TO_DOUBLE(plugin_get_interval());
    double avg_time = (double)HTIC2SEC(delta_time) / (double)delta_ops;
    return interval * avg_time;
}

int disk_read(void)
{
    diskstats_t *ds, *pre_ds;

    int numdisk = perfstat_disk(NULL, NULL, sizeof(perfstat_disk_t), 0);
    if (numdisk < 0) {
        PLUGIN_WARNING("perfstat_disk: %s", STRERRNO);
        return -1;
    }

    if (numdisk != pnumdisk || stat_disk == NULL) {
        free(stat_disk);
        stat_disk = calloc(numdisk, sizeof(*stat_disk));
    }
    pnumdisk = numdisk;

    perfstat_id_t firstpath;
    firstpath.name[0] = '\0';
    int rnumdisk = perfstat_disk(&firstpath, stat_disk, sizeof(perfstat_disk_t), numdisk);
    if (rnumdisk < 0) {
        PLUGIN_WARNING("perfstat_disk : %s", STRERRNO);
        return -1;
    }

    for (int i = 0; i < rnumdisk; i++) {
        char *disk_name = stat_disk[i].name;

        if (!exclist_match(&excl_disk, disk_name))
            continue;

        for (ds = disklist, pre_ds = disklist; ds != NULL; pre_ds = ds, ds = ds->next) {
            if (strcmp(disk_name, ds->name) == 0)
                break;
        }

        if (ds == NULL) {
            ds = (diskstats_t *) calloc (1, sizeof (diskstats_t));
            if (ds == NULL)
                continue;

            if ((ds->name = strdup (disk_name)) == NULL) {
                free (ds);
                continue;
            }

            if (pre_ds == NULL) {
                disklist = ds;
            } else {
                pre_ds->next = ds;
            }
        }

        uint64_t read_bytes = stat_disk[i].rblks*stat_disk[i].bsize;
        uint64_t write_bytes = stat_disk[i].wblks*stat_disk[i].bsize;

        uint64_t read_time = stat_disk[i].rserv;
        uint64_t write_time = stat_disk[i].wserv;

        uint64_t read_ops = stat_disk[i].xrate;
        uint64_t write_ops = stat_disk[i].xfers - stat_disk[i].xrate;

        uint64_t diff_read_ops = read_ops - ds->read_ops;
        uint64_t diff_write_ops = write_ops - ds->write_ops;

        uint64_t diff_read_time = read_time - ds->read_time;
        uint64_t diff_write_time = write_time - ds->write_time;

        if (diff_read_ops != 0)
            ds->avg_read_time += disk_calc_time_incr(diff_read_time, diff_read_ops);
        if (diff_write_ops != 0)
            ds->avg_write_time += disk_calc_time_incr(diff_write_time, diff_write_ops);

        ds->read_ops = read_ops;
        ds->read_time = read_time;

        ds->write_ops = write_ops;
        ds->write_time = write_time;

        if (ds->poll_count == 0)
           continue;
        ds->poll_count++;

        if ((read_ops == 0) && (write_ops == 0)) {
           PLUGIN_DEBUG ("disk plugin: ((read_ops == 0) && (write_ops == 0)); => Not writing.");
           continue;
        }

        metric_family_append(&fams[FAM_DISK_READ_BYTES], VALUE_COUNTER(read_bytes), NULL,
                             &(label_pair_const_t){.name="device", .value=disk_name}, NULL);
        metric_family_append(&fams[FAM_DISK_WRITE_BYTES], VALUE_COUNTER(write_bytes), NULL,
                             &(label_pair_const_t){.name="device", .value=disk_name}, NULL);

        metric_family_append(&fams[FAM_DISK_READ_OPS], VALUE_COUNTER(read_ops), NULL,
                             &(label_pair_const_t){.name="device", .value=disk_name}, NULL);
        metric_family_append(&fams[FAM_DISK_WRITE_OPS], VALUE_COUNTER(write_ops), NULL,
                             &(label_pair_const_t){.name="device", .value=disk_name}, NULL);

        metric_family_append(&fams[FAM_DISK_READ_TIME],
                             VALUE_COUNTER_FLOAT64(HTIC2SEC(read_time)), NULL,
                             &(label_pair_const_t){.name="device", .value=disk_name}, NULL);
        metric_family_append(&fams[FAM_DISK_WRITE_TIME],
                             VALUE_COUNTER_FLOAT64(HTIC2SEC(write_time)), NULL,
                             &(label_pair_const_t){.name="device", .value=disk_name}, NULL);

        metric_family_append(&fams[FAM_DISK_READ_WEIGHTED_TIME],
                             VALUE_COUNTER_FLOAT64(ds->avg_read_time), NULL,
                             &(label_pair_const_t){.name="device", .value=disk_name}, NULL);
        metric_family_append(&fams[FAM_DISK_WRITE_WEIGHTED_TIME],
                             VALUE_COUNTER_FLOAT64(ds->avg_write_time), NULL,
                             &(label_pair_const_t){.name="device", .value=disk_name}, NULL);

        metric_family_append(&fams[FAM_DISK_IO_TIME],
                             VALUE_COUNTER_FLOAT64(HTIC2SEC(stat_disk[i].time)), NULL,
                             &(label_pair_const_t){.name="device", .value=disk_name}, NULL);

        metric_family_append(&fams[FAM_DISK_PENDING_OPERATIONS],
                             VALUE_GAUGE(stat_disk[i].qdepth), NULL,
                             &(label_pair_const_t){.name="device", .value=disk_name}, NULL);

        metric_family_append(&fams[FAM_DISK_READ_TIMEOUT],
                             VALUE_COUNTER(stat_disk[i].rtimeout), NULL,
                             &(label_pair_const_t){.name="device", .value=disk_name}, NULL);
        metric_family_append(&fams[FAM_DISK_WRITE_TIMEOUT],
                             VALUE_COUNTER(stat_disk[i].wtimeout), NULL,
                             &(label_pair_const_t){.name="device", .value=disk_name}, NULL);

        metric_family_append(&fams[FAM_DISK_READ_FAILED],
                             VALUE_COUNTER(stat_disk[i].rfailed), NULL,
                             &(label_pair_const_t){.name="device", .value=disk_name}, NULL);
        metric_family_append(&fams[FAM_DISK_WRITE_FAILED],
                             VALUE_COUNTER(stat_disk[i].wfailed), NULL,
                             &(label_pair_const_t){.name="device", .value=disk_name}, NULL);
    }

    plugin_dispatch_metric_family_array(fams, FAM_DISK_MAX, 0);
    return 0;
}

int disk_shutdown(void)
{
    if (stat_disk != NULL)
        free(stat_disk);

    exclist_reset(&excl_disk);

    return 0;
}
