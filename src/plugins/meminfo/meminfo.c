// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

static char *path_proc_meminfo;
static plugin_filter_t *filter;

enum {
    FAM_MEMINFO_MEMORY_TOTAL_BYTES,
    FAM_MEMINFO_MEMORY_FREE_BYTES,
    FAM_MEMINFO_MEMORY_AVAILABLE_BYTES,
    FAM_MEMINFO_BUFFERS_BYTES,
    FAM_MEMINFO_CACHED_BYTES,
    FAM_MEMINFO_SWAP_CACHED_BYTES,
    FAM_MEMINFO_ACTIVE_BYTES,
    FAM_MEMINFO_INACTIVE_BYTES,
    FAM_MEMINFO_HIGH_TOTAL_BYTES,
    FAM_MEMINFO_HIGH_FREE_BYTES,
    FAM_MEMINFO_LOW_TOTAL_BYTES,
    FAM_MEMINFO_LOW_FREE_BYTES,
    FAM_MEMINFO_ACTIVE_ANONYMOUS_BYTES,
    FAM_MEMINFO_INACTIVE_ANONYMOUS_BYTES,
    FAM_MEMINFO_ACTIVE_PAGE_CACHE_BYTES,
    FAM_MEMINFO_INACTIVE_PAGE_CACHE_BYTES,
    FAM_MEMINFO_UNEVICTABLE_BYTES,
    FAM_MEMINFO_MLOCKED_BYTES,
    FAM_MEMINFO_SWAP_TOTAL_BYTES,
    FAM_MEMINFO_SWAP_FREE_BYTES,
    FAM_MEMINFO_ZSWAP_TOTAL_BYTES,
    FAM_MEMINFO_ZSWAP_STORED_BYTES,
    FAM_MEMINFO_DIRTY_BYTES,
    FAM_MEMINFO_WRITEBACK_BYTES,
    FAM_MEMINFO_ANONYMOUS_BYTES,
    FAM_MEMINFO_MAPPED_BYTES,
    FAM_MEMINFO_SHMEM_BYTES,
    FAM_MEMINFO_KERNEL_RECLAIMABLE_BYTES,
    FAM_MEMINFO_SLAB_BYTES,
    FAM_MEMINFO_SLAB_RECLAIMABLE_BYTES,
    FAM_MEMINFO_SLAB_UNRECLAIMABLE_BYTES,
    FAM_MEMINFO_KERNEL_STACK_BYTES,
    FAM_MEMINFO_PAGE_TABLES_BYTES,
    FAM_MEMINFO_SECONDARY_PAGE_TABLES_BYTES,
    FAM_MEMINFO_BOUNCE_BYTES,
    FAM_MEMINFO_WRITE_BACK_TMP_BYTES,
    FAM_MEMINFO_COMMIT_LIMIT_BYTES,
    FAM_MEMINFO_COMMITTED_BYTES,
    FAM_MEMINFO_VMALLOC_TOTAL_BYTES,
    FAM_MEMINFO_VMALLOC_USED_BYTES,
    FAM_MEMINFO_VMALLOC_CHUNCK_BYTES,
    FAM_MEMINFO_PERCPU_BYTES,
    FAM_MEMINFO_EARLY_MEMTEST_BAD_BYTES,
    FAM_MEMINFO_HARDWARE_CORRUPTED_BYTES,
    FAM_MEMINFO_ANONYMOUS_HUGEPAGES_BYTES,
    FAM_MEMINFO_SHMEM_HUGEPAGES_BYTES,
    FAM_MEMINFO_SHMEM_HUGEPAGES_PMDMAPPED_BYTES,
    FAM_MEMINFO_FILE_HUGEPAGES_BYTES,
    FAM_MEMINFO_FILE_HUGEPAGES_PMDMAPPED_BYTES,
    FAM_MEMINFO_CMA_TOTAL_BYTES,
    FAM_MEMINFO_CMA_FREE_BYTES,
    FAM_MEMINFO_HUGEPAGES,
    FAM_MEMINFO_HUGEPAGES_FREE,
    FAM_MEMINFO_HUGEPAGES_RESERVED,
    FAM_MEMINFO_HUGEPAGES_SURPASSED,
    FAM_MEMINFO_HUGEPAGE_SIZE_BYTES,
    FAM_MEMINFO_HUGEPAGES_BYTES,
    FAM_MEMINFO_DIRECTMAP_4K_BYTES,
    FAM_MEMINFO_DIRECTMAP_2M_BYTES,
    FAM_MEMINFO_DIRECTMAP_2G_BYTES,
    FAM_MEMINFO_MAX,
};

