// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

static char *path_sys_block;

enum {
    FAM_ZRAM_READ_IOS,
    FAM_ZRAM_READ_MERGES,
    FAM_ZRAM_READ_SECTORS,
    FAM_ZRAM_READ_SECONDS,
    FAM_ZRAM_WRITE_IOS,
    FAM_ZRAM_WRITE_MERGES,
    FAM_ZRAM_WRITE_SECTORS,
    FAM_ZRAM_WRITE_SECONDS,
    FAM_ZRAM_IN_FLIGHT,
    FAM_ZRAM_IO_SECONDS,
    FAM_ZRAM_TIME_IN_QUEUE_SECONDS,
    FAM_ZRAM_DISCARD_IOS,
    FAM_ZRAM_DISCARD_MERGES,
    FAM_ZRAM_DISCARD_SECTORS,
    FAM_ZRAM_DISCARD_SECONDS,
    FAM_ZRAM_FAILED_READS,
    FAM_ZRAM_FAILED_WRITES,
    FAM_ZRAM_INVALID_IO,
    FAM_ZRAM_NOTIFY_FREE,
    FAM_ZRAM_UNCOMPRESSED_BYTES,
    FAM_ZRAM_COMPRESSED_BYTES,
    FAM_ZRAM_MEMORY_USED_BYTES,
    FAM_ZRAM_MEMORY_LIMIT_BYTES,
    FAM_ZRAM_MEMORY_USED_MAX_BYTES,
    FAM_ZRAM_SAME_PAGES,
    FAM_ZRAM_COMPACTED_PAGES,
    FAM_ZRAM_HUGE_PAGES,
    FAM_ZRAM_BACKING_BYTES,
    FAM_ZRAM_BACKING_READS_BYTES,
    FAM_ZRAM_BACKING_WRITE_BYTES,
    FAM_ZRAM_MAX,
};

static metric_family_t fams[FAM_ZRAM_MAX] = {
    [FAM_ZRAM_READ_IOS] = {
        .name = "system_zram_read_ios",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of read I/Os processed.",
    },
    [FAM_ZRAM_READ_MERGES] = {
        .name = "system_zram_read_merges",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of read I/Os merged with in-queue I/O.",
    },
    [FAM_ZRAM_READ_SECTORS] = {
        .name = "system_zram_read_sectors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of sectors read.",
    },
    [FAM_ZRAM_READ_SECONDS] = {
        .name = "system_zram_read_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total wait time for read requests.",
    },
    [FAM_ZRAM_WRITE_IOS] = {
        .name = "system_zram_write_ios",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of write I/Os processed.",
    },
    [FAM_ZRAM_WRITE_MERGES] = {
        .name = "system_zram_write_merges",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of write I/Os merged with in-queue I/O.",
    },
    [FAM_ZRAM_WRITE_SECTORS] = {
        .name = "system_zram_write_sectors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of sectors written.",
    },
    [FAM_ZRAM_WRITE_SECONDS] = {
        .name = "system_zram_write_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total wait time for write requests.",
    },
    [FAM_ZRAM_IN_FLIGHT] = {
        .name = "system_zram_in_flight",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of I/Os currently in flight.",
    },
    [FAM_ZRAM_IO_SECONDS] = {
        .name = "system_zram_io_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total time this block device has been active.",
    },
    [FAM_ZRAM_TIME_IN_QUEUE_SECONDS] = {
        .name = "system_zram_time_in_queue_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total wait time for all requests.",
    },
    [FAM_ZRAM_DISCARD_IOS] = {
        .name = "system_zram_discard_ios",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of discard I/Os processed.",
    },
    [FAM_ZRAM_DISCARD_MERGES] = {
        .name = "system_zram_discard_merges",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of discard I/Os merged with in-queue I/O.",
    },
    [FAM_ZRAM_DISCARD_SECTORS] = {
        .name = "system_zram_discard_sectors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of sectors discarded",
    },
    [FAM_ZRAM_DISCARD_SECONDS] = {
        .name = "system_zram_discard_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total wait time for discard requests.",
    },
    [FAM_ZRAM_FAILED_READS] = {
        .name = "system_zram_failed_reads",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of failed reads.",
    },
    [FAM_ZRAM_FAILED_WRITES] = {
        .name = "system_zram_failed_writes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of failed writes.",
    },
    [FAM_ZRAM_INVALID_IO] = {
        .name = "system_zram_invalid_io",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of non-page-size-aligned I/O requests",
    },
    [FAM_ZRAM_NOTIFY_FREE] = {
        .name = "system_zram_notify_free",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of pages freed because of swap slot free notifications "
                "or because of REQ_OP_DISCARD requests sent by bio.",
    },
    [FAM_ZRAM_UNCOMPRESSED_BYTES] = {
        .name = "system_zram_uncompressed_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Uncompressed size of data stored in this disk. "
                "This excludes same-element-filled pages (same_pages) "
                "since no memory is allocated for them.",
    },
    [FAM_ZRAM_COMPRESSED_BYTES] = {
        .name = "system_zram_compressed_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Compressed size of data stored in this disk.",
    },
    [FAM_ZRAM_MEMORY_USED_BYTES] = {
        .name = "system_zram_memory_used_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The amount of memory allocated for this disk. "
                "This includes allocator fragmentation and metadata overhead, "
                "allocated for this disk.",
    },
    [FAM_ZRAM_MEMORY_LIMIT_BYTES] = {
        .name = "system_zram_memory_limit_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The maximum amount of memory ZRAM can use to store the compressed data.",
    },
    [FAM_ZRAM_MEMORY_USED_MAX_BYTES] = {
        .name = "system_zram_memory_used_max_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The maximum amount of memory zram have consumed to store the data.",
    },
    [FAM_ZRAM_SAME_PAGES] = {
        .name = "system_zram_same_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of same element filled pages written to this disk. "
                "No memory is allocated for such pages.",
    },
    [FAM_ZRAM_COMPACTED_PAGES] = {
        .name = "system_zram_compacted_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of pages freed during compaction.",
    },
    [FAM_ZRAM_HUGE_PAGES] = {
        .name = "system_zram_huge_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of incompressible pages.",
    },
    [FAM_ZRAM_BACKING_BYTES] = {
        .name = "system_zram_backing_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Size of data written in backing device.",
    },
    [FAM_ZRAM_BACKING_READS_BYTES] = {
        .name = "system_zram_backing_reads_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of reads from backing device.",
    },
    [FAM_ZRAM_BACKING_WRITE_BYTES] = {
        .name = "system_zram_backing_write_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of writes to backing device.",
    },
};

