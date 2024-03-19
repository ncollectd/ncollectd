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

metric_family_t fams[FAM_MEMORY_MAX] = {
    [FAM_MEMORY_USED_BYTES] = {
        .name = "system_memory_used_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Used memory in bytes.",
    },
    [FAM_MEMORY_FREE_BYTES] = {
        .name = "system_memory_free_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Unused memory in bytes.",
    },
    [FAM_MEMORY_BUFFERS_BYTES] = {
        .name = "system_memory_buffers_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory used by kernel buffers in bytes.",
    },
    [FAM_MEMORY_CACHED_BYTES] = {
        .name = "system_memory_cached_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory used by the page cache and slabs in bytes.",
    },
    [FAM_MEMORY_SLAB_BYTES] = {
        .name = "system_memory_slab_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "In-kernel data structures cache.",
    },
    [FAM_MEMORY_SLAB_RECLAIMABLE_BYTES] = {
        .name = "system_memory_slab_reclaimable_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Part of Slab, that might be reclaimed, such as caches.",
    },
    [FAM_MEMORY_SLAB_UNRECLAIMABLE_BYTES] = {
        .name = "system_memory_slab_unreclaimable_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Part of Slab, that cannot be reclaimed on memory pressure.",
    },
    [FAM_MEMORY_WIRED_BYTES] = {
        .name = "system_memory_wired_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MEMORY_ACTIVE_BYTES] = {
        .name = "system_memory_active_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MEMORY_INACTIVE_BYTES] = {
        .name = "system_memory_inactive_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MEMORY_KERNEL_BYTES] = {
        .name = "system_memory_kernel_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MEMORY_LOCKED_BYTES] = {
        .name = "system_memory_locked_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MEMORY_ARC_BYTES] = {
        .name = "system_memory_arc_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MEMORY_UNUSED_BYTES] = {
        .name = "system_memory_unused_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
};

int memory_read(void);

__attribute__(( weak ))
int memory_init(void)
{
    return 0;
}

__attribute__(( weak ))
int memory_shutdown(void)
{
    return 0;
}

void module_register(void)
{
    plugin_register_init("memory", memory_init);
    plugin_register_read("memory", memory_read);
    plugin_register_shutdown("memory", memory_shutdown);
}
