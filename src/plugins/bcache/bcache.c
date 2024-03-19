// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com

#include "plugin.h"
#include "libutils/common.h"

enum  {
    FAM_BCACHE_AVERAGE_KEY_SIZE_SECTORS,
    FAM_BCACHE_BTREE_CACHE_SIZE_BYTES,
    FAM_BCACHE_CACHE_AVAILABLE_PERCENT,
    FAM_BCACHE_CONGESTED,
    FAM_BCACHE_ROOT_USAGE_PERCENT,
    FAM_BCACHE_TREE_DEPTH,
    FAM_BCACHE_ACTIVE_JOURNAL_ENTRIES,
    FAM_BCACHE_BTREE_NODES,
    FAM_BCACHE_BTREE_READ_AVERAGE_DURATION_SECONDS,
    FAM_BCACHE_CACHE_READ_RACES,
    FAM_BCACHE_DIRTY_DATA_BYTES,
    FAM_BCACHE_DIRTY_TARGET_BYTES,
    FAM_BCACHE_WRITEBACK_RATE,
    FAM_BCACHE_WRITEBACK_RATE_PROPORTIONAL_TERM,
    FAM_BCACHE_WRITEBACK_RATE_INTEGRAL_TERM,
    FAM_BCACHE_WRITEBACK_CHANGE,
    FAM_BCACHE_BYPASSED_BYTES,
    FAM_BCACHE_CACHE_HITS,
    FAM_BCACHE_CACHE_MISSES,
    FAM_BCACHE_CACHE_BYPASS_HITS,
    FAM_BCACHE_CACHE_BYPASS_MISSES,
    FAM_BCACHE_CACHE_MISS_COLLISIONS,
    FAM_BCACHE_CACHE_READAHEADS,
    FAM_BCACHE_IO_ERRORS,
    FAM_BCACHE_METADATA_WRITTEN_BYTES,
    FAM_BCACHE_WRITTEN_BYTES,
    FAM_BCACHE_MAX,
};