static exclist_t excl_device;

static int zram_read_device_stat(int dirfd, const char *device)
{
    char path[PATH_MAX];
    int size = snprintf(path, sizeof(path), "%s/stat", device);
    if (unlikely(size < 0))
        return -1;

    char buffer[512];
    ssize_t len = read_file_at(dirfd, path, buffer, sizeof(buffer));
    if (unlikely(len < 0))
        return -1;
    char *line = strntrim(buffer, len);

    char *fields[15];
    int fields_num = strsplit(line, fields, STATIC_ARRAY_SIZE(fields));

    if (unlikely(fields_num < 15))
        return -1;

    metric_family_append(&fams[FAM_ZRAM_READ_IOS], VALUE_COUNTER(atoll(fields[0])), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_READ_MERGES], VALUE_COUNTER(atoll(fields[1])), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_READ_SECTORS], VALUE_COUNTER(atoll(fields[2])), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_READ_SECONDS],
                         VALUE_COUNTER_FLOAT64((double)atoll(fields[3])/1000000L), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_WRITE_IOS], VALUE_COUNTER(atoll(fields[4])), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_WRITE_MERGES], VALUE_COUNTER(atoll(fields[5])), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_WRITE_SECTORS], VALUE_COUNTER(atoll(fields[6])), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_WRITE_SECONDS],
                         VALUE_COUNTER_FLOAT64((double)atoll(fields[7])/1000000L), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_IN_FLIGHT], VALUE_COUNTER(atoll(fields[8])), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_IO_SECONDS], VALUE_COUNTER(atoll(fields[9])), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_TIME_IN_QUEUE_SECONDS],
                         VALUE_COUNTER_FLOAT64((double)atoll(fields[10])/1000000L), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_DISCARD_IOS], VALUE_COUNTER(atoll(fields[11])), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_DISCARD_MERGES], VALUE_COUNTER(atoll(fields[12])), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_DISCARD_SECTORS], VALUE_COUNTER(atoll(fields[13])), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_DISCARD_SECONDS],
                         VALUE_COUNTER_FLOAT64((double)atoll(fields[14])/1000000L), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);

    return 0;
}

