// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2012 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2009-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#include "disk.h"

#ifdef HAVE_LIBUDEV_H
#include <libudev.h>

static struct udev *handle_udev;
#endif

typedef struct diskstats {
    char *name;

    /* This overflows in roughly 1361 years */
    unsigned int poll_count;

    int64_t read_sectors;
    int64_t write_sectors;

    int64_t read_bytes;
    int64_t write_bytes;

    int64_t read_ops;
    int64_t write_ops;
    int64_t read_time;
    int64_t write_time;

    double avg_read_time;
    double avg_write_time;

    int64_t discard_ops;
    int64_t discard_sectors;
    int64_t discard_bytes;
    int64_t discard_time;
    double avg_discard_time;

    int64_t flush_ops;
    int64_t flush_time;
    double avg_flush_time;

    bool has_merged;
    bool has_in_progress;
    bool has_io_time;
    bool has_discard;
    bool has_flush;

    struct diskstats *next;
} diskstats_t;

static diskstats_t *disklist;

static char *path_proc_diskstats;

extern exclist_t excl_disk;
extern char *conf_udev_name_attr;
extern metric_family_t fams[];

static double disk_calc_time_incr(int64_t delta_time, int64_t delta_ops, double scale)
{
    double interval = CDTIME_T_TO_DOUBLE(plugin_get_interval());
    double avg_time = ((double)delta_time) / ((double)delta_ops);
    return interval * avg_time * scale;
}

#ifdef HAVE_LIBUDEV_H
static char *disk_udev_attr_name(struct udev *udev, char *disk_name, const char *attr)
{
    struct udev_device *dev;
    const char *prop;
    char *output = NULL;

    dev = udev_device_new_from_subsystem_sysname(udev, "block", disk_name);
    if (dev != NULL) {
        prop = udev_device_get_property_value(dev, attr);
        if (prop) {
            output = strdup(prop);
            PLUGIN_DEBUG("renaming %s => %s", disk_name, output);
        }
        udev_device_unref(dev);
    }
    return output;
}
#endif