static metric_family_t fams[FAM_BCACHE_MAX] = {
    [FAM_BCACHE_AVERAGE_KEY_SIZE_SECTORS] = {
        .name = "system_bcache_average_key_size_sectors",
        .type = METRIC_TYPE_GAUGE,
        .help = "Average data per key in the btree in sectors.",
    },
    [FAM_BCACHE_BTREE_CACHE_SIZE_BYTES] = {
        .name = "system_bcache_btree_cache_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory currently used by the btree cache.",
    },
    [FAM_BCACHE_CACHE_AVAILABLE_PERCENT] = {
        .name = "system_bcache_cache_available_percent",
        .type = METRIC_TYPE_GAUGE,
        .help = "Percentage of cache device without dirty data, "
                "usable for writeback (may contain clean cached data).",
    },
    [FAM_BCACHE_CONGESTED] = {
        .name = "system_bcache_congested",
        .type = METRIC_TYPE_GAUGE,
        .help = "Congestion.",
    },
    [FAM_BCACHE_ROOT_USAGE_PERCENT] = {
        .name = "system_bcache_root_usage_percent",
        .type = METRIC_TYPE_GAUGE,
        .help = "Percentage of the root btree node in use. "
                "If this gets too high the node will split, increasing the tree depth."
    },
    [FAM_BCACHE_TREE_DEPTH] = {
        .name = "system_bcache_tree_depth",
        .type = METRIC_TYPE_GAUGE,
        .help = "Depth of the btree (A single node btree has depth 0)."
    },
    [FAM_BCACHE_ACTIVE_JOURNAL_ENTRIES] = {
        .name = "system_bcache_active_journal_entries",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of journal entries that are newer than the index.",
    },
    [FAM_BCACHE_BTREE_NODES] = {
        .name = "system_bcache_btree_nodes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total nodes in the btree.",
    },
    [FAM_BCACHE_BTREE_READ_AVERAGE_DURATION_SECONDS] = {
        .name = "system_bcache_btree_read_average_duration_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Average btree read duration.",
    },
    [FAM_BCACHE_CACHE_READ_RACES] = {
        .name = "system_bcache_cache_read_races",
        .type = METRIC_TYPE_COUNTER,
        .help = "counts instances where while data was being read from the cache, "
                "the bucket was reused and invalidated - "
                "i.e. where the pointer was stale after the read completed."
    },
    [FAM_BCACHE_DIRTY_DATA_BYTES] = {
        .name = "system_bcache_dirty_data_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of dirty data for this backing device in the cache.",
    },
    [FAM_BCACHE_DIRTY_TARGET_BYTES] = {
        .name = "system_bcache_dirty_target_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current dirty data target threshold for this backing device in bytes.",
    },
    [FAM_BCACHE_WRITEBACK_RATE] = {
        .name = "system_bcache_writeback_rate",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current writeback rate for this backing device in bytes.",
    },
    [FAM_BCACHE_WRITEBACK_RATE_PROPORTIONAL_TERM] = {
        .name = "system_bcache_writeback_rate_proportional_term",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current result of proportional controller, part of writeback rate",
    },
    [FAM_BCACHE_WRITEBACK_RATE_INTEGRAL_TERM] = {
        .name = "system_bcache_writeback_rate_integral_term",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current result of integral controller, part of writeback rate",
    },
    [FAM_BCACHE_WRITEBACK_CHANGE] = {
        .name = "system_bcache_writeback_change",
        .type = METRIC_TYPE_GAUGE,
        .help = "Last writeback rate change step for this backing device.",
    },
    [FAM_BCACHE_BYPASSED_BYTES] = {
        .name = "system_bcache_bypassed_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of IO (both reads and writes) that has bypassed the cache.",
    },
    [FAM_BCACHE_CACHE_HITS] = {
        .name = "system_bcache_cache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Hits counted per individual IO as bcache sees them.",
    },
    [FAM_BCACHE_CACHE_MISSES] = {
        .name = "system_bcache_cache_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Misses counted per individual IO as bcache sees them.",
    },
    [FAM_BCACHE_CACHE_BYPASS_HITS] = {
        .name = "system_bcache_cache_bypass_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Hits for IO intended to skip the cache.",
    },
    [FAM_BCACHE_CACHE_BYPASS_MISSES] = {
        .name = "system_bcache_cache_bypass_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Misses for IO intended to skip the cache.",
    },
    [FAM_BCACHE_CACHE_MISS_COLLISIONS] = {
        .name = "system_bcache_cache_miss_collisions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Instances where data insertion from cache miss raced with write "
                "(data already present).",
    },
    [FAM_BCACHE_CACHE_READAHEADS] = {
        .name = "system_bcache_cache_readaheads",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of times readahead occurred.",
    },
    [FAM_BCACHE_IO_ERRORS] = {
        .name = "system_bcache_io_errors",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of errors that have occurred, decayed by io_error_halflife.",
    },
    [FAM_BCACHE_METADATA_WRITTEN_BYTES] = {
        .name = "system_bcache_metadata_written_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Sum of all non data writes (btree writes and all other metadata).",
    },
    [FAM_BCACHE_WRITTEN_BYTES] = {
        .name = "system_bcache_written_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Sum of all data that has been written to the cache.",
    }
};

static char *path_sys_bcache;

typedef struct {
    char *file;
    double scale;
    unsigned int fam;
} bcache_file_t;

static bcache_file_t bcache_files[] = {
    { "average_key_size",                        1.0,   FAM_BCACHE_AVERAGE_KEY_SIZE_SECTORS            },
    { "btree_cache_size",                        1.0,   FAM_BCACHE_BTREE_CACHE_SIZE_BYTES              },
    { "cache_available_percent",                 1.0,   FAM_BCACHE_CACHE_AVAILABLE_PERCENT             },
    { "congested",                               1.0,   FAM_BCACHE_CONGESTED                           },
    { "root_usage_percent",                      1.0,   FAM_BCACHE_ROOT_USAGE_PERCENT                  },
    { "tree_depth",                              1.0,   FAM_BCACHE_TREE_DEPTH                          },
    { "internal/active_journal_entries",         1.0,   FAM_BCACHE_ACTIVE_JOURNAL_ENTRIES              },
    { "internal/btree_nodes",                    1.0,   FAM_BCACHE_BTREE_NODES                         },
    { "internal/btree_read_average_duration_us", 1e-6, FAM_BCACHE_BTREE_READ_AVERAGE_DURATION_SECONDS },
    { "internal/cache_read_races",               1.0,   FAM_BCACHE_CACHE_READ_RACES                    },
};
static size_t bcache_files_size = STATIC_ARRAY_SIZE(bcache_files);