static int zram_read_device_io_stat(int dirfd, const char *device)
{
    char path[PATH_MAX];
    int size = snprintf(path, sizeof(path), "%s/io_stat", device);
    if (unlikely(size < 0))
        return -1;

    char buffer[512];
    ssize_t len = read_file_at(dirfd, path, buffer, sizeof(buffer));
    if (unlikely(len < 0))
        return -1;
    char *line = strntrim(buffer, len);

    char *fields[4];
    int fields_num = strsplit(line, fields, STATIC_ARRAY_SIZE(fields));

    if (unlikely(fields_num < 4))
        return -1;

    metric_family_append(&fams[FAM_ZRAM_FAILED_READS], VALUE_COUNTER(atoll(fields[0])), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_FAILED_WRITES], VALUE_COUNTER(atoll(fields[1])), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_INVALID_IO], VALUE_COUNTER(atoll(fields[2])), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_NOTIFY_FREE], VALUE_COUNTER(atoll(fields[3])), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);

    return 0;
}

static int zram_read_device_mm_stat(int dirfd, const char *device)
{
    char path[PATH_MAX];
    int size = snprintf(path, sizeof(path), "%s/mm_stat", device);
    if (unlikely(size < 0))
        return -1;

    char buffer[512];
    ssize_t len = read_file_at(dirfd, path, buffer, sizeof(buffer));
    if (unlikely(len < 0))
        return -1;

    char *line = strntrim(buffer, len);

    char *fields[8];
    int fields_num = strsplit(line, fields, STATIC_ARRAY_SIZE(fields));

    if (unlikely(fields_num < 8))
        return -1;

    metric_family_append(&fams[FAM_ZRAM_UNCOMPRESSED_BYTES], VALUE_GAUGE(atof(fields[0])), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_COMPRESSED_BYTES], VALUE_GAUGE(atof(fields[1])), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_MEMORY_USED_BYTES], VALUE_GAUGE(atof(fields[2])), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_MEMORY_LIMIT_BYTES], VALUE_GAUGE(atof(fields[3])), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_MEMORY_USED_MAX_BYTES], VALUE_GAUGE(atof(fields[4])), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_SAME_PAGES], VALUE_GAUGE(atof(fields[5])), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_COMPACTED_PAGES], VALUE_GAUGE(atof(fields[6])), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_HUGE_PAGES], VALUE_GAUGE(atof(fields[7])), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);

    return 0;
}

static int zram_read_device_bd_stat(int dirfd, const char *device)
{
    char path[PATH_MAX];
    int size = snprintf(path, sizeof(path), "%s/bd_stat", device);
    if (unlikely(size < 0))
        return -1;

    char buffer[512];
    ssize_t len = read_file_at(dirfd, path, buffer, sizeof(buffer));
    if (unlikely(len < 0))
        return -1;

    char *line = strntrim(buffer, len);

    char *fields[4];
    int fields_num = strsplit(line, fields, STATIC_ARRAY_SIZE(fields));

    if (unlikely(fields_num < 4))
        return -1;

    metric_family_append(&fams[FAM_ZRAM_BACKING_BYTES],
                         VALUE_COUNTER(atoll(fields[3])*4096), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_BACKING_READS_BYTES],
                         VALUE_COUNTER(atoll(fields[3])*4096), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);
    metric_family_append(&fams[FAM_ZRAM_BACKING_WRITE_BYTES],
                         VALUE_COUNTER(atoll(fields[3])*4096), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);

    return 0;
}

static int zram_read_device(int dirfd, __attribute__((unused)) const char *path,
                            const char *entry, __attribute__((unused)) void *ud)
{
    if (!exclist_match(&excl_device, entry))
        return 0;

    if (strncmp("zram", entry, strlen("zram")) != 0)
        return 0;

    zram_read_device_stat(dirfd, entry);
    zram_read_device_io_stat(dirfd, entry);
    zram_read_device_mm_stat(dirfd, entry);
    zram_read_device_bd_stat(dirfd, entry);
    return 0;
}

static int zram_read(void)
{
    walk_directory(path_sys_block, zram_read_device, NULL, 0);
    plugin_dispatch_metric_family_array(fams, FAM_ZRAM_MAX, 0);
    return 0;
}

static int zram_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "device") == 0) {
            status = cf_util_exclist(child, &excl_device);
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

static int zram_init(void)
{
    path_sys_block = plugin_syspath("block");
    if (path_sys_block == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    return 0;
}

static int zram_shutdown(void)
{
    free(path_sys_block);
    exclist_reset(&excl_device);
    return 0;
}

void module_register(void)
{
    plugin_register_init("zram", zram_init);
    plugin_register_config("zram", zram_config);
    plugin_register_read("zram", zram_read);
    plugin_register_shutdown("zram", zram_shutdown);
}
