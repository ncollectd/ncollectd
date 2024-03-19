// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2012 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2009-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#include "disk.h"

metric_family_t fams[FAM_DISK_MAX] = {
    [FAM_DISK_READ_BYTES] = {
        .name = "system_disk_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of bytes read successfully.",
    },
    [FAM_DISK_READ_MERGED] = {
        .name = "system_disk_read_merged",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of reads merged.",
    },
    [FAM_DISK_READ_OPS] = {
        .name = "system_disk_read_ops",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of reads completed successfully.",
    },
    [FAM_DISK_READ_TIME] = {
        .name = "system_disk_read_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total time, in seconds, spent reading.",
    },
    [FAM_DISK_READ_WEIGHTED_TIME] = {
        .name = "system_disk_read_weighted_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The average time, in seconds, for read requests "
                "issued to the device to be served.",
    },
    [FAM_DISK_READ_TIMEOUT] = {
        .name = "system_disk_read_timeout",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of read request timeout.",
    },
    [FAM_DISK_READ_FAILED] = {
        .name = "system_disk_read_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of failed read requests.",
    },
    [FAM_DISK_WRITE_BYTES] = {
        .name = "system_disk_write_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of bytes written successfully.",
    },
    [FAM_DISK_WRITE_MERGED] = {
        .name = "system_disk_write_merged",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of writes merged.",
    },
    [FAM_DISK_WRITE_OPS] = {
        .name = "system_disk_write_ops",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of writes completed successfully.",
    },
    [FAM_DISK_WRITE_TIME] = {
        .name = "system_disk_write_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total time, in seconds, spent writing.",
    },
    [FAM_DISK_WRITE_WEIGHTED_TIME] = {
        .name = "system_disk_write_weighted_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The average time, seconds, for write requests "
                "issued to the device to be served.",
    },
    [FAM_DISK_WRITE_TIMEOUT] = {
        .name = "system_disk_write_timeout",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of write request timeout.",
    },
    [FAM_DISK_WRITE_FAILED] = {
        .name = "system_disk_write_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of failed write requests.",
    },
    [FAM_DISK_IO_TIME] = {
        .name = "system_disk_io_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total time, in seconds, spent doing I/Os."
    },
    [FAM_DISK_IO_WEIGHTED_TIME] = {
        .name = "system_disk_io_weighted_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The average time, in seconds, spent doing I/Os."
    },
    [FAM_DISK_PENDING_OPERATIONS] = {
        .name = "system_disk_pending_operations",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of I/Os currently in progress.",
    },
    [FAM_DISK_DISCARD_BYTES] = {
        .name = "system_disk_discard_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of bytes discarded.",
    },
    [FAM_DISK_DISCARD_MERGED] = {
        .name = "system_disk_discard_merged",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of discard requests merged that were queued to the device.",
    },
    [FAM_DISK_DISCARD_OPS] = {
        .name = "system_disk_discard_operations",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of discard requests completed successfully.",
    },
    [FAM_DISK_DISCARD_TIME] = {
        .name = "system_disk_discard_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total time, in seconds, spent discarding.",
    },
    [FAM_DISK_DISCARD_WEIGHTED_TIME] = {
        .name = "system_disk_discard_weighted_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The average time, in seconds, for discard requests "
                "issued to the device to be served."
    },
    [FAM_DISK_FLUSH_OPS] = {
        .name = "system_disk_flush_operations",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of flush requests completed successfully.",
    },
    [FAM_DISK_FLUSH_TIME] = {
        .name = "system_disk_flush_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total time, in seconds, spent flushing.",
    },
    [FAM_DISK_FLUSH_WEIGHTED_TIME] = {
        .name = "system_disk_flush_weighted_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The  average  time, in seconds, for flush requests "
                "issued to the device to be served."
    },
};

exclist_t excl_disk = {0};
char *conf_udev_name_attr = NULL;
bool use_bsd_name;

static int disk_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "disk") == 0) {
            status = cf_util_exclist(child, &excl_disk);
        } else if (strcasecmp(child->key, "use-bsd-name") == 0) {
#ifdef HAVE_IOKIT_IOKITLIB_H
            status = cf_util_get_boolean(child, &use_bsd_name);
#else
            PLUGIN_WARNING("The 'use-bsd-name' option is only supported "
                           "on Mach / Mac OS X and will be ignored.");
#endif
        } else if (strcasecmp(child->key, "udev-name-attr") == 0) {
#ifdef HAVE_LIBUDEV_H
            status = cf_util_get_string(child, &conf_udev_name_attr);
#else
            PLUGIN_WARNING("The 'udev-name-attr' option is only supported "
                           "if ncollectd is built with libudev support");
#endif
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

int disk_read(void);

__attribute__(( weak ))
int disk_shutdown(void)
{
    return 0;
}

__attribute__(( weak ))
int disk_init(void)
{
    return 0;
}

void module_register(void)
{
    plugin_register_config("disk", disk_config);
    plugin_register_init("disk", disk_init);
    plugin_register_shutdown("disk", disk_shutdown);
    plugin_register_read("disk", disk_read);
}