static bcache_file_t bcache_backing_files[] = {
    { "dirty_data",                        1.0, FAM_BCACHE_DIRTY_DATA_BYTES      },
    { "stats_total/bypassed",              1.0, FAM_BCACHE_BYPASSED_BYTES        },
    { "stats_total/cache_hits",            1.0, FAM_BCACHE_CACHE_HITS            },
    { "stats_total/cache_misses",          1.0, FAM_BCACHE_CACHE_MISSES          },
    { "stats_total/cache_bypass_hits",     1.0, FAM_BCACHE_CACHE_BYPASS_HITS     },
    { "stats_total/cache_bypass_misses",   1.0, FAM_BCACHE_CACHE_BYPASS_MISSES   },
    { "stats_total/cache_miss_collisions", 1.0, FAM_BCACHE_CACHE_MISS_COLLISIONS },
    { "stats_total/cache_readaheads",      1.0, FAM_BCACHE_CACHE_READAHEADS      },
};
static size_t bcache_backing_files_size = STATIC_ARRAY_SIZE(bcache_backing_files);


static bcache_file_t bcache_cache_files[] = {
    { "io_errors",        1.0, FAM_BCACHE_IO_ERRORS              },
    { "written",          1.0, FAM_BCACHE_METADATA_WRITTEN_BYTES },
    { "metadata_written", 1.0, FAM_BCACHE_WRITTEN_BYTES          },
};
static size_t bcache_cache_files_size = STATIC_ARRAY_SIZE(bcache_cache_files);

static double bache_strtovalue(char *string, metric_type_t type, double vscale, value_t *value)
{
    double scale = 1.0;

    size_t len = strlen(string);
    if (len > 0) {
        switch(string[len-1]) {
        case 'k':
            scale = 1e3;
            string[len-1] = '\0';
            break;
        case 'M':
            scale = 1e6;
            string[len-1] = '\0';
            break;
        case 'G':
            scale = 1e9;
            string[len-1] = '\0';
            break;
        case 'T':
            scale = 1e12;
            string[len-1] = '\0';
            break;
        case 'P':
            scale = 1e15;
            string[len-1] = '\0';
            break;
        case 'E':
            scale = 1e18;
            string[len-1] = '\0';
            break;
        case 'Z':
            scale = 1e21;
            string[len-1] = '\0';
            break;
        case 'Y':
            scale = 1e22;
            string[len-1] = '\0';
            break;
        }
    }

    double num;
    if (strtodouble(string, &num) != 0) {
        PLUGIN_ERROR("Cannot parse \"%s\".", string);
        return -1;
    }
    num *= scale;


    if (type == METRIC_TYPE_GAUGE) {
        if (vscale != 1.0)
            num *= vscale;
        *value = VALUE_GAUGE(num);
    } else if (type == METRIC_TYPE_COUNTER) {
        *value = VALUE_COUNTER((uint64_t)num);
    }


    return 0;
}

static int bcache_read_file(int dir_fd, bcache_file_t *bf, label_set_t *labels)
{
    char buf[256];
    ssize_t size = read_file_at(dir_fd, bf->file, buf, sizeof(buf));
    if (size < 0)
        return size;
    strntrim(buf, size);

    value_t value = {0};
    metric_family_t *fam = &fams[bf->fam];
    int status = bache_strtovalue(buf, fam->type, bf->scale, &value);
    if (status == 0)
        metric_family_append(fam, value, labels, NULL);

    return 0;
}

static int bcache_read_writeback_rate_debug(int dir_fd, label_set_t *labels)
{

    FILE *fh = fopenat(dir_fd, "writeback_rate_debug", "r");
    if (fh == NULL) {
        PLUGIN_ERROR("fopen (\"writeback_rate_debug\") failed: %s.", STRERRNO);
        return -1;
    }

    char buf[256];
    while (fgets(buf, sizeof(buf), fh) != NULL) {
        char *fields[2];
        int numfields = strsplit(buf, fields, STATIC_ARRAY_SIZE(fields));

        if (numfields != 2)
            continue;

        metric_family_t *fam = NULL;
        value_t value = {0};

        if (strcmp("target:", fields[0]) == 0) {
            int status = bache_strtovalue(fields[1], METRIC_TYPE_GAUGE, 1.0, &value);
            if (status == 0)
                fam = &fams[FAM_BCACHE_DIRTY_TARGET_BYTES];
        } else if (strcmp("rate:", fields[0]) == 0) {
            size_t len = strlen(fields[1]);
            if (len > strlen("/sec")) {
                if (strcmp(fields[1] + len - strlen("/sec"), "/sec") == 0)
                    fields[1][len - strlen("/sec")] = '\0';
            }
            int status = bache_strtovalue(fields[1], METRIC_TYPE_GAUGE, 1.0, &value);
            if (status == 0)
                fam = &fams[FAM_BCACHE_WRITEBACK_RATE];
        } else if (strcmp("proportional:", fields[0]) == 0) {
            int status = bache_strtovalue(fields[1], METRIC_TYPE_GAUGE, 1.0, &value);
            if (status == 0)
                fam = &fams[FAM_BCACHE_WRITEBACK_RATE_PROPORTIONAL_TERM];
        } else if (strcmp("integral:", fields[0]) == 0) {
            int status = bache_strtovalue(fields[1], METRIC_TYPE_GAUGE, 1.0, &value);
            if (status == 0)
                fam = &fams[FAM_BCACHE_WRITEBACK_RATE_INTEGRAL_TERM];
        } else if (strcmp("change:", fields[0]) == 0) {
            size_t len = strlen(fields[1]);
            if (len > strlen("/sec")) {
                if (strcmp(fields[1] + len - strlen("/sec"), "/sec") == 0)
                    fields[1][len - strlen("/sec")] = '\0';
            }
            int status = bache_strtovalue(fields[1], METRIC_TYPE_GAUGE, 1.0, &value);
            if (status == 0)
                fam = &fams[FAM_BCACHE_WRITEBACK_CHANGE];
        }
        if (fam != NULL)
            metric_family_append(fam, value, labels, NULL);
    }

    fclose(fh);

    return 0;
}

