// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2020 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2009 Simon Kuhnle
// SPDX-FileCopyrightText: Copyright (C) 2009-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Simon Kuhnle <simon at blarzwurst.de>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include "memory.h"

static char *path_proc_meminfo;

extern metric_family_t fams[];

typedef enum {
    MEMINFO_MEMORY_TOTAL,
    MEMINFO_MEMORY_FREE,
    MEMINFO_MEMORY_AVAILABLE,
    MEMINFO_BUFFERS,
    MEMINFO_CACHED,
    MEMINFO_SWAP_CACHED,
    MEMINFO_ANONYMOUS_PAGES,
    MEMINFO_SHMEM,
    MEMINFO_SLAB,
    MEMINFO_SLAB_RECLAIMABLE,
    MEMINFO_SLAB_UNRECLAIMABLE,
    MEMINFO_KERNEL_STACK,
    MEMINFO_PAGE_TABLES,
    MEMINFO_HUGEPAGES,
    MEMINFO_MAX
} meminfo_t;

#include "plugins/memory/meminfo.h"

int memory_read(void)
{
    FILE *fh = fopen(path_proc_meminfo, "r");
    if (fh == NULL) {
        int status = errno;
        PLUGIN_ERROR("fopen '%s' failed: %s", path_proc_meminfo, STRERRNO);
        return status;
    }

    double meminfo[MEMINFO_MAX];

    meminfo[MEMINFO_MEMORY_TOTAL] = 0;
    for (size_t i = 1; i < MEMINFO_MAX; i++) {
        meminfo[i] = NAN;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *fields[5] = {NULL};
        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if ((fields_num < 2) || (fields_num > 3))
            continue;

        const struct meminfo_metric *mm = meminfo_get_key(fields[0], strlen(fields[0]));
        if (mm == NULL)
            continue;

        double value = atof(fields[1]);

        if (fields_num == 3) {
            if ((fields[2][0] == 'k') && (fields[2][1] == 'B')) {
                value *= 1024.0;
            } else {
                continue;
            }
        }

        if (!isfinite(value))
            continue;

        meminfo[mm->mkey] = value;
    }

    fclose(fh);

    if (isnan(meminfo[MEMINFO_MEMORY_TOTAL]) || (meminfo[MEMINFO_MEMORY_TOTAL] == 0))
        return EINVAL;

    double mem_used = 0;

    if (isnan(meminfo[MEMINFO_MEMORY_AVAILABLE]) || (meminfo[MEMINFO_MEMORY_AVAILABLE]== 0)) {
        double mem_not_used = 0;

        if (!isnan(meminfo[MEMINFO_BUFFERS]))
            mem_not_used += meminfo[MEMINFO_BUFFERS];
        if (!isnan(meminfo[MEMINFO_CACHED]))
            mem_not_used += meminfo[MEMINFO_CACHED];
        if (!isnan(meminfo[MEMINFO_MEMORY_FREE]))
            mem_not_used += meminfo[MEMINFO_MEMORY_FREE];

        if (!isnan(meminfo[MEMINFO_SLAB_RECLAIMABLE]))
            mem_not_used += meminfo[MEMINFO_SLAB_RECLAIMABLE];
        else if (!isnan(meminfo[MEMINFO_SLAB]))
            mem_not_used += meminfo[MEMINFO_SLAB];

        if (meminfo[MEMINFO_MEMORY_TOTAL] < mem_not_used)
            return EINVAL;

        /* "used" is not explicitly reported. It is calculated as everything that is
         * not "not used", e.g. cached, buffers, ... */
        mem_used = meminfo[MEMINFO_MEMORY_TOTAL] - mem_not_used;
    } else {
        if (meminfo[MEMINFO_MEMORY_AVAILABLE] > meminfo[MEMINFO_MEMORY_TOTAL])
            mem_used = meminfo[MEMINFO_MEMORY_TOTAL] - meminfo[MEMINFO_MEMORY_FREE];
        else
            mem_used = meminfo[MEMINFO_MEMORY_TOTAL] - meminfo[MEMINFO_MEMORY_AVAILABLE];
    }

    metric_family_append(&fams[FAM_MEMORY_TOTAL_BYTES],
                         VALUE_GAUGE(meminfo[MEMINFO_MEMORY_TOTAL]), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_USED_BYTES],
                         VALUE_GAUGE(mem_used), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_FREE_BYTES],
                         VALUE_GAUGE(meminfo[MEMINFO_MEMORY_FREE]), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_SHARED_BYTES],
                         VALUE_GAUGE(meminfo[MEMINFO_SHMEM]), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_BUFFERS_BYTES],
                         VALUE_GAUGE(meminfo[MEMINFO_BUFFERS]), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_CACHED_BYTES],
                         VALUE_GAUGE(meminfo[MEMINFO_CACHED]), NULL, NULL);

    /* SReclaimable and SUnreclaim were introduced in kernel 2.6.19
     * They sum up to the value of Slab, which is available on older & newer
     * kernels. So SReclaimable/SUnreclaim are submitted if available, and Slab
     * if not. */
    if (!isnan(meminfo[MEMINFO_SLAB_RECLAIMABLE]) || !isnan(meminfo[MEMINFO_SLAB_UNRECLAIMABLE])) {
        metric_family_append(&fams[FAM_MEMORY_SLAB_RECLAIMABLE_BYTES],
                             VALUE_GAUGE(meminfo[MEMINFO_SLAB_RECLAIMABLE]), NULL, NULL);
        metric_family_append(&fams[FAM_MEMORY_SLAB_UNRECLAIMABLE_BYTES],
                             VALUE_GAUGE(meminfo[MEMINFO_SLAB_UNRECLAIMABLE]), NULL, NULL);
    } else {
        metric_family_append(&fams[FAM_MEMORY_SLAB_BYTES],
                             VALUE_GAUGE(meminfo[MEMINFO_SLAB]), NULL, NULL);
    }

    if (!isnan(meminfo[MEMINFO_MEMORY_AVAILABLE]) && (meminfo[MEMINFO_MEMORY_AVAILABLE] != 0))
        metric_family_append(&fams[FAM_MEMORY_AVAILABLE_BYTES],
                             VALUE_GAUGE(meminfo[MEMINFO_MEMORY_AVAILABLE]), NULL, NULL);
    if (!isnan(meminfo[MEMINFO_SWAP_CACHED]))
        metric_family_append(&fams[FAM_MEMORY_SWAP_CACHED_BYTES],
                             VALUE_GAUGE(meminfo[MEMINFO_SWAP_CACHED]), NULL, NULL);
    if (!isnan(meminfo[MEMINFO_ANONYMOUS_PAGES]))
        metric_family_append(&fams[FAM_MEMORY_ANONYMOUS_BYTES],
                             VALUE_GAUGE(meminfo[MEMINFO_ANONYMOUS_PAGES]), NULL, NULL);
    if (!isnan(meminfo[MEMINFO_KERNEL_STACK]))
        metric_family_append(&fams[FAM_MEMORY_KERNEL_STACK_BYTES],
                             VALUE_GAUGE(meminfo[MEMINFO_KERNEL_STACK]), NULL, NULL);
    if (!isnan(meminfo[MEMINFO_PAGE_TABLES]))
        metric_family_append(&fams[FAM_MEMORY_PAGE_TABLES_BYTES],
                             VALUE_GAUGE(meminfo[MEMINFO_PAGE_TABLES]), NULL, NULL);
    if (!isnan(meminfo[MEMINFO_HUGEPAGES]))
        metric_family_append(&fams[FAM_MEMORY_HUGEPAGES_BYTES],
                             VALUE_GAUGE(meminfo[MEMINFO_HUGEPAGES]), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_MEMORY_MAX, 0);

    return 0;
}

int memory_init(void)
{
    path_proc_meminfo = plugin_procpath("meminfo");
    if (path_proc_meminfo == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

int memory_shutdown(void)
{
    free(path_proc_meminfo);
    return 0;
}
