// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

// https://forums.oracle.com/ords/apexds/post/part-10-monitoring-and-tuning-zfs-performance-4977
// https://github.com/pdf/zfs_exporter/tree/master
// https://github.com/eliothedeman/zfs_exporter
// https://github.com/eripa/prometheus-zfs
// https://freebsdfoundation.org/wp-content/uploads/2017/12/Monitoring-ZFS.pdf

// https://docs.oracle.com/cd/E27998_01/html/E48490/analytics__statistics__cache_arc_accesses.html
// https://www.brendangregg.com/blog/2012-01-09/activity-of-the-zfs-arc.html

// https://utcc.utoronto.ca/~cks/space/blog/linux/ZFSOnLinuxARCMemoryStatistics


#define KSTAT_DATA_CHAR   0
#define KSTAT_DATA_INT32  1
#define KSTAT_DATA_UINT32 2
#define KSTAT_DATA_INT64  3
#define KSTAT_DATA_UINT64 4

#include "plugins/zfs/zfs_fam.h"
#include "plugins/zfs/zfs_flags.h"
#include "plugins/zfs/zfs_stats.h"

extern metric_family_t fams_zfs[FAM_ZFS_MAX];

static char *path_proc_zfs;
extern uint64_t zfs_flags;

#if 0
system_zfs_zpool_

"zpool" "xxx"

system_zfs_zpool_dataset
"zpool"   "xxx"
"dataset" "xxx"

system_zfs_zpool_state

"zpool" "xxx"
"state" "xxx"
#endif