static int bache_read_device(int dirfd, const char *path, const char *filename, void *ud)
{
    label_set_t *labels = ud;

    if (strncmp("bdev", filename, strlen("bdev")) == 0) {
        if (!isdigit(filename[strlen("bdev")]))
            return 0;

        int dir_bdev_fd = openat(dirfd, filename, O_RDONLY | O_DIRECTORY);
        if (dir_bdev_fd < 0) {
            PLUGIN_ERROR("open (%s) in %s failed: %s.", filename, path, STRERRNO);
            return -1;
        }
        label_set_add(labels, true, "backing_device", filename);
        for (size_t i = 0; i < bcache_backing_files_size; i++) {
            bcache_read_file(dir_bdev_fd, &bcache_backing_files[i], labels);
        }
        bcache_read_writeback_rate_debug(dir_bdev_fd, labels);
        label_set_add(labels, true, "backing_device", NULL);
        close(dir_bdev_fd);
    } else if (strncmp("cache", filename, strlen("cache")) == 0) {
        if (!isdigit(filename[strlen("cache")]))
            return 0;

        int dir_cache_fd = openat(dirfd, filename, O_RDONLY | O_DIRECTORY);
        if (dir_cache_fd < 0) {
            PLUGIN_ERROR("open (%s) in %s failed: %s.", filename, path, STRERRNO);
            return -1;
        }
        label_set_add(labels, true, "cache_device", filename);
        for (size_t i = 0; i < bcache_cache_files_size; i++) {
            bcache_read_file(dir_cache_fd, &bcache_cache_files[i], labels);
        }
        label_set_add(labels, true, "cache_device", NULL);
        close(dir_cache_fd);
    }

    return 0;
}

static int bache_read_devices(int dir_fd, const char *path, const char *filename, void *ud)
{
    label_set_t *labels = ud;

    struct stat statbuf;
    int status = fstatat(dir_fd, filename, &statbuf, 0);
    if (status != 0) {
        PLUGIN_ERROR("stat (%s) in %s failed: %s.", filename, path, STRERRNO);
        return -1;
    }

    if (S_ISDIR(statbuf.st_mode)) {
        label_set_add(labels, true, "uuid", filename);

        int dir_device_fd = openat(dir_fd, filename, O_RDONLY | O_DIRECTORY);
        if (dir_device_fd < 0) {
            PLUGIN_ERROR("open (%s) in %s failed: %s.", filename, path, STRERRNO);
            return -1;
        }

        for (size_t i = 0; i < bcache_files_size ; i++) {
            bcache_read_file(dir_device_fd, &bcache_files[i], labels);
        }

        walk_directory_at(dir_fd, filename, bache_read_device, labels, 0);

        close(dir_device_fd);
    }

    return 0;
}

static int bcache_read(void)
{
    label_set_t labels = {0};
    int status = walk_directory(path_sys_bcache, bache_read_devices, &labels, 0);
    label_set_reset(&labels);

    plugin_dispatch_metric_family_array(fams, FAM_BCACHE_MAX, 0);

    if (status != 0)
        return -1;

    return 0;
}

static int bcache_init(void)
{
    path_sys_bcache = plugin_syspath("fs/bcache/");
    if (path_sys_bcache == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    return 0;
}

static int bcache_shutdown(void)
{
    free(path_sys_bcache);
    return 0;
}

void module_register(void)
{
    plugin_register_init("bcache", bcache_init);
    plugin_register_read("bcache", bcache_read);
    plugin_register_shutdown("bcache", bcache_shutdown);
}
