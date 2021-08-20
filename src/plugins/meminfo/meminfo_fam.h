#pragma once 

enum {
  FAM_MEMINFO_MEMORY_TOTAL_BYTES,
  FAM_MEMINFO_MEMORY_FREE_BYTES,
  FAM_MEMINFO_MEMORY_AVAILABLE_BYTES,
  FAM_MEMINFO_BUFFERS_BYTES,
  FAM_MEMINFO_CACHED_BYTES,
  FAM_MEMINFO_SWAP_CACHED_BYTES,
  FAM_MEMINFO_ACTIVE_BYTES,
  FAM_MEMINFO_INACTIVE_BYTES,
  FAM_MEMINFO_ACTIVE_ANONYMOUS_BYTES,
  FAM_MEMINFO_INACTIVE_ANONYMOUS_BYTES,
  FAM_MEMINFO_ACTIVE_PAGE_CACHE_BYTES,
  FAM_MEMINFO_INACTIVE_PAGE_CACHE_BYTES,
  FAM_MEMINFO_UNEVICTABLE_BYTES,
  FAM_MEMINFO_MLOCKED_BYTES,
  FAM_MEMINFO_SWAP_TOTAL_BYTES,
  FAM_MEMINFO_SWAP_FREE_BYTES,
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
  FAM_MEMINFO_BOUNCE_BYTES,
  FAM_MEMINFO_WRITE_BACK_TMP_BYTES,
  FAM_MEMINFO_COMMIT_LIMIT_BYTES,
  FAM_MEMINFO_COMMITTED_BYTES,
  FAM_MEMINFO_VMALLOC_TOTAL_BYTES,
  FAM_MEMINFO_VMALLOC_USED_BYTES,
  FAM_MEMINFO_VMALLOC_CHUNCK_BYTES,
  FAM_MEMINFO_PERCPU_BYTES,
  FAM_MEMINFO_HARDWARE_CORRUPTED_BYTES,
  FAM_MEMINFO_HUGEPAGES_ANONYMOUS_BYTES,
  FAM_MEMINFO_HUGEPAGES_SHMEM_BYTES,
  FAM_MEMINFO_HUGEPAGES_SHMEM_PMDMAPPED_BYTES,
  FAM_MEMINFO_HUGEPAGES_FILE_BYTES,
  FAM_MEMINFO_HUGEPAGES_FILE_PMDMAPPED_BYTES,
  FAM_MEMINFO_CMA_TOTAL_BYTES,
  FAM_MEMINFO_CMA_FREE_BYTES,
  FAM_MEMINFO_HUGEPAGES_TOTAL,
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
    .name = "host_meminfo_memory_total_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Total usable RAM (i.e. physical RAM minus a few reserved bits and the kernel binary code).",
  },
  [FAM_MEMINFO_MEMORY_FREE_BYTES] = {
    .name = "host_meminfo_memory_free_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "The sum of LowFree+HighFree.",
  },
  [FAM_MEMINFO_MEMORY_AVAILABLE_BYTES] = {
    .name = "host_meminfo_memory_available_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "An estimate of how much memory is available for starting new applications, without swapping.",
  },
  [FAM_MEMINFO_BUFFERS_BYTES] = {
    .name = "host_meminfo_buffers_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Relatively temporary storage for raw disk blocks.",
  },
  [FAM_MEMINFO_CACHED_BYTES] = {
    .name = "host_meminfo_cached_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "In-memory cache for files read from the disk (the pagecache). Doesn’t include SwapCached.",
  },
  [FAM_MEMINFO_SWAP_CACHED_BYTES] = {
    .name = "host_meminfo_swap_cached_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Memory that once was swapped out, is swapped back in but still also is in the swapfile.",
  },
  [FAM_MEMINFO_ACTIVE_BYTES] = {
    .name = "host_meminfo_active_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Memory that has been used more recently and usually not reclaimed unless absolutely necessary.",
  },
  [FAM_MEMINFO_INACTIVE_BYTES] = {
    .name = "host_meminfo_inactive_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Memory which has been less recently used. It is more eligible to be reclaimed for other purposes.",
  },
  [FAM_MEMINFO_ACTIVE_ANONYMOUS_BYTES] = {
    .name = "host_meminfo_active_anonymous_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Anonymous memory that has been used more recently and usually not swapped out.",
  },
  [FAM_MEMINFO_INACTIVE_ANONYMOUS_BYTES] = {
    .name = "host_meminfo_inactive_anonymous_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Anonymous memory that has not been used recently and can be swapped out.",
  },
  [FAM_MEMINFO_ACTIVE_PAGE_CACHE_BYTES] = {
    .name = "host_meminfo_active_page_cache_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Pagecache memory that has been used more recently and usually not reclaimed until needed.",
  },
  [FAM_MEMINFO_INACTIVE_PAGE_CACHE_BYTES] = {
    .name = "host_meminfo_inactive_page_cache_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Pagecache memory that can be reclaimed without huge performance impact.",
  },
  [FAM_MEMINFO_UNEVICTABLE_BYTES] = {
    .name = "host_meminfo_unevictable_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Unevictable pages can't be swapped out for a variety of reasons.",
  },
  [FAM_MEMINFO_MLOCKED_BYTES] = {
    .name = "host_meminfo_mlocked_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Pages locked to memory using the mlock() system call. Mlocked pages are also Unevictable.",
  },
  [FAM_MEMINFO_SWAP_TOTAL_BYTES] = {
    .name = "host_meminfo_swap_total_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Total amount of swap space available.",
  },
  [FAM_MEMINFO_SWAP_FREE_BYTES] = {
    .name = "host_meminfo_swap_free_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Memory which has been evicted from RAM, and is temporarily on the disk.",
  },
  [FAM_MEMINFO_DIRTY_BYTES] = {
    .name = "host_meminfo_dirty_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Memory which is waiting to get written back to the disk.",
  },
  [FAM_MEMINFO_WRITEBACK_BYTES] = {
    .name = "host_meminfo_writeback_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Memory which is actively being written back to the disk.",
  },
  [FAM_MEMINFO_ANONYMOUS_BYTES] = {
    .name = "host_meminfo_anonymous_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Non-file backed pages mapped into userspace page tables.",
  },
  [FAM_MEMINFO_MAPPED_BYTES] = {
    .name = "host_meminfo_mapped_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Files which have been mmaped, such as libraries.",
  },
  [FAM_MEMINFO_SHMEM_BYTES] = {
    .name = "host_meminfo_shmem_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Total memory used by shared memory (shmem) and tmpfs.",
  },
  [FAM_MEMINFO_KERNEL_RECLAIMABLE_BYTES] = {
    .name = "host_meminfo_kernel_reclaimable_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Kernel allocations that the kernel will attempt to reclaim under memory pressure.",
  },
  [FAM_MEMINFO_SLAB_BYTES] = {
    .name = "host_meminfo_slab_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "In-kernel data structures cache.",
  },
  [FAM_MEMINFO_SLAB_RECLAIMABLE_BYTES] = {
    .name = "host_meminfo_slab_reclaimable_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Part of Slab, that might be reclaimed, such as caches.",
  },
  [FAM_MEMINFO_SLAB_UNRECLAIMABLE_BYTES] = {
    .name = "host_meminfo_slab_unreclaimable_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Part of Slab, that cannot be reclaimed on memory pressure.",
  },
  [FAM_MEMINFO_KERNEL_STACK_BYTES] = {
    .name = "host_meminfo_kernel_stack_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "The memory the kernel stack uses. This is not reclaimable."
  },
  [FAM_MEMINFO_PAGE_TABLES_BYTES] = {
    .name = "host_meminfo_page_tables_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Amount of memory dedicated to the lowest level of page tables.",
  },
  [FAM_MEMINFO_BOUNCE_BYTES] = {
    .name = "host_meminfo_bounce_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Memory used for block device \"bounce buffersr\".",
  },
  [FAM_MEMINFO_WRITE_BACK_TMP_BYTES] = {
    .name = "host_meminfo_write_back_tmp_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Memory used by FUSE for temporary writeback buffers.",
  },
  [FAM_MEMINFO_COMMIT_LIMIT_BYTES] = {
    .name = "host_meminfo_commit_limit_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "This is the total amount of memory currently available to be allocated on the system.",
  },
  [FAM_MEMINFO_COMMITTED_BYTES] = {
    .name = "host_meminfo_committed_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "The amount of memory presently allocated on the system.",
  },
  [FAM_MEMINFO_VMALLOC_TOTAL_BYTES] = {
    .name = "host_meminfo_vmalloc_total_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Total size of vmalloc memory area.",
  },
  [FAM_MEMINFO_VMALLOC_USED_BYTES] = {
    .name = "host_meminfo_vmalloc_used_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Amount of vmalloc area which is used.",
  },
  [FAM_MEMINFO_VMALLOC_CHUNCK_BYTES] = {
    .name = "host_meminfo_vmalloc_chunck_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Largest contiguous block of vmalloc area which is free.",
  },
  [FAM_MEMINFO_PERCPU_BYTES] = {
    .name = "host_meminfo_percpu_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Memory allocated to the percpu allocator used to back percpu allocations. This stat excludes the cost of metadata.",
  },
  [FAM_MEMINFO_HARDWARE_CORRUPTED_BYTES] = {
    .name = "host_meminfo_hardware_corrupted_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "The amount of RAM the kernel identified as corrupted / not working.",
  },
  [FAM_MEMINFO_HUGEPAGES_ANONYMOUS_BYTES] = {
    .name = "host_meminfo_hugepages_anonumous_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Total size of non-file backed huge pages mapped into userspace page tables.",
  },
  [FAM_MEMINFO_HUGEPAGES_SHMEM_BYTES] = {
    .name = "host_meminfo_hugepages_shmem_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Total size of huge pages which are used for share memory allocations, or tmpfs.",
  },
  [FAM_MEMINFO_HUGEPAGES_SHMEM_PMDMAPPED_BYTES] = {
    .name = "host_meminfo_hugepages_shmem_pmdmapped_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Total size of shared memory mapped into user space with huge pages.",
  },
  [FAM_MEMINFO_HUGEPAGES_FILE_BYTES] = {
    .name = "host_meminfo_hugepages_file_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Total size of huge pages which are used for XXX." // FIXME
  },
  [FAM_MEMINFO_HUGEPAGES_FILE_PMDMAPPED_BYTES] = {
    .name = "host_meminfo_hugepages_file_pmdmapped_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Total size of file memory mapped into user space with huge pages", // FIXME
  },
  [FAM_MEMINFO_CMA_TOTAL_BYTES] = {
    .name = "host_meminfo_cma_total_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Total pages allocated by contiguous memory allocator.",
  },
  [FAM_MEMINFO_CMA_FREE_BYTES] = {
    .name = "host_meminfo_cma_free_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Free contiguous memory allocator pages.",
  },
  [FAM_MEMINFO_HUGEPAGES_TOTAL] = {
    .name = "host_meminfo_hugepages_total",
    .type = METRIC_TYPE_GAUGE,
    .help = "Number of hugepages being allocated by the kernel.",
  },
  [FAM_MEMINFO_HUGEPAGES_FREE] = {
    .name = "host_meminfo_hugepages_free",
    .type = METRIC_TYPE_GAUGE,
    .help = "The number of hugepages not being allocated by a process."
  },
  [FAM_MEMINFO_HUGEPAGES_RESERVED] = {
    .name = "host_meminfo_hugepages_reserved",
    .type = METRIC_TYPE_GAUGE,
    .help = "The number of hugepages for which a commitment to allocate from the pool has been made, but no allocation has yet been made.",
  },
  [FAM_MEMINFO_HUGEPAGES_SURPASSED] = {
    .name = "host_meminfo_hugepages_surpassed",
    .type = METRIC_TYPE_GAUGE,
    .help = "The number of hugepages in the pool above the value in vm.nr_hugepages.",
  },
  [FAM_MEMINFO_HUGEPAGE_SIZE_BYTES] = {
    .name = "host_meminfo_hugepage_size_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "The default size of a hugepage.",
  },
  [FAM_MEMINFO_HUGEPAGES_BYTES] = {
    .name = "host_meminfo_hugepages_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Total amount of memory consumed by huge pages of all sizes.",
  },
  [FAM_MEMINFO_DIRECTMAP_4K_BYTES] = {
    .name = "host_meminfo_directmap_4k_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "The amount of memory being mapped to standard 4k pages."
  },
  [FAM_MEMINFO_DIRECTMAP_2M_BYTES] = {
    .name = "host_meminfo_directmap_2M_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "The amount of memory being mapped to hugepages (usually 2MB in size)."
  },
  [FAM_MEMINFO_DIRECTMAP_2G_BYTES] = {
    .name = "host_meminfo_directmap_2G_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "The amount of memory being mapped to hugepages (usually 1GB in size)."
  },
};

