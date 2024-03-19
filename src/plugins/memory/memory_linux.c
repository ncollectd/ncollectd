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

int memory_read(void)
{
    FILE *fh = fopen(path_proc_meminfo, "r");
    if (fh == NULL) {
        int status = errno;
        PLUGIN_ERROR("fopen '%s' failed: %s", path_proc_meminfo, STRERRNO);
        return status;
    }

    double mem_free = NAN;
    double mem_buffers = NAN;
    double mem_cached = NAN;
    double mem_slab = NAN;
    double mem_slab_recl = NAN;
    double mem_slab_unrecl = NAN;
    double mem_total = 0;
    double mem_not_used = 0;

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *fields[4] = {NULL};
        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if ((fields_num != 3) || (strcmp("kB", fields[2]) != 0))
            continue;

        switch (fields[0][0]) {
        case 'B':
            if (strcmp(fields[0], "Buffers:") == 0) {
                mem_buffers = 1024.0 * atoll(fields[1]);
                mem_not_used += mem_buffers;
            }
            break;
        case 'C':
            if (strcmp(fields[0], "Cached:") == 0) {
                mem_cached = 1024.0 * atoll(fields[1]);
                mem_not_used += mem_cached;
            }
            break;
        case 'M':
            if (strcmp(fields[0], "MemTotal:") == 0) {
                mem_total = 1024.0 * atoll(fields[1]);
            } else if (strcmp(fields[0], "MemFree:") == 0) {
                mem_free = 1024.0 * atoll(fields[1]);
                mem_not_used += mem_free;
            }
            break;
        case 'S':
            if (strcmp(fields[0], "Slab:") == 0) {
                mem_slab = 1024.0 * atoll(fields[1]);
            } else if (strcmp(fields[0], "SReclaimable:") == 0) {
                mem_slab_recl = 1024.0 * atoll(fields[1]);
            } else if (strcmp(fields[0], "SUnreclaim:") == 0) {
                mem_slab_unrecl = 1024.0 * atoll(fields[1]);
            }
            break;
        }
    }

    fclose(fh);

    if (isnan(mem_total) || (mem_total == 0))
        return EINVAL;

    if (!isnan(mem_slab_recl))
        mem_not_used += mem_slab_recl;
    else if (!isnan(mem_slab))
        mem_not_used += mem_slab;

    if (mem_total < mem_not_used)
        return EINVAL;

    /* "used" is not explicitly reported. It is calculated as everything that is
     * not "not used", e.g. cached, buffers, ... */
    double mem_used = mem_total - mem_not_used;

    metric_family_append(&fams[FAM_MEMORY_USED_BYTES], VALUE_GAUGE(mem_used), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_FREE_BYTES], VALUE_GAUGE(mem_free), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_BUFFERS_BYTES], VALUE_GAUGE(mem_buffers), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_CACHED_BYTES], VALUE_GAUGE(mem_cached), NULL, NULL);

    /* SReclaimable and SUnreclaim were introduced in kernel 2.6.19
     * They sum up to the value of Slab, which is available on older & newer
     * kernels. So SReclaimable/SUnreclaim are submitted if available, and Slab
     * if not. */
    if (!isnan(mem_slab_recl) || !isnan(mem_slab_unrecl)) {
        metric_family_append(&fams[FAM_MEMORY_SLAB_RECLAIMABLE_BYTES],
                             VALUE_GAUGE(mem_slab_recl), NULL, NULL);
        metric_family_append(&fams[FAM_MEMORY_SLAB_UNRECLAIMABLE_BYTES],
                             VALUE_GAUGE(mem_slab_unrecl), NULL, NULL);
    } else {
        metric_family_append(&fams[FAM_MEMORY_SLAB_BYTES],
                             VALUE_GAUGE(mem_slab), NULL, NULL);
    }

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