static metric_family_t fams[FAM_MEMINFO_MAX] = {
    [FAM_MEMINFO_MEMORY_TOTAL_BYTES] = {
        .name = "system_meminfo_memory_total_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total usable RAM (i.e. physical RAM minus a few reserved bits "
                "and the kernel binary code).",
    },
    [FAM_MEMINFO_MEMORY_FREE_BYTES] = {
        .name = "system_meminfo_memory_free_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total free RAM. On highmem systems, the sum of LowFree+HighFree."
    },
    [FAM_MEMINFO_MEMORY_AVAILABLE_BYTES] = {
        .name = "system_meminfo_memory_available_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "An estimate of how much memory is available for starting new applications, "
                "without swapping.",
    },
    [FAM_MEMINFO_BUFFERS_BYTES] = {
        .name = "system_meminfo_buffers_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Relatively temporary storage for raw disk blocks.",
    },
    [FAM_MEMINFO_CACHED_BYTES] = {
        .name = "system_meminfo_cached_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "In-memory cache for files read from the disk (the pagecache) "
                "as well as tmpfs & shmem. Doesn’t include SwapCached.",
    },
    [FAM_MEMINFO_SWAP_CACHED_BYTES] = {
        .name = "system_meminfo_swap_cached_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory that once was swapped out, is swapped back in "
                "but still also is in the swapfile.",
    },
    [FAM_MEMINFO_ACTIVE_BYTES] = {
        .name = "system_meminfo_active_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory that has been used more recently and usually not reclaimed "
                "unless absolutely necessary.",
    },
    [FAM_MEMINFO_INACTIVE_BYTES] = {
        .name = "system_meminfo_inactive_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory which has been less recently used. "
                "It is more eligible to be reclaimed for other purposes.",
    },
    [FAM_MEMINFO_HIGH_TOTAL_BYTES] = {
        .name = "system_meminfo_high_total_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The total amount of high memory.",
    },
    [FAM_MEMINFO_HIGH_FREE_BYTES] = {
        .name = "system_meminfo_high_free_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The total amount of high memory free.",
    },
    [FAM_MEMINFO_LOW_TOTAL_BYTES] = {
        .name = "system_meminfo_low_total_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The total amount of non-highmem memory.",
    },
    [FAM_MEMINFO_LOW_FREE_BYTES] = {
        .name = "system_meminfo_low_free_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The total amount of non-highmem memory free.",
    },
    [FAM_MEMINFO_ACTIVE_ANONYMOUS_BYTES] = {
        .name = "system_meminfo_active_anonymous_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Anonymous memory that has been used more recently and usually not swapped out.",
    },
    [FAM_MEMINFO_INACTIVE_ANONYMOUS_BYTES] = {
        .name = "system_meminfo_inactive_anonymous_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Anonymous memory that has not been used recently and can be swapped out.",
    },
    [FAM_MEMINFO_ACTIVE_PAGE_CACHE_BYTES] = {
        .name = "system_meminfo_active_page_cache_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Pagecache memory that has been used more recently "
                "and usually not reclaimed until needed.",
    },
    [FAM_MEMINFO_INACTIVE_PAGE_CACHE_BYTES] = {
        .name = "system_meminfo_inactive_page_cache_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Pagecache memory that can be reclaimed without huge performance impact.",
    },
    [FAM_MEMINFO_UNEVICTABLE_BYTES] = {
        .name = "system_meminfo_unevictable_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory allocated for userspace which cannot be reclaimed, such as "
                "mlocked pages, ramfs backing pages, secret memfd pages etc.",
    },
    [FAM_MEMINFO_MLOCKED_BYTES] = {
        .name = "system_meminfo_mlocked_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory locked with mlock().",
    },
    [FAM_MEMINFO_SWAP_TOTAL_BYTES] = {
        .name = "system_meminfo_swap_total_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total amount of swap space available.",
    },
    [FAM_MEMINFO_SWAP_FREE_BYTES] = {
        .name = "system_meminfo_swap_free_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The remaining swap space available.",
    },
    [FAM_MEMINFO_ZSWAP_TOTAL_BYTES] = {
        .name = "system_meminfo_zswap_total_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory consumed by the zswap backend (compressed size).",
    },
    [FAM_MEMINFO_ZSWAP_STORED_BYTES] = {
        .name = "system_meminfo_zswap_stored_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of anonymous memory stored in zswap (original size).",
    },
    [FAM_MEMINFO_DIRTY_BYTES] = {
        .name = "system_meminfo_dirty_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory which is waiting to get written back to the disk.",
    },
    [FAM_MEMINFO_WRITEBACK_BYTES] = {
        .name = "system_meminfo_writeback_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory which is actively being written back to the disk.",
    },
    [FAM_MEMINFO_ANONYMOUS_BYTES] = {
        .name = "system_meminfo_anonymous_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Non-file backed pages mapped into userspace page tables.",
    },
    [FAM_MEMINFO_MAPPED_BYTES] = {
        .name = "system_meminfo_mapped_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Files which have been mapped, such as libraries.",
    },
    [FAM_MEMINFO_SHMEM_BYTES] = {
        .name = "system_meminfo_shmem_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total memory used by shared memory (shmem) and tmpfs.",
    },
    [FAM_MEMINFO_KERNEL_RECLAIMABLE_BYTES] = {
        .name = "system_meminfo_kernel_reclaimable_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Kernel allocations that the kernel will attempt "
                "to reclaim under memory pressure.",
    },
    [FAM_MEMINFO_SLAB_BYTES] = {
        .name = "system_meminfo_slab_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "In-kernel data structures cache.",
    },
    [FAM_MEMINFO_SLAB_RECLAIMABLE_BYTES] = {
        .name = "system_meminfo_slab_reclaimable_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Part of Slab, that might be reclaimed, such as caches.",
    },
    [FAM_MEMINFO_SLAB_UNRECLAIMABLE_BYTES] = {
        .name = "system_meminfo_slab_unreclaimable_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Part of Slab, that cannot be reclaimed on memory pressure.",
    },
    [FAM_MEMINFO_KERNEL_STACK_BYTES] = {
        .name = "system_meminfo_kernel_stack_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory consumed by the kernel stacks of all tasks. This is not reclaimable."
    },
    [FAM_MEMINFO_PAGE_TABLES_BYTES] = {
        .name = "system_meminfo_page_tables_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory dedicated to the lowest level of page tables.",
    },
    [FAM_MEMINFO_SECONDARY_PAGE_TABLES_BYTES] = {
        .name = "system_meminfo_secondary_page_tables_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory consumed by secondary page tables, this currently includes "
                "KVM mmu allocations on x86 and arm64.",
    },
    [FAM_MEMINFO_BOUNCE_BYTES] = {
        .name = "system_meminfo_bounce_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory used for block device \"bounce buffers\".",
    },
    [FAM_MEMINFO_WRITE_BACK_TMP_BYTES] = {
        .name = "system_meminfo_write_back_tmp_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory used by FUSE for temporary writeback buffers.",
    },
    [FAM_MEMINFO_COMMIT_LIMIT_BYTES] = {
        .name = "system_meminfo_commit_limit_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "This is the total amount of memory currently available "
                "to be allocated on the system.",
    },
    [FAM_MEMINFO_COMMITTED_BYTES] = {
        .name = "system_meminfo_committed_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The amount of memory presently allocated on the system.",
    },
    [FAM_MEMINFO_VMALLOC_TOTAL_BYTES] = {
        .name = "system_meminfo_vmalloc_total_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total size of vmalloc virtual address space.",
    },
    [FAM_MEMINFO_VMALLOC_USED_BYTES] = {
        .name = "system_meminfo_vmalloc_used_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of vmalloc area which is used.",
    },
    [FAM_MEMINFO_VMALLOC_CHUNCK_BYTES] = {
        .name = "system_meminfo_vmalloc_chunck_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Largest contiguous block of vmalloc area which is free.",
    },
    [FAM_MEMINFO_PERCPU_BYTES] = {
        .name = "system_meminfo_percpu_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory allocated to the percpu allocator used to back percpu allocations. "
                "This stat excludes the cost of metadata.",
    },
    [FAM_MEMINFO_EARLY_MEMTEST_BAD_BYTES] = {
        .name = "system_meminfo_early_memtest_bad_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The amount of RAM/memory in kB, that was identified as corrupted "
                "by early memtest."
    },
    [FAM_MEMINFO_HARDWARE_CORRUPTED_BYTES] = {
        .name = "system_meminfo_hardware_corrupted_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The amount of RAM, the kernel identified as corrupted / not working.",
    },
    [FAM_MEMINFO_ANONYMOUS_HUGEPAGES_BYTES] = {
        .name = "system_meminfo_anonumous_hugepages_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total size of non-file backed huge pages mapped into userspace page tables.",
    },
    [FAM_MEMINFO_SHMEM_HUGEPAGES_BYTES] = {
        .name = "system_meminfo_shmem_hugepages_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory used by shared memory (shmem) and tmpfs allocated with huge pages.",
    },
    [FAM_MEMINFO_SHMEM_HUGEPAGES_PMDMAPPED_BYTES] = {
        .name = "system_meminfo_shmem_hugepages_pmdmapped_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Shared memory mapped into userspace with huge pages",
    },
    [FAM_MEMINFO_FILE_HUGEPAGES_BYTES] = {
        .name = "system_meminfo_file_hugepages_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory used for filesystem data (page cache) allocated with huge pages.",
    },
    [FAM_MEMINFO_FILE_HUGEPAGES_PMDMAPPED_BYTES] = {
        .name = "system_meminfo_file_hugepages_pmdmapped_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Page cache mapped into userspace with huge pages.",
    },
    [FAM_MEMINFO_CMA_TOTAL_BYTES] = {
        .name = "system_meminfo_cma_total_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory reserved for the Contiguous Memory Allocator (CMA).",
    },
    [FAM_MEMINFO_CMA_FREE_BYTES] = {
        .name = "system_meminfo_cma_free_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Free remaining memory in the CMA reserves.",
    },
    [FAM_MEMINFO_HUGEPAGES] = {
        .name = "system_meminfo_hugepages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of hugepages being allocated by the kernel.",
    },
    [FAM_MEMINFO_HUGEPAGES_FREE] = {
        .name = "system_meminfo_hugepages_free",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of hugepages not being allocated by a process."
    },
    [FAM_MEMINFO_HUGEPAGES_RESERVED] = {
        .name = "system_meminfo_hugepages_reserved",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of hugepages for which a commitment to allocate from the pool "
                "has been made, but no allocation has yet been made.",
    },
    [FAM_MEMINFO_HUGEPAGES_SURPASSED] = {
        .name = "system_meminfo_hugepages_surpassed",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of hugepages in the pool above the value in vm.nr_hugepages.",
    },
    [FAM_MEMINFO_HUGEPAGE_SIZE_BYTES] = {
        .name = "system_meminfo_hugepage_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The default size of a hugepage.",
    },
    [FAM_MEMINFO_HUGEPAGES_BYTES] = {
        .name = "system_meminfo_hugepages_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total amount of memory consumed by huge pages of all sizes.",
    },
    [FAM_MEMINFO_DIRECTMAP_4K_BYTES] = {
        .name = "system_meminfo_directmap_4k_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The amount of memory being mapped to standard 4k pages."
    },
    [FAM_MEMINFO_DIRECTMAP_2M_BYTES] = {
        .name = "system_meminfo_directmap_2M_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The amount of memory being mapped to hugepages (usually 2MB in size)."
    },
    [FAM_MEMINFO_DIRECTMAP_2G_BYTES] = {
        .name = "system_meminfo_directmap_2G_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The amount of memory being mapped to hugepages (usually 1GB in size)."
    },
};

#include "plugins/meminfo/meminfo.h"

static int meminfo_read(void)
{
    FILE *fh = fopen(path_proc_meminfo, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Cannot open '%s' failed: %s", path_proc_meminfo, STRERRNO);
        return -1;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *fields[4] = {0};

        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (fields_num < 2)
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

        metric_family_append(&fams[mm->fam], VALUE_GAUGE(value), NULL, NULL);
    }
    fclose(fh);

    plugin_dispatch_metric_family_array_filtered(fams, FAM_MEMINFO_MAX, filter, 0);


    return 0;
}

static int meminfo_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &filter);
        } else {
            PLUGIN_ERROR("The configuration option '%s' in %s:%d is not allowed here.",
                         child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int meminfo_init(void)
{
    path_proc_meminfo = plugin_procpath("meminfo");
    if (path_proc_meminfo == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int meminfo_shutdown(void)
{
    free(path_proc_meminfo);
    plugin_filter_free(filter);
    return 0;
}

void module_register(void)
{
    plugin_register_init("meminfo", meminfo_init);
    plugin_register_config("meminfo", meminfo_config);
    plugin_register_read("meminfo", meminfo_read);
    plugin_register_shutdown("meminfo", meminfo_shutdown);
}