#if 0

    [FAM_ZFS_ZPOOL_READS] = {
        .name = "system_zfs_zpool_reads",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of number of read operations.",
    },
    [FAM_ZFS_ZPOOL_WRITES] = {
        .name = "system_zfs_zpool_writes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of number of write operations.",
    },
    [FAM_ZFS_ZPOOL_READ_BYTES] = {
        .name = "system_zfs_zpool_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of bytes read.",
    },
    [FAM_ZFS_ZPOOL_WRITTEN_BYTES] = {
        .name = "system_zfs_zpool_written_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of bytes written.";
    },

    [FAM_ZFS_ZPOOL_WAIT_TIME_SECONDS] = {
        .name = "system_zfs_zpool_wait_time_seconds",
        .type = METRIC_TYPE_COUNTER,
    [FAM_ZFS_ZPOOL_RUN_TIME_SECONDS] = {
        .name = "system_zfs_zpool_run_time_seconds",
        .type = METRIC_TYPE_COUNTER,
    [FAM_ZFS_ZPOOL_WAIT_STATE] = {
        .name = "system_zfs_zpool_wait_state",
        .type = METRIC_TYPE_COUNTER,
    [FAM_ZFS_ZPOOL_RUN_STATE] = {
        .name = "system_zfs_zpool_run_state",
        .type = METRIC_TYPE_COUNTER,



              hrtime_t   wtime;            /* cumulative wait (pre-service) time */
              hrtime_t   rtime;            /* cumulative run (service) time */
              uint_t     wcnt;             /* count of elements in wait state */
              uint_t     rcnt;             /* count of elements in run state */

//nread    nwritten reads    writes   wtime    wlentime wupdate  rtime    rlentime rupdate  wcnt     rcnt

           /*
            * Basic counters.
            */
              u_longlong_t     nread;      /* number of bytes read */
              u_longlong_t     nwritten;   /* number of bytes written */
              uint_t           reads;      /* number of read operations */
              uint_t           writes;     /* number of write operations */
           /*
            * Accumulated time and queue length statistics.
            *
            * Time statistics are kept as a running sum of "active" time.
            * Queue length statistics are kept as a running sum of the
            * product of queue length and elapsed time at that length --
            * that is, a Riemann sum for queue length integrated against time.
            *
            *               ^
            *               |                       _________
            *               8                       | i4    |
            *               |                       |       |
            *       Queue   6                       |       |
            *       Length  |       _________       |       |
            *               4       | i2    |_______|       |
            *               |       |       i3              |
            *               2_______|                       |
            *               |    i1                         |
            *               |_______________________________|
            *               Time->  t1      t2      t3      t4
            *
            * At each change of state (entry or exit from the queue),
            * we add the elapsed time (since the previous state change)
            * to the active time if the queue length was non-zero during
            * that interval; and we add the product of the elapsed time
            * times the queue length to the running length*time sum.
            *
            * This method is generalizable to measuring residency
            * in any defined system: instead of queue lengths, think
            * of "outstanding RPC calls to server X".
            *
            * A large number of I/O subsystems have at least two basic
            * "lists" of transactions they manage: one for transactions
            * that have been accepted for processing but for which processing
            * has yet to begin, and one for transactions which are actively
            * being processed (but not done). For this reason, two cumulative
            * time statistics are defined here: pre-service (wait) time,
            * and service (run) time.
            *
            * The units of cumulative busy time are accumulated nanoseconds.
            * The units of cumulative length*time products are elapsed time
            * times queue length.
            */
              hrtime_t   wtime;            /* cumulative wait (pre-service) time */
              hrtime_t   wlentime;         /* cumulative wait length*time product*/
              hrtime_t   wlastupdate;      /* last time wait queue changed */
              hrtime_t   rtime;            /* cumulative run (service) time */
              hrtime_t   rlentime;         /* cumulative run length*time product */
              hrtime_t   rlastupdate;      /* last time run queue changed */
              uint_t     wcnt;             /* count of elements in wait state */
              uint_t     rcnt;             /* count of elements in run state */



#endif

static int zfs_append_metric(char *buffer, size_t prefix_len, size_t buffer_len,
                             char *name, char *dtype, char *sval)
{
    char data = dtype[0];
    if ((data < '1') || (data > '4'))
        return -1;

    sstrncpy(buffer + prefix_len, name, buffer_len - prefix_len);

    const struct zfs_stats *zfss  = zfs_stats_get_key (buffer, strlen(buffer));
    if (zfss == NULL)
        return -1;

    metric_family_t *fam = &fams_zfs[zfss->fam];
    value_t value = {0};

    uint64_t num = 0;
    int status = parse_uinteger(sval, &num);
    if (status != 0)
        return -1;

    if (fam->type == METRIC_TYPE_COUNTER) {
        value = VALUE_COUNTER(num);
    } else if (fam->type == METRIC_TYPE_GAUGE) {
        value = VALUE_GAUGE(num);
    } else {
        return -1;
    }

    metric_family_append(fam, value, NULL,
                         &(label_pair_const_t){.name=zfss->lkey, .value=zfss->lvalue}, NULL);

    return 0;
}


static int zfs_read_proc_pool_objset(int dir_fd, const char *pool, const char *filename)
{
    FILE *fh = fopenat(dir_fd, filename, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Cannot fopen '%s/%s': %s", pool, filename, STRERRNO);
        return -1;
    }

    char dataset_name[256] = {'\0'};

    struct {
        char *name;
        bool found;
        int fam;
        uint64_t value;
    } metrics[] = {
        { "writes",    false,  FAM_ZFS_ZPOOL_DATASET_WRITES,        0},
        { "nwritten",  false,  FAM_ZFS_ZPOOL_DATASET_WRITTEN_BYTES, 0},
        { "reads",     false,  FAM_ZFS_ZPOOL_DATASET_READS,         0},
        { "nread",     false,  FAM_ZFS_ZPOOL_DATASET_READ_BYTES,    0},
        { "nunlinks",  false,  FAM_ZFS_ZPOOL_DATASET_UNLINKS,       0},
        { "nunlinked", false,  FAM_ZFS_ZPOOL_DATASET_UNLINKED,      0},
        { NULL,        false,  -1,                                  0}
    };

    char buffer[256];

    int lineno = 0;
    char *fields[4];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        lineno++;
        if (lineno < 3)
            continue;
        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (fields_num != 3)
            continue;

        if (fields[1][0] == '7') {
            if (strcmp(fields[0], "dataset_name") == 0)
                sstrncpy(dataset_name, fields[0], sizeof(dataset_name));
        } else if (fields[1][0] == '4') {
            for (size_t i = 0; metrics[i].name != NULL; i++) {
                if (strcmp(fields[0], metrics[i].name) == 0) {
                    int status = parse_uinteger(fields[2], &metrics[i].value);
                    if (status != 0)
                        break;
                    metrics[i].found = true;
                    break;
                }
            }
        }
    }

    fclose(fh);

    if (dataset_name[0] == '\0')
        return -1;

    for (size_t i = 0; metrics[i].name != NULL; i++) {
        if (metrics[i].found) {
            metric_family_append(&fams_zfs[metrics[i].fam], VALUE_GAUGE(metrics[i].value), NULL,
                                 &(label_pair_const_t){.name="dataset", .value=dataset_name},
                                 &(label_pair_const_t){.name="pool", .value=pool}, NULL);
        }
    }

    return 0;
}

static int zfs_read_proc_pool_io(int dir_fd, const char *pool)
{
    FILE *fh = fopenat(dir_fd, "io", "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Cannot fopen '%s/io': %s", pool, STRERRNO);
        return -1;
    }

    char buffer[512];

    char *fields[16];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (fields_num != 12)
            break;

//nread    nwritten reads    writes   wtime    wlentime wupdate  rtime    rlentime rupdate  wcnt     rcnt
#if 0


              u_longlong_t     nread;      /* number of bytes read */
              u_longlong_t     nwritten;   /* number of bytes written */
              uint_t           reads;      /* number of read operations */
              uint_t           writes;     /* number of write operations */
           /*
            * Accumulated time and queue length statistics.
            *
            * Time statistics are kept as a running sum of "active" time.
            * Queue length statistics are kept as a running sum of the
            * product of queue length and elapsed time at that length --
            * that is, a Riemann sum for queue length integrated against time.
            *
            *               ^
            *               |                       _________
            *               8                       | i4    |
            *               |                       |       |
            *       Queue   6                       |       |
            *       Length  |       _________       |       |
            *               4       | i2    |_______|       |
            *               |       |       i3              |
            *               2_______|                       |
            *               |    i1                         |
            *               |_______________________________|
            *               Time->  t1      t2      t3      t4
            *
            * At each change of state (entry or exit from the queue),
            * we add the elapsed time (since the previous state change)
            * to the active time if the queue length was non-zero during
            * that interval; and we add the product of the elapsed time
            * times the queue length to the running length*time sum.
            *
            * This method is generalizable to measuring residency
            * in any defined system: instead of queue lengths, think
            * of "outstanding RPC calls to server X".
            *
            * A large number of I/O subsystems have at least two basic
            * "lists" of transactions they manage: one for transactions
            * that have been accepted for processing but for which processing
            * has yet to begin, and one for transactions which are actively
            * being processed (but not done). For this reason, two cumulative
            * time statistics are defined here: pre-service (wait) time,
            * and service (run) time.
            *
            * The units of cumulative busy time are accumulated nanoseconds.
            * The units of cumulative length*time products are elapsed time
            * times queue length.
            */
              hrtime_t   wtime;            /* cumulative wait (pre-service) time */
              hrtime_t   wlentime;         /* cumulative wait length*time product*/
              hrtime_t   wlastupdate;      /* last time wait queue changed */
              hrtime_t   rtime;            /* cumulative run (service) time */
              hrtime_t   rlentime;         /* cumulative run length*time product */
              hrtime_t   rlastupdate;      /* last time run queue changed */
              uint_t     wcnt;             /* count of elements in wait state */
              uint_t     rcnt;             /* count of elements in run state */
#endif
    }

    fclose(fh);

    return 0;
}

static int zfs_read_proc_pool_state(int dir_fd, const char *pool)
{
    char buffer[64];

    int len = read_file_at(dir_fd, "state", buffer, sizeof(buffer));
    if (len < 0)
        return -1;
    char *state = strntrim(buffer, len);

    state_t states[] = {
        { .name = "online",    .enabled = false },
        { .name = "degraded",  .enabled = false },
        { .name = "faulted",   .enabled = false },
        { .name = "offline",   .enabled = false },
        { .name = "removed",   .enabled = false },
        { .name = "unavail",   .enabled = false },
        { .name = "suspended", .enabled = false },
    };
    state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };

    for (size_t i = 0; i < STATIC_ARRAY_SIZE(states); i++) {
        if (strcasecmp(states[i].name, state) == 0) {
            states[i].enabled = true;
            break;
        }
    }

    metric_family_append(&fams_zfs[FAM_ZFS_ZPOOL_STATE], VALUE_STATE_SET(set), NULL,
                         &(label_pair_const_t){.name="pool", .value=pool}, NULL);

    return 0;
}

