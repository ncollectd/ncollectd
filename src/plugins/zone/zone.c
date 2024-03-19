// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2011  Mathijs Mohlmann
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Mathijs Mohlmann
// SPDX-FileContributor: Dagobert Michelsen (forward-porting)
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#ifdef HAVE_CONFIG_H
#include "config.h"
#undef HAVE_CONFIG_H
#endif
/* avoid procfs.h error "Cannot use procfs in the large file compilation environment" */
#if !defined(_LP64) && _FILE_OFFSET_BITS == 64
#undef _FILE_OFFSET_BITS
#undef _LARGEFILE64_SOURCE
#endif

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/avltree.h"

#include <procfs.h>
#include <zone.h>

#define MAX_PROCFS_PATH 40
#define FRC2PCT(pp) (((float)(pp)) / 0x8000 * 100)

typedef struct {
    ushort_t pctcpu;
    ushort_t pctmem;
} zone_stats_t;

enum {
    FAM_ZONE_CPU_PERCENT,
    FAM_ZONE_MEMORY_PERCENT,
    FAM_ZONE_MAX,
};

static metric_family_t fams[FAM_ZONE_MAX] = {
    [FAM_ZONE_CPU_PERCENT] = {
        .name = "system_zone_cpu_percent",
        .type = METRIC_TYPE_GAUGE,
        .help = "% of recent cpu time used by all lwp.",
    },
    [FAM_ZONE_MEMORY_PERCENT] = {
        .name = "system_zone_memory_percent",
        .type = METRIC_TYPE_GAUGE,
        .help = "% of system memory used by process.",
    },
};

static int zone_compare(const void *a, const void *b)
{
    if (*(const zoneid_t *)a == *(const zoneid_t *)b)
        return 0;
    if (*(const zoneid_t *)a < *(const zoneid_t *)b)
        return -1;
    return 1;
}

static int zone_read_procfile(char const *pidstr, char const *name, void *buf, size_t bufsize)
{
    char procfile[MAX_PROCFS_PATH];
    (void)snprintf(procfile, sizeof(procfile), "/proc/%s/%s", pidstr, name);

    int fd = open(procfile, O_RDONLY);
    if (fd == -1)
        return 1;

    if (sread(fd, buf, bufsize) != 0) {
        PLUGIN_ERROR("Reading \"%s\" failed: %s", procfile, STRERRNO);
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

static int zone_read(void)
{
    DIR *procdir = opendir("/proc");
    if (procdir == NULL) {
        PLUGIN_ERROR("cannot open /proc directory\n");
        return -1;
    }

    c_avl_tree_t *tree = c_avl_create(zone_compare);
    if (tree == NULL) {
        PLUGIN_WARNING("Failed to create tree");
        closedir(procdir);
        return -1;
    }

    rewinddir(procdir);
    dirent_t *direntp;
    while ((direntp = readdir(procdir))) {
        char const *pidstr = direntp->d_name;
        if (pidstr[0] == '.') /* skip "." and ".."  */
            continue;

        pid_t pid = atoi(pidstr);
        if (pid == 0 || pid == 2 || pid == 3)
            continue; /* skip sched, pageout and fsflush */

        psinfo_t psinfo;
        if (zone_read_procfile(pidstr, "psinfo", &psinfo, sizeof(psinfo_t)) != 0)
            continue;

        zoneid_t zoneid = psinfo.pr_zoneid;
        zone_stats_t *stats = NULL;
        if (c_avl_get(tree, (void **)&zoneid, (void **)&stats)) {
            stats = malloc(sizeof(*stats));
            if (stats == NULL) {
                PLUGIN_WARNING("no memory");
                continue;
            }

            zoneid_t *key = malloc(sizeof(*key));
            if (key == NULL) {
                PLUGIN_WARNING("no memory");
                free(stats);
                continue;
            }

            *key = zoneid;
            if (c_avl_insert(tree, key, stats)) {
                PLUGIN_WARNING("error inserting into tree");
                free(key);
                free(stats);
                continue;
            }
        }

        if (stats) {
            stats->pctcpu += psinfo.pr_pctcpu;
            stats->pctmem += psinfo.pr_pctmem;
        }
    }

    closedir(procdir);

    zoneid_t *zoneid = NULL;
    zone_stats_t *stats = NULL;
    while (c_avl_pick(tree, (void **)&zoneid, (void **)&stats) == 0) {
        char zonename[ZONENAME_MAX];
        if (getzonenamebyid(*zoneid, zonename, sizeof(zonename)) == -1) {
            PLUGIN_WARNING("error retrieving zonename");
        } else {
            metric_family_append(&fams[FAM_ZONE_CPU_PERCENT],
                                 VALUE_GAUGE(FRC2PCT(stats->pctcpu)), NULL,
                                 &(label_pair_const_t){.name="zone", .value=zonename}, NULL);

            metric_family_append(&fams[FAM_ZONE_MEMORY_PERCENT],
                                 VALUE_GAUGE(FRC2PCT(stats->pctmem)), NULL,
                                 &(label_pair_const_t){.name="zone", .value=zonename}, NULL);

        }
        free(stats);
        free(zoneid);
    }

    c_avl_destroy(tree);

    plugin_dispatch_metric_family_array(fams, FAM_ZONE_MAX, 0);

    return 0;
}

void module_register(void)
{
    plugin_register_read("zone", zone_read);
}