int disk_read(void)
{
    static unsigned int poll_count = 0;

    int64_t read_sectors = 0;
    int64_t write_sectors = 0;

    int64_t read_ops = 0;
    int64_t read_merged = 0;
    int64_t read_time = 0;
    int64_t write_ops = 0;
    int64_t write_merged = 0;
    int64_t write_time = 0;
    double in_progress = NAN;
    int64_t io_time = 0;
    int64_t weighted_time = 0;
    int is_disk = 0;
    int64_t discard_ops = 0;
    int64_t discard_merged = 0;
    int64_t discard_sectors = 0;
    int64_t discard_time = 0;
    int64_t flush_ops = 0;
    int64_t flush_time = 0;

    diskstats_t *ds, *pre_ds;

    FILE *fh = fopen(path_proc_diskstats, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Cannot open '%s': %s", path_proc_diskstats, STRERRNO);
        return -1;
    }

    poll_count++;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *fields[32];
        int numfields = strsplit(buffer, fields, 32);

        /* need either 7 fields (partition) or at least 14 fields */
        if ((numfields != 7) && (numfields < 14))
            continue;

        char *disk_name = fields[2];

        for (ds = disklist, pre_ds = disklist; ds != NULL; pre_ds = ds, ds = ds->next) {
            if (strcmp(disk_name, ds->name) == 0)
                break;
        }

        if (ds == NULL) {
            if ((ds = calloc(1, sizeof(*ds))) == NULL)
                continue;

            if ((ds->name = strdup(disk_name)) == NULL) {
                free(ds);
                continue;
            }

            if (pre_ds == NULL)
                disklist = ds;
            else
                pre_ds->next = ds;
        }

        is_disk = 0;
        if (numfields == 7) {
            /* Kernel 2.6, Partition */
            read_ops = atoll(fields[3]);
            read_sectors = atoll(fields[4]);
            write_ops = atoll(fields[5]);
            write_sectors = atoll(fields[6]);
        } else {
            assert(numfields >= 14);
            read_ops = atoll(fields[3]);
            write_ops = atoll(fields[7]);

            read_sectors = atoll(fields[5]);
            write_sectors = atoll(fields[9]);

            is_disk = 1;
            read_merged = atoll(fields[4]);
            read_time = atoll(fields[6]);
            write_merged = atoll(fields[8]);
            write_time = atoll(fields[10]);

            in_progress = atof(fields[11]);

            io_time = atof(fields[12]);
            weighted_time = atof(fields[13]);
        }

        if (numfields >= 18) {
            discard_ops = atoll(fields[14]);
            discard_merged = atoll(fields[15]);
            discard_sectors = atoll(fields[16]);
            discard_time = atoll(fields[17]);
        }

        if (numfields >= 20) {
            flush_ops = atoll(fields[18]);
            flush_time = atoll(fields[19]);
        }

        int64_t diff_read_sectors = counter_diff(ds->read_sectors, read_sectors);
        ds->read_bytes += 512 * diff_read_sectors;
        ds->read_sectors = read_sectors;

        int64_t diff_write_sectors = counter_diff(ds->write_sectors, write_sectors);
        ds->write_bytes += 512 * diff_write_sectors;
        ds->write_sectors = write_sectors;

        /* Calculate the average time an io-op needs to complete */
        if (is_disk) {
            int64_t diff_read_ops = counter_diff(ds->read_ops, read_ops);
            ds->read_ops = read_ops;

            int64_t diff_write_ops = counter_diff(ds->write_ops, write_ops);
            ds->write_ops = write_ops;

            int64_t diff_read_time = counter_diff(ds->read_time, read_time);
            ds->read_time = read_time;

            if (diff_read_ops != 0)
                ds->avg_read_time += disk_calc_time_incr(diff_read_time, diff_read_ops, 1e-3);

            int64_t diff_write_time = counter_diff(ds->write_time, write_time);
            ds->write_time = write_time;
            if (diff_write_ops != 0)
                ds->avg_write_time += disk_calc_time_incr(diff_write_time, diff_write_ops, 1e-3);

            if (read_merged || write_merged)
                ds->has_merged = true;

            if (in_progress)
                ds->has_in_progress = true;

            if (io_time)
                ds->has_io_time = true;

            if (discard_time) {
                int64_t diff_discard_ops = counter_diff(ds->discard_ops, discard_ops);
                ds->discard_ops = discard_ops;

                int64_t diff_discard_sectors = counter_diff(ds->discard_sectors, discard_sectors);
                ds->discard_bytes += 512 * diff_discard_sectors;
                ds->discard_sectors = discard_sectors;

                int64_t diff_discard_time = counter_diff(ds->discard_time, discard_time);
                ds->discard_time = discard_time;

                if (diff_discard_ops != 0)
                    ds->avg_discard_time += disk_calc_time_incr(diff_discard_time,
                                                                diff_discard_ops, 1e-3);

                ds->has_discard = true;
            }

            if (flush_time) {
                int64_t diff_flush_ops = counter_diff(ds->flush_ops, flush_ops);
                ds->flush_ops = flush_ops;

                int64_t diff_flush_time = counter_diff(ds->flush_time, flush_time);
                ds->flush_time = flush_time;

                if (diff_flush_ops != 0)
                    ds->avg_flush_time += disk_calc_time_incr(diff_flush_time,
                                                              diff_flush_ops, 1e-3);

                ds->has_flush = true;
            }
        }

        /* Skip first cycle for newly-added disk */
        if (ds->poll_count == 0) {
            PLUGIN_DEBUG("(ds->poll_count = 0) => Skipping.");
            ds->poll_count = poll_count;
            continue;
        }
        ds->poll_count = poll_count;

        if ((read_ops == 0) && (write_ops == 0)) {
            PLUGIN_DEBUG("((read_ops == 0) && (write_ops == 0)); => Not writing.");
            continue;
        }

        char *output_name = disk_name;

#ifdef HAVE_LIBUDEV_H
        char *alt_name = NULL;
        if (conf_udev_name_attr != NULL) {
            alt_name = disk_udev_attr_name(handle_udev, disk_name, conf_udev_name_attr);
            if (alt_name != NULL)
                output_name = alt_name;
        }
#endif

        if (!exclist_match(&excl_disk, output_name)) {
#ifdef HAVE_LIBUDEV_H
            /* release udev-based alternate name, if allocated */
            free(alt_name);
#endif
            continue;
        }

        label_pair_const_t device_label = {.name="device", .value=output_name};

        if ((ds->read_bytes != 0) || (ds->write_bytes != 0)) {
            metric_family_append(&fams[FAM_DISK_READ_BYTES],
                                 VALUE_COUNTER(ds->read_bytes), NULL, &device_label, NULL);

            metric_family_append(&fams[FAM_DISK_WRITE_BYTES],
                                 VALUE_COUNTER(ds->write_bytes), NULL, &device_label, NULL);
        }

        if ((ds->read_ops != 0) || (ds->write_ops != 0)) {
            metric_family_append(&fams[FAM_DISK_READ_OPS],
                                 VALUE_COUNTER(read_ops), NULL, &device_label, NULL);

            metric_family_append(&fams[FAM_DISK_WRITE_OPS],
                                 VALUE_COUNTER(write_ops), NULL, &device_label, NULL);
        }

        if ((ds->read_time != 0) || (ds->write_time != 0)) {
            metric_family_append(&fams[FAM_DISK_READ_TIME],
                                 VALUE_COUNTER_FLOAT64((double)read_time/1000.0),
                                 NULL, &device_label, NULL);

            metric_family_append(&fams[FAM_DISK_WRITE_TIME],
                                 VALUE_COUNTER_FLOAT64((double)write_time/1000.0),
                                 NULL, &device_label, NULL);
        }

        if ((ds->avg_read_time != 0) || (ds->avg_write_time != 0)) {
            metric_family_append(&fams[FAM_DISK_READ_WEIGHTED_TIME],
                                 VALUE_COUNTER_FLOAT64(ds->avg_read_time),
                                 NULL, &device_label, NULL);

            metric_family_append(&fams[FAM_DISK_WRITE_WEIGHTED_TIME],
                                 VALUE_COUNTER_FLOAT64(ds->avg_write_time),
                                 NULL, &device_label, NULL);
        }

        if (is_disk) {
            if (ds->has_merged) {
                metric_family_append(&fams[FAM_DISK_READ_MERGED],
                                     VALUE_COUNTER(read_merged), NULL, &device_label, NULL);

                metric_family_append(&fams[FAM_DISK_WRITE_MERGED],
                                     VALUE_COUNTER(write_merged), NULL, &device_label, NULL);
            }
            if (ds->has_in_progress)
                metric_family_append(&fams[FAM_DISK_PENDING_OPERATIONS],
                                     VALUE_GAUGE(in_progress), NULL, &device_label, NULL);
            if (ds->has_io_time) {
                metric_family_append(&fams[FAM_DISK_IO_TIME],
                                     VALUE_COUNTER_FLOAT64((double)io_time/1000.0), NULL,
                                     &device_label, NULL);

                metric_family_append(&fams[FAM_DISK_IO_WEIGHTED_TIME],
                                     VALUE_COUNTER_FLOAT64((double)weighted_time/1000.0), NULL,
                                     &device_label, NULL);
            }

            if (ds->has_discard) {
                metric_family_append(&fams[FAM_DISK_DISCARD_BYTES],
                                     VALUE_COUNTER(ds->discard_bytes), NULL, &device_label, NULL);

                metric_family_append(&fams[FAM_DISK_DISCARD_MERGED],
                                     VALUE_COUNTER(discard_merged), NULL, &device_label, NULL);

                metric_family_append(&fams[FAM_DISK_DISCARD_OPS],
                                     VALUE_COUNTER(discard_ops), NULL, &device_label, NULL);

                metric_family_append(&fams[FAM_DISK_DISCARD_TIME],
                                     VALUE_COUNTER_FLOAT64((double)discard_time/1000.0),
                                     NULL, &device_label, NULL);

                metric_family_append(&fams[FAM_DISK_DISCARD_WEIGHTED_TIME],
                                     VALUE_COUNTER_FLOAT64(ds->avg_discard_time),
                                     NULL, &device_label, NULL);
            }

            if (ds->has_flush) {
                metric_family_append(&fams[FAM_DISK_FLUSH_OPS],
                                     VALUE_COUNTER(flush_ops), NULL, &device_label, NULL);

                metric_family_append(&fams[FAM_DISK_FLUSH_TIME],
                                     VALUE_COUNTER_FLOAT64((double)flush_time/1000.0),
                                     NULL, &device_label, NULL);

                metric_family_append(&fams[FAM_DISK_FLUSH_WEIGHTED_TIME],
                                     VALUE_COUNTER_FLOAT64(ds->avg_flush_time),
                                     NULL, &device_label, NULL);
            }
        }

#ifdef HAVE_LIBUDEV_H
        /* release udev-based alternate name, if allocated */
        free(alt_name);
#endif
    } /* while (fgets (buffer, sizeof (buffer), fh) != NULL) */

    /* Remove disks that have disappeared from diskstats */
    for (ds = disklist, pre_ds = disklist; ds != NULL;) {
        /* Disk exists */
        if (ds->poll_count == poll_count) {
            pre_ds = ds;
            ds = ds->next;
            continue;
        }

        /* Disk is missing, remove it */
        diskstats_t *missing_ds = ds;
        if (ds == disklist) {
            pre_ds = disklist = ds->next;
        } else {
            pre_ds->next = ds->next;
        }
        ds = ds->next;

        PLUGIN_DEBUG("Disk %s disappeared.", missing_ds->name);
        free(missing_ds->name);
        free(missing_ds);
    }
    fclose(fh);

    plugin_dispatch_metric_family_array(fams, FAM_DISK_MAX, 0);
    return 0;
}

int disk_init(void)
{
    path_proc_diskstats = plugin_procpath("diskstats");
    if (path_proc_diskstats == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

#ifdef HAVE_LIBUDEV_H
    if (conf_udev_name_attr != NULL) {
        handle_udev = udev_new();
        if (handle_udev == NULL) {
            PLUGIN_ERROR("udev_new() failed!");
            return -1;
        }
    }
#endif
    return 0;
}

int disk_shutdown(void)
{
    free(path_proc_diskstats);

#ifdef HAVE_LIBUDEV_H
    if (handle_udev != NULL)
        udev_unref(handle_udev);
#endif

    diskstats_t *ds = disklist;
    while (ds != NULL) {
        diskstats_t *next = ds->next;
        free(ds->name);
        free(ds);
        ds = next;
    }
    disklist = NULL;

    exclist_reset(&excl_disk);

    return 0;
}