static int zfs_read_proc_file(const char *filename, const char *prefix)
{
    char path[PATH_MAX];
    ssnprintf(path, sizeof(path), "%s/%s", path_proc_zfs, filename);

    /* coverity[TOCTOU] */
    if (access(path, R_OK))
        return 0;

    char name[256];
    size_t prefix_len = strlen(prefix);
    if (prefix_len > sizeof(name)-1)
        return -1;
    sstrncpy(name, prefix, sizeof(name));

    FILE *fh = fopen(path, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Cannot fopen '%s': %s", path, STRERRNO);
        return -1;
    }

    char buffer[256];

    int lineno = 0;
    char *fields[4];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        lineno++;
        if (lineno < 3)
            continue;
        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (fields_num != 3)
            continue;

        zfs_append_metric(name, prefix_len, sizeof(name), fields[0], fields[1], fields[2]);
    }

    fclose(fh);
    return 0;
}

static int zfs_read_pool(int dir_fd, const char *dirname, const char *filename,
                                     __attribute__((unused)) void *user_data)
{
    if ((filename[0] == '.') && (filename[1] == '\0'))
        return 0;

    if ((filename[0] == '.') && (filename[1] == '.') && (filename[2] == '\0'))
        return 0;

    if (strcmp(filename, "io") == 0) {
        if (zfs_flags & COLLECT_IO)
            zfs_read_proc_pool_io(dir_fd, dirname);
    } else if (strcmp(filename, "state") == 0) {
        if (zfs_flags & COLLECT_STATE)
            zfs_read_proc_pool_state(dir_fd, dirname);
    } else if (strncmp(filename, "objset-", strlen("objset-")) == 0) {
        if (zfs_flags & COLLECT_OBJSET)
            zfs_read_proc_pool_objset(dir_fd, dirname, filename);
    }

    return 0;
}

