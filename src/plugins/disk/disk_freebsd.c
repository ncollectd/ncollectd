// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2012 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2009-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#include "disk.h"

#include <devstat.h>
#include <libgeom.h>

extern exclist_t excl_disk;
extern char *conf_udev_name_attr;
extern metric_family_t fams[];

static struct gmesh geom_tree;

int disk_read(void)
{
    int retry, dirty;

    void *snap = NULL;
    struct devstat *snap_iter;

    struct gident *geom_id;

    const char *disk_name;

    for (retry = 0, dirty = 1; retry < 5 && dirty == 1; retry++) {
        if (snap != NULL)
            geom_stats_snapshot_free(snap);

        /* Get a fresh copy of stats snapshot */
        snap = geom_stats_snapshot_get();
        if (snap == NULL) {
            PLUGIN_ERROR("geom_stats_snapshot_get() failed.");
            return -1;
        }

        /* Check if we have dirty read from this snapshot */
        dirty = 0;
        geom_stats_snapshot_reset(snap);
        while ((snap_iter = geom_stats_snapshot_next(snap)) != NULL) {
            if (snap_iter->id == NULL)
                continue;
            geom_id = geom_lookupid(&geom_tree, snap_iter->id);

            /* New device? refresh GEOM tree */
            if (geom_id == NULL) {
                geom_deletetree(&geom_tree);
                if (geom_gettree(&geom_tree) != 0) {
                    PLUGIN_ERROR("geom_gettree() failed");
                    geom_stats_snapshot_free(snap);
                    return -1;
                }
                geom_id = geom_lookupid(&geom_tree, snap_iter->id);
            }
            /*
             * This should be rare: the device come right before we take the
             * snapshot and went away right after it.  We will handle this
             * case later, so don't mark dirty but silently ignore it.
             */
            if (geom_id == NULL)
                continue;

            /* Only collect PROVIDER data */
            if (geom_id->lg_what != ISPROVIDER)
                continue;

            /* Only collect data when rank is 1 (physical devices) */
            if (((struct gprovider *)(geom_id->lg_ptr))->lg_geom->lg_rank != 1)
                continue;

            /* Check if this is a dirty read quit for another try */
            if (snap_iter->sequence0 != snap_iter->sequence1) {
                dirty = 1;
                break;
            }
        }
    }

    /* Reset iterator */
    geom_stats_snapshot_reset(snap);
    for (;;) {
        snap_iter = geom_stats_snapshot_next(snap);
        if (snap_iter == NULL)
            break;

        if (snap_iter->id == NULL)
            continue;
        geom_id = geom_lookupid(&geom_tree, snap_iter->id);
        if (geom_id == NULL)
            continue;
        if (geom_id->lg_what != ISPROVIDER)
            continue;
        if (((struct gprovider *)(geom_id->lg_ptr))->lg_geom->lg_rank != 1)
            continue;
        /* Skip dirty reads, if present */
        if (dirty && (snap_iter->sequence0 != snap_iter->sequence1))
            continue;

        disk_name = ((struct gprovider *)geom_id->lg_ptr)->lg_name;

        if (!exclist_match(&excl_disk, disk_name))
            continue;

        if ((snap_iter->bytes[DEVSTAT_READ] != 0) || (snap_iter->bytes[DEVSTAT_WRITE] != 0)) {
            metric_family_append(&fams[FAM_DISK_READ_BYTES],
                                 VALUE_COUNTER(snap_iter->bytes[DEVSTAT_READ]), NULL,
                                 &(label_pair_const_t){.name="device", .value=disk_name}, NULL);

            metric_family_append(&fams[FAM_DISK_WRITE_BYTES],
                                 VALUE_COUNTER(snap_iter->bytes[DEVSTAT_WRITE]), NULL,
                                 &(label_pair_const_t){.name="device", .value=disk_name}, NULL);
        }

        if ((snap_iter->operations[DEVSTAT_READ] != 0) ||
                (snap_iter->operations[DEVSTAT_WRITE] != 0)) {
            metric_family_append(&fams[FAM_DISK_READ_OPS],
                                 VALUE_COUNTER(snap_iter->operations[DEVSTAT_READ]), NULL,
                                 &(label_pair_const_t){.name="device", .value=disk_name}, NULL);

            metric_family_append(&fams[FAM_DISK_WRITE_OPS],
                                 VALUE_COUNTER(snap_iter->operations[DEVSTAT_WRITE]), NULL,
                                 &(label_pair_const_t){.name="device", .value=disk_name}, NULL);
        }

        long double read_time = devstat_compute_etime(&snap_iter->duration[DEVSTAT_READ], NULL);
        long double write_time = devstat_compute_etime(&snap_iter->duration[DEVSTAT_WRITE], NULL);
        if ((read_time != 0) || (write_time != 0)) {
            metric_family_append(&fams[FAM_DISK_READ_TIME],
                                 VALUE_COUNTER_FLOAT64(read_time), NULL,
                                 &(label_pair_const_t){.name="device", .value=disk_name}, NULL);

            metric_family_append(&fams[FAM_DISK_WRITE_TIME],
                                 VALUE_COUNTER_FLOAT64(write_time), NULL,
                                 &(label_pair_const_t){.name="device", .value=disk_name}, NULL);
        }

        long double busy_time, total_duration;
        uint64_t queue_length;
        if (devstat_compute_statistics(snap_iter, NULL, 1.0,
                                                  DSM_TOTAL_BUSY_TIME, &busy_time,
                                                  DSM_TOTAL_DURATION, &total_duration,
                                                  DSM_QUEUE_LENGTH, &queue_length, DSM_NONE) != 0) {
            PLUGIN_WARNING("%s", devstat_errbuf);
        } else {
            metric_family_append(&fams[FAM_DISK_IO_TIME],
                                 VALUE_COUNTER_FLOAT64(busy_time), NULL,
                                 &(label_pair_const_t){.name="device", .value=disk_name}, NULL);

            metric_family_append(&fams[FAM_DISK_IO_WEIGHTED_TIME],
                                 VALUE_COUNTER_FLOAT64(total_duration), NULL,
                                 &(label_pair_const_t){.name="device", .value=disk_name}, NULL);

            metric_family_append(&fams[FAM_DISK_PENDING_OPERATIONS],
                                 VALUE_GAUGE(queue_length), NULL,
                                 &(label_pair_const_t){.name="device", .value=disk_name}, NULL);
        }
    }

    geom_stats_snapshot_free(snap);

    plugin_dispatch_metric_family_array(fams, FAM_DISK_MAX, 0);

    return 0;
}

int disk_init(void)
{
    int rv = geom_gettree(&geom_tree);
    if (rv != 0) {
        PLUGIN_ERROR("geom_gettree() failed, returned %d", rv);
        return -1;
    }

    rv = geom_stats_open();
    if (rv != 0) {
        PLUGIN_ERROR("geom_stats_open() failed, returned %d", rv);
        return -1;
    }
    return 0;
}

int disk_shutdown(void)
{
    exclist_reset(&excl_disk);

    return 0;
}