static int zfs_read_proc(int dir_fd, const char *dirname, const char *filename,
                                     __attribute__((unused)) void *user_data)
{
    struct stat statbuf;
    int status = fstatat(dir_fd, filename, &statbuf, 0);
    if (status != 0) {
        PLUGIN_ERROR("stat (%s) in %s failed: %s.", filename, dirname, STRERRNO);
        return -1;
    }

    if (S_ISDIR(statbuf.st_mode))
        walk_directory_at(dir_fd, filename, zfs_read_pool, NULL, 0);

    return 0;
}

int zfs_read(void)
{

    if (zfs_flags & COLLECT_ABDSTATS)
        zfs_read_proc_file("abdstats", "abdstats.");
    if (zfs_flags & COLLECT_ARCSTATS)
        zfs_read_proc_file("arcstats", "arcstats.");
    if (zfs_flags & COLLECT_DBUFSTATS)
        zfs_read_proc_file("dbufstats", "dbufstats.");
    if (zfs_flags & COLLECT_DMU_TX)
        zfs_read_proc_file("dmu_tx", "dmu_tx.");
    if (zfs_flags & COLLECT_DNODESTATS)
        zfs_read_proc_file("dnodestats", "dnodestats.");
    if (zfs_flags & COLLECT_FM)
        zfs_read_proc_file("fm", "fm.");
    if (zfs_flags & COLLECT_QAT)
        zfs_read_proc_file("qat", "qat.");
    if (zfs_flags & COLLECT_VDEV_CACHE_STATS)
        zfs_read_proc_file("vdev_cache_stats", "vdev_cache_stats.");
    if (zfs_flags & COLLECT_VDEV_MIRROR_STATS)
        zfs_read_proc_file("vdev_mirror_stats", "vdev_mirror_stats.");
    //  no known consumers of the XUIO interface on Linux exist
    if (zfs_flags & COLLECT_XUIO_STATS)
        zfs_read_proc_file("xuio_stats", "xuio_stats.");
    if (zfs_flags & COLLECT_ZFETCHSTATS)
        zfs_read_proc_file("zfetchstats", "zfetchstats.");
    if (zfs_flags & COLLECT_ZIL)
        zfs_read_proc_file("zil", "zil.");

    if (zfs_flags & (COLLECT_IO | COLLECT_STATE | COLLECT_OBJSET))
        walk_directory(path_proc_zfs, zfs_read_proc, NULL, 0);

    plugin_dispatch_metric_family_array(fams_zfs, FAM_ZFS_MAX, 0);

    return 0;
}

int zfs_init(void)
{
    path_proc_zfs = plugin_procpath("spl/kstat/zfs");
    if (path_proc_zfs == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

int zfs_shutdown(void)
{
    free(path_proc_zfs);
    return 0;
}
