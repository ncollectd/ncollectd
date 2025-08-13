// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"

#include "zfs_fam.h"

metric_family_t fams_zfs[FAM_ZFS_MAX] = {
    [FAM_ZFS_ABD_STRUCT_SIZE_BYTES] = {
        .name = "system_zfs_abd_struct_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory occupied by all of the abd_t struct allocations in bytes."
    },
    [FAM_ZFS_ABD_LINEAR_COUNT] = {
        .name = "system_zfs_abd_linear_count",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of linear ABDs which are currently allocated.",
    },
    [FAM_ZFS_ABD_LINEAR_DATA_BYTES] = {
        .name = "system_zfs_abd_linear_data_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of data stored in all linear ABDs in bytes.",
    },
    [FAM_ZFS_ABD_SCATTER_COUNT] = {
        .name = "system_zfs_abd_scatter_count",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of scatter ABDs which are currently allocated.",
    },
    [FAM_ZFS_ABD_SCATTER_DATA_BYTES] = {
        .name = "system_zfs_abd_scatter_data_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of data stored in all scatter ABDs in bytes.",
    },
    [FAM_ZFS_ABD_SCATTER_CHUNK_WASTE_BYTES] = {
        .name = "system_zfs_abd_scatter_chunk_waste_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The amount of space wasted at the end of the last chunk "
                "across all scatter ABDs in bytes."
    },
    [FAM_ZFS_ABD_SCATTER_ORDER] = {
        .name = "system_zfs_abd_scatter_order",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of compound allocations of a given order. "
                "These allocations are spread over all currently allocated ABDs, "
                "and act as a measure of memory fragmentation.",
    },
    [FAM_ZFS_ABD_SCATTER_PAGE_MULTI_CHUNK] = {
        .name = "system_zfs_abd_scatter_page_multi_chunk",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of scatter ABDs which contain multiple chunks. "
                "ABDs are preferentially allocated from the minimum number of "
                "contiguous multi-page chunks, a single chunk is optimal.",
    },
    [FAM_ZFS_ABD_SCATTER_PAGE_MULTI_ZONE] = {
        .name = "system_zfs_abd_scatter_page_multi_zone",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of scatter ABDs which are split across memory zones. "
                "ABDs are preferentially allocated using pages from a single zone."
    },
    [FAM_ZFS_ABD_SCATTER_PAGE_ALLOC_RETRY] = {
        .name = "system_zfs_abd_scatter_page_alloc_retry",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of retries encountered when attempting to "
                "allocate the pages to populate the scatter ABD.",
    },
    [FAM_ZFS_ABD_SCATTER_SG_TABLE_RETRY] = {
        .name = "system_zfs_abd_scatter_sg_table_retry",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of retries encountered when attempting "
                "to allocate the sg table for an ABD.",
    },
    [FAM_ZFS_ARC_HITS] = {
        .name = "system_zfs_arc_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of request that were satisfied without I/O.",
    },
    [FAM_ZFS_ARC_IOHITS] = {
        .name = "system_zfs_arc_iohits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of requests for which I/O was already running.",
    },
    [FAM_ZFS_ARC_MISSES] = {
        .name = "system_zfs_arc_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of requests for which I/O has to be issued.",
    },
    [FAM_ZFS_ARC_DEMAND_DATA_HITS] = {
        .name = "system_zfs_arc_demand_data_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of request that were satisfied without I/O for demand data.",
    },
    [FAM_ZFS_ARC_DEMAND_DATA_IOHITS] = {
        .name = "system_zfs_arc_demand_data_iohits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of requests for which I/O was already running for demand data.",
    },
    [FAM_ZFS_ARC_DEMAND_DATA_MISSES] = {
        .name = "system_zfs_arc_demand_data_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of requests for which I/O has to be issued for demand data.",
    },
    [FAM_ZFS_ARC_DEMAND_METADATA_HITS] = {
        .name = "system_zfs_arc_demand_metadata_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of request that were satisfied without I/O for demand metadata,",
    },
    [FAM_ZFS_ARC_DEMAND_METADATA_IOHITS] = {
        .name = "system_zfs_arc_demand_metadata_iohits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of requests for which I/O was already running for demand metadata.",
    },
    [FAM_ZFS_ARC_DEMAND_METADATA_MISSES] = {
        .name = "system_zfs_arc_demand_metadata_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of requests for which I/O has to be issued for demand metadata.",
    },
    [FAM_ZFS_ARC_PREFETCH_DATA_HITS] = {
        .name = "system_zfs_arc_prefetch_data_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of request that were satisfied without I/O for prefetch data.",
    },
    [FAM_ZFS_ARC_PREFETCH_DATA_IOHITS] = {
        .name = "system_zfs_arc_prefetch_data_iohits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of requests for which I/O was already running for prefetch data.",
    },
    [FAM_ZFS_ARC_PREFETCH_DATA_MISSES] = {
        .name = "system_zfs_arc_prefetch_data_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of requests for which I/O has to be issued for prefetch data.",
    },
    [FAM_ZFS_ARC_PREFETCH_METADATA_HITS] = {
        .name = "system_zfs_arc_prefetch_metadata_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of request that were satisfied without I/O for prefetch metadata.",
    },
    [FAM_ZFS_ARC_PREFETCH_METADATA_IOHITS] = {
        .name = "system_zfs_arc_prefetch_metadata_iohits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of requests for which I/O was already running for prefetch metadata.",
    },
    [FAM_ZFS_ARC_PREFETCH_METADATA_MISSES] = {
        .name = "system_zfs_arc_prefetch_metadata_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of requests for which I/O has to be issued for prefetch metadata.",
    },
    [FAM_ZFS_ARC_MRU_HITS] = {
        .name = "system_zfs_arc_mru_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total cache hits in the “most recently used cache”, "
                "we move this to the mfu cache.",
    },
    [FAM_ZFS_ARC_MRU_GHOST_HITS] = {
        .name = "system_zfs_arc_mru_ghost_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total cache hits in the “most recently used ghost list” we had this item "
                "in the cache, but evicted it, maybe we should increase the mru cache size.",
    },
    [FAM_ZFS_ARC_MFU_HITS] = {
        .name = "system_zfs_arc_mfu_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total cache hits in the “most frequently used cache”, "
                "we move this to the beginning of the mfu cache."
    },
    [FAM_ZFS_ARC_MFU_GHOST_HITS] = {
        .name = "system_zfs_arc_mfu_ghost_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total cache hits in the “most frequently used ghost list” we had this item "
                "in the cache, but evicted it, maybe we should increase the mfu cache size.",
    },
    [FAM_ZFS_ARC_UNCACHED_HITS] = {
        .name = "system_zfs_arc_uncached_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total cache hits of uncacheable buffers.",
    },
    [FAM_ZFS_ARC_DELETED] = {
        .name = "system_zfs_arc_deleted",
        .type = METRIC_TYPE_COUNTER,
        .help = "Old data is evicted (deleted) from the cache.",
    },
    [FAM_ZFS_ARC_MUTEX_MISS] = {
        .name = "system_zfs_arc_mutex_miss",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of buffers that could not be evicted because the hash lock "
                "was held by another thread.",
    },
    [FAM_ZFS_ARC_ACCESS_SKIP] = {
        .name = "system_zfs_arc_access_skip",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of buffers skipped when updating the access state due to the "
                "header having already been released after acquiring the hash lock.",
    },
    [FAM_ZFS_ARC_EVICT_SKIP] = {
        .name = "system_zfs_arc_evict_skip",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of buffers skipped because they have I/O in progress, are "
                "indirect prefetch buffers that have not lived long enough, or are "
                "not from the spa we're trying to evict from.",
    },
    [FAM_ZFS_ARC_EVICT_NOT_ENOUGH] = {
        .name = "system_zfs_arc_evict_not_enough",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times arc_evict_state() was unable to evict enough "
                "buffers to reach its target amount.",
    },
    [FAM_ZFS_ARC_EVICT_L2_CACHED] = {
        .name = "system_zfs_arc_evict_l2_cached",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of evictions from the ARC, but its still cached in the L2.",
    },
    [FAM_ZFS_ARC_EVICT_L2_ELIGIBLE] = {
        .name = "system_zfs_arc_evict_l2_eligible",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of evictions from the ARC, but it’s not in the L2.",
    },
    [FAM_ZFS_ARC_EVICT_L2_INELIGIBLE] = {
        .name = "system_zfs_arc_evict_l2_ineligible",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of evictions from the ARC which cannot be stored in the L2.",
    },
    [FAM_ZFS_ARC_EVICT_L2_SKIP] = {
        .name = "system_zfs_arc_evict_l2_skip",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of evictions skipped due to L2 writes.",
    },
    [FAM_ZFS_ARC_HASH_ELEMENTS] = {
        .name = "system_zfs_arc_hash_elements",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number or elements in ARC hash.",
    },
    [FAM_ZFS_ARC_HASH_ELEMENTS_MAX] = {
        .name = "system_zfs_arc_hash_elements_max",
        .type = METRIC_TYPE_GAUGE,
        .help = "Max elements in ARC hash.",
    },
    [FAM_ZFS_ARC_HASH_COLLISIONS] = {
        .name = "system_zfs_arc_hash_collisions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of collisions in ARC hash.",
    },
    [FAM_ZFS_ARC_HASH_CHAINS] = {
        .name = "system_zfs_arc_hash_chains",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number or chains in ARC hash.",
    },
    [FAM_ZFS_ARC_HASH_CHAIN_MAX] = {
        .name = "system_zfs_arc_hash_chain_max",
        .type = METRIC_TYPE_GAUGE,
        .help = "Max chain size in ARC hash.",
    },
    [FAM_ZFS_ARC_META] = {
        .name = "system_zfs_arc_meta",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_P] = {
        .name = "system_zfs_arc_p",
        .type = METRIC_TYPE_GAUGE,
        .help = "The size of the MFU cache in bytes.",
    },
    [FAM_ZFS_ARC_PD] = {
        .name = "system_zfs_arc_pd",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_PM] = {
        .name = "system_zfs_arc_pm",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_C] = {
        .name = "system_zfs_arc_c",
        .type = METRIC_TYPE_GAUGE,
        .help = "This is the size the system thinks the ARC should have.",
    },
    [FAM_ZFS_ARC_C_MIN] = {
        .name = "system_zfs_arc_c_min",
        .type = METRIC_TYPE_GAUGE,
        .help = "The maximum target ARC size.",
    },
    [FAM_ZFS_ARC_C_MAX] = {
        .name = "system_zfs_arc_c_max",
        .type = METRIC_TYPE_GAUGE,
        .help = "The minimum target ARC size.",
    },
    [FAM_ZFS_ARC_SIZE_BYTES] = {
        .name = "system_zfs_arc_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current ARC size in bytes.",
    },
    [FAM_ZFS_ARC_COMPRESSED_SIZE_BYTES] = {
        .name = "system_zfs_arc_compressed_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Compressed size stored in the arc_buf_hdr_t's b_pabd in bytes.",
    },
    [FAM_ZFS_ARC_UNCOMPRESSED_SIZE_BYTES] = {
        .name = "system_zfs_arc_uncompressed_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Uncompressed size of the data stored in the arc_buf_hdr_t's b_pabd in bytes.",
    },
    [FAM_ZFS_ARC_OVERHEAD_SIZE_BYTES] = {
        .name = "system_zfs_arc_overhead_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes stored in all the arc_buf_t's.",
    },
    [FAM_ZFS_ARC_HDR_SIZE_BYTES] = {
        .name = "system_zfs_arc_hdr_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes consumed by internal ARC structures necessary "
                "for tracking purposes.",
    },
    [FAM_ZFS_ARC_DATA_SIZE_BYTES] = {
        .name = "system_zfs_arc_data_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes consumed by ARC buffers of type equal to ARC_BUFC_DATA.",
    },
    [FAM_ZFS_ARC_METADATA_SIZE_BYTES] = {
        .name = "system_zfs_arc_metadata_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes consumed by ARC buffers of type equal to ARC_BUFC_METADATA.",
    },
    [FAM_ZFS_ARC_DBUF_SIZE_BYTES] = {
        .name = "system_zfs_arc_dbuf_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes consumed by dmu_buf_impl_t objects.",
    },
    [FAM_ZFS_ARC_DNODE_SIZE_BYTES] = {
        .name = "system_zfs_arc_dnode_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes consumed by dnode_t objects."
    },
    [FAM_ZFS_ARC_BONUS_SIZE_BYTES] = {
        .name = "system_zfs_arc_bonus_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes consumed by bonus buffers.",
    },
    [FAM_ZFS_ARC_OTHER_SIZE_BYTES] = {
        .name = "system_zfs_arc_other_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes consumed by dmu_buf_impl_t objects, "
                "dnode_t objects and bonus buffers.",
    },
    [FAM_ZFS_ARC_ANON_SIZE_BYTES] = {
        .name = "system_zfs_arc_anon_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total number of bytes consumed by ARC buffers residing in the arc_anon state.",
    },
    [FAM_ZFS_ARC_ANON_DATA_SIZE_BYTES] = {
        .name = "system_zfs_arc_anon_data_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_ANON_METADATA_SIZE_BYTES] = {
        .name = "system_zfs_arc_anon_metadata_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_ANON_EVICTABLE_DATA_BYTES] = {
        .name = "system_zfs_arc_anon_evictable_data_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes consumed by ARC buffers that meet the following criteria: "
                "backing buffers of type ARC_BUFC_DATA, residing in the arc_anon state, "
                "and are eligible for eviction.",
    },
    [FAM_ZFS_ARC_ANON_EVICTABLE_METADATA_BYTES] = {
        .name = "system_zfs_arc_anon_evictable_metadata_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes consumed by ARC buffers that meet the following criteria: "
                "backing buffers of type ARC_BUFC_METADATA, residing in the arc_anon state, "
                "and are eligible for eviction.",
    },
    [FAM_ZFS_ARC_MRU_SIZE_BYTES] = {
        .name = "system_zfs_arc_mru_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total number of bytes consumed by ARC buffers residing in the arc_mru state.",
    },
    [FAM_ZFS_ARC_MRU_DATA_BYTES] = {
        .name = "system_zfs_arc_mru_data_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_MRU_METADATA_BYTES] = {
        .name = "system_zfs_arc_mru_metadata_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_MRU_EVICTABLE_DATA_BYTES] = {
        .name = "system_zfs_arc_mru_evictable_data_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes consumed by ARC buffers that meet the following criteria: "
                "backing buffers of type ARC_BUFC_DATA, residing in the arc_mru state, "
                "and are eligible for eviction.",
    },
    [FAM_ZFS_ARC_MRU_EVICTABLE_METADATA_BYTES] = {
        .name = "system_zfs_arc_mru_evictable_metadata_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes consumed by ARC buffers that meet the following criteria: "
                "backing buffers of type ARC_BUFC_METADATA, residing in the arc_mru state, "
                "and are eligible for eviction.",
    },
    [FAM_ZFS_ARC_MRU_GHOST_SIZE_BYTES] = {
        .name = "system_zfs_arc_mru_ghost_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total number of bytes that *would have been* consumed by ARC buffers "
                "in the arc_mru_ghost state.",
    },
    [FAM_ZFS_ARC_MRU_GHOST_DATA_BYTES] = {
        .name = "system_zfs_arc_mru_ghost_data_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_MRU_GHOST_METADATA_BYTES] = {
        .name = "system_zfs_arc_mru_ghost_metadata_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_MRU_GHOST_EVICTABLE_DATA_BYTES] = {
        .name = "system_zfs_arc_mru_ghost_evictable_data_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes that *would have been* consumed by ARC buffers that "
                "are eligible for eviction, of type ARC_BUFC_DATA, "
                "and linked off the arc_mru_ghost state.",
    },
    [FAM_ZFS_ARC_MRU_GHOST_EVICTABLE_METADATA_BYTES] = {
        .name = "system_zfs_arc_mru_ghost_evictable_metadata_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes that *would have been* consumed by ARC buffers that "
                "are eligible for eviction, of type ARC_BUFC_METADATA, "
                "and linked off the arc_mru_ghost state.",
    },
    [FAM_ZFS_ARC_MFU_SIZE_BYTES] = {
        .name = "system_zfs_arc_mfu_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total number of bytes consumed by ARC buffers residing in the arc_mfu state.",
    },
    [FAM_ZFS_ARC_MFU_DATA_BYTES] = {
        .name = "system_zfs_arc_mfu_data_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_MFU_METADATA_BYTES] = {
        .name = "system_zfs_arc_mfu_metadata_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_MFU_EVICTABLE_DATA_BYTES] = {
        .name = "system_zfs_arc_mfu_evictable_data_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes consumed by ARC buffers that are eligible for eviction, "
                "of type ARC_BUFC_DATA, and reside in the arc_mfu state.",
    },
    [FAM_ZFS_ARC_MFU_EVICTABLE_METADATA_BYTES] = {
        .name = "system_zfs_arc_mfu_evictable_metadata_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes consumed by ARC buffers that are eligible for eviction, "
                "of type ARC_BUFC_METADATA, and reside in the arc_mfu state.",
    },
    [FAM_ZFS_ARC_MFU_GHOST_SIZE_BYTES] = {
        .name = "system_zfs_arc_mfu_ghost_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total number of bytes that *would have been* consumed by ARC buffers "
                "in the arc_mfu_ghost state.",
    },
    [FAM_ZFS_ARC_MFU_GHOST_DATA_BYTES] = {
        .name = "system_zfs_arc_mfu_ghost_data_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_MFU_GHOST_METADATA_BYTES] = {
        .name = "system_zfs_arc_mfu_ghost_metadata_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_MFU_GHOST_EVICTABLE_DATA_BYTES] = {
        .name = "system_zfs_arc_mfu_ghost_evictable_data_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes that *would have been* consumed by ARC buffers that "
                "are eligible for eviction, of type ARC_BUFC_DATA, "
                "and linked off the arc_mfu_ghost state.",
    },
    [FAM_ZFS_ARC_MFU_GHOST_EVICTABLE_METADATA_BYTES] = {
        .name = "system_zfs_arc_mfu_ghost_evictable_metadata_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes that *would have been* consumed by ARC buffers that "
                "are eligible for eviction, of type ARC_BUFC_METADATA, "
                "and linked off the arc_mru_ghost state.",
    },
    [FAM_ZFS_ARC_UNCACHED_SIZE_BYTES] = {
        .name = "system_zfs_arc_uncached_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total number of bytes that are going to be evicted from ARC "
                "due to ARC_FLAG_UNCACHED being set.",
    },
    [FAM_ZFS_ARC_UNCACHED_DATA_BYTES] = {
        .name = "system_zfs_arc_uncached_data_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_UNCACHED_METADATA_BYTES] = {
        .name = "system_zfs_arc_uncached_metadata_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_UNCACHED_EVICTABLE_DATA_BYTES] = {
        .name = "system_zfs_arc_uncached_evictable_data_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of data bytes that are going to be evicted from ARC "
                "due to ARC_FLAG_UNCACHED being set.",
    },
    [FAM_ZFS_ARC_UNCACHED_EVICTABLE_METADATA_BYTES] = {
        .name = "system_zfs_arc_uncached_evictable_metadata_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of metadata bytes that that are going to be evicted from ARC "
                "due to ARC_FLAG_UNCACHED being set.",
    },
    [FAM_ZFS_ARC_L2_HITS] = {
        .name = "system_zfs_arc_l2_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total hits to the L2 cache. (It was not in the ARC, but in the L2 cache).",
    },
    [FAM_ZFS_ARC_L2_MISSES] = {
        .name = "system_zfs_arc_l2_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total misses the L2 cache. (It was not in the ARC, and not in the L2 cache).",
    },
    [FAM_ZFS_ARC_L2_PREFETCH_ASIZE_BYTES] = {
        .name = "system_zfs_arc_l2_prefetch_asize_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Aligned size in bytes of L2ARC buffers that were cached "
                "while they had the prefetch flag set in ARC.",
    },
    [FAM_ZFS_ARC_L2_MRU_ASIZE_BYTES] = {
        .name = "system_zfs_arc_l2_mru_asize_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Aligned size in bytes of L2ARC buffers that were cached "
                "while they had the mru flag set in ARC.",
    },
    [FAM_ZFS_ARC_L2_MFU_ASIZE_BYTES] = {
        .name = "system_zfs_arc_l2_mfu_asize_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Aligned size in bytes of L2ARC buffers that were cached "
                "while they had the mfu flag set in ARC.",
    },
    [FAM_ZFS_ARC_L2_BUFC_DATA_ASIZE_BYTES] = {
        .name = "system_arc_l2_bufc_data_asize_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Allocated size in bytes of L2ARC cached buffers data.",
    },
    [FAM_ZFS_ARC_L2_BUFC_METADATA_ASIZE_BYTES] = {
        .name = "system_arc_l2_bufc_metadata_asize_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Allocated size in bytes of L2ARC cached buffers metadata.",
    },
    [FAM_ZFS_ARC_L2_FEEDS] = {
        .name = "system_zfs_arc_l2_feeds",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_L2_RW_CLASH] = {
        .name = "system_zfs_arc_l2_rw_clash",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_L2_READ_BYTES] = {
        .name = "system_zfs_arc_l2_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_L2_WRITE_BYTES] = {
        .name = "system_zfs_arc_l2_write_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_L2_WRITES_SENT] = {
        .name = "system_zfs_arc_l2_writes_sent",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_L2_WRITES_DONE] = {
        .name = "system_zfs_arc_l2_writes_done",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_L2_WRITES_ERROR] = {
        .name = "system_zfs_arc_l2_writes_error",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_L2_WRITES_LOCK_RETRY] = {
        .name = "system_zfs_arc_l2_writes_lock_retry",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_L2_EVICT_LOCK_RETRY] = {
        .name = "system_zfs_arc_l2_evict_lock_retry",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_L2_EVICT_READING] = {
        .name = "system_zfs_arc_l2_evict_reading",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_L2_EVICT_L1CACHED] = {
        .name = "system_zfs_arc_l2_evict_l1cached",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_L2_FREE_ON_WRITE] = {
        .name = "system_zfs_arc_l2_free_on_write",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_L2_CDATA_FREE_ON_WRITE] = {
        .name = "system_zfs_arc_l2_cdata_free_on_write",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_L2_ABORT_LOWMEM] = {
        .name = "system_zfs_arc_l2_abort_lowmem",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_L2_CKSUM_BAD] = {
        .name = "system_zfs_arc_l2_cksum_bad",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_L2_IO_ERROR] = {
        .name = "system_zfs_arc_l2_io_error",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_L2_SIZE_BYTES] = {
        .name = "system_zfs_arc_l2_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Size of the data in bytes in the L2ARC.",
    },
    [FAM_ZFS_ARC_L2_ASIZE_BYTES] = {
        .name = "system_zfs_arc_l2_asize_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Aligned size of the data in bytes in the L2ARC.",
    },
    [FAM_ZFS_ARC_L2_HDR_SIZE_BYTES] = {
        .name = "system_zfs_arc_l2_hdr_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Size in bytes of the metadata in the ARC used to manage the L2 cache.",
    },
    [FAM_ZFS_ARC_L2_LOG_BLK_WRITES] = {
        .name = "system_zfs_arc_l2_log_blk_writes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of L2ARC log blocks written.",
    },
    [FAM_ZFS_ARC_L2_LOG_BLK_AVG_ASIZE_BYTES] = {
        .name = "system_zfs_arc_l2_log_blk_avg_asize_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Moving average of the aligned size of the L2ARC log blocks, in bytes.",
    },
    [FAM_ZFS_ARC_L2_LOG_BLK_ASIZE] = {
        .name = "system_zfs_arc_l2_log_blk_asize",
        .type = METRIC_TYPE_GAUGE,
        .help = "Aligned size of L2ARC log blocks on L2ARC devices.",
    },
    [FAM_ZFS_ARC_L2_LOG_BLK_COUNT] = {
        .name = "system_zfs_arc_l2_log_blk_count",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of L2ARC log blocks present on L2ARC devices.",
    },
    [FAM_ZFS_ARC_L2_DATA_TO_META_RATIO] = {
        .name = "system_zfs_arc_l2_data_to_meta_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "Moving average of the aligned size of L2ARC restored data, in bytes, "
                "to the aligned size of their metadata in L2ARC, in bytes.",
    },
    [FAM_ZFS_ARC_L2_REBUILD_SUCCESS] = {
        .name = "system_zfs_arc_l2_rebuild_success",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the L2ARC rebuild was successful for an L2ARC device.",
    },
    [FAM_ZFS_ARC_L2_REBUILD_UNSUPPORTED] = {
        .name = "system_zfs_arc_l2_rebuild_unsupported",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the L2ARC rebuild failed because the device header "
                "was in an unsupported format or corrupted.",
    },
    [FAM_ZFS_ARC_L2_REBUILD_IO_ERRORS] = {
        .name = "system_zfs_arc_l2_rebuild_io_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the L2ARC rebuild failed because of IO errors "
                "while reading a log block.",
    },
    [FAM_ZFS_ARC_L2_REBUILD_DH_ERRORS] = {
        .name = "system_zfs_arc_l2_rebuild_dh_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the L2ARC rebuild failed because of IO errors "
                "when reading the device header.",
    },
    [FAM_ZFS_ARC_L2_REBUILD_CKSUM_LB_ERRORS] = {
        .name = "system_zfs_arc_l2_rebuild_cksum_lb_errors",
        .help = "Number of L2ARC log blocks which failed to be restored due to checksum errors.",
    },
    [FAM_ZFS_ARC_L2_REBUILD_LOWMEM] = {
        .name = "system_zfs_arc_l2_rebuild_lowmem",
        .help = "Number of times the L2ARC rebuild was aborted due to low system memory.",
    },
    [FAM_ZFS_ARC_L2_REBUILD_SIZE] = {
        .name = "system_zfs_arc_l2_rebuild_size",
        .help = "Logical size of L2ARC restored data, in bytes.",
    },
    [FAM_ZFS_ARC_L2_REBUILD_ASIZE] = {
        .name = "system_zfs_arc_l2_rebuild_asize",
        .help = "Aligned size of L2ARC restored data, in bytes.",
    },
    [FAM_ZFS_ARC_L2_REBUILD_BUFS] = {
        .name = "system_zfs_arc_l2_rebuild_bufs",
        .help = "Number of L2ARC log entries (buffers) that were successfully restored in ARC.",
    },
    [FAM_ZFS_ARC_L2_REBUILD_BUFS_PRECACHED] = {
        .name = "system_zfs_arc_l2_rebuild_bufs_precached",
        .help = "Number of L2ARC log entries (buffers) already cached in ARC. These were not restored again.",
    },
    [FAM_ZFS_ARC_L2_REBUILD_LOG_BLKS] = {
        .name = "system_zfs_arc_l2_rebuild_log_blks",
        .help = "Number of L2ARC log blocks that were restored successfully. "
                "Each log block may hold up to L2ARC_LOG_BLK_MAX_ENTRIES buffers.",
    },

// delete ¿?
    [FAM_ZFS_ARC_L2_COMPRESS_SUCCESSES] = {
        .name = "system_zfs_arc_l2_compress_successes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_L2_COMPRESS_ZEROS] = {
        .name = "system_zfs_arc_l2_compress_zeros",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_L2_COMPRESS_FAILURES] = {
        .name = "system_zfs_arc_l2_compress_failures",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_DUPLICATE_BUFFERS] = {
        .name = "system_zfs_arc_duplicate_buffers",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_DUPLICATE_BUFFERS_SIZE] = {
        .name = "system_zfs_arc_duplicate_buffers_size",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_DUPLICATE_READS] = {
        .name = "system_zfs_arc_duplicate_reads",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
// END

    [FAM_ZFS_ARC_MEMORY_THROTTLE_COUNT] = {
        .name = "system_zfs_arc_memory_throttle_count",
        .type = METRIC_TYPE_COUNTER,
        .help = "The  number of times that ZFS had to limit the ARC growth.",
    },
    [FAM_ZFS_ARC_MEMORY_DIRECT_COUNT] = {
        .name = "system_zfs_arc_memory_direct_count",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_MEMORY_INDIRECT_COUNT] = {
        .name = "system_zfs_arc_memory_indirect_count",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_MEMORY_ALL_BYTES] = {
        .name = "system_zfs_arc_memory_all_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_MEMORY_FREE_BYTES] = {
        .name = "system_zfs_arc_memory_free_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_MEMORY_AVAILABLE_BYTES] = {
        .name = "system_zfs_arc_memory_available_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },

    [FAM_ZFS_ARC_NO_GROW] = {
        .name = "system_zfs_arc_no_grow",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_TEMPRESERVE] = {
        .name = "system_zfs_arc_tempreserve",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_LOANED_BYTES] = {
        .name = "system_zfs_arc_loaned_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_PRUNE] = {
        .name = "system_zfs_arc_prune",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_META_USED] = {
        .name = "system_zfs_arc_meta_used",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },


    [FAM_ZFS_ARC_META_LIMIT] = {
        .name = "system_zfs_arc_meta_limit",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_META_MAX] = {
        .name = "system_zfs_arc_meta_max",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_META_MIN] = {
        .name = "system_zfs_arc_meta_min",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },


    [FAM_ZFS_ARC_DNODE_LIMIT] = {
        .name = "system_zfs_arc_dnode_limit",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_ASYNC_UPGRADE_SYNC] = {
        .name = "system_zfs_arc_async_upgrade_sync",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total sync reads that needs to wait for an in-flight async read.",
    },
    [FAM_ZFS_ARC_PREDICTIVE_PREFETCH] = {
        .name = "system_zfs_arc_predictive_prefetch",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_DEMAND_HIT_PREDICTIVE_PREFETCH] = {
        .name = "system_zfs_arc_demand_hit_predictive_prefetch",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_DEMAND_IOHIT_PREDICTIVE_PREFETCH] = {
        .name = "system_zfs_arc_demand_iohit_predictive_prefetch",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_PRESCIENT_PREFETCH] = {
        .name = "system_zfs_arc_prescient_prefetch",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_DEMAND_HIT_PRESCIENT_PREFETCH] = {
        .name = "system_zfs_arc_demand_hit_prescient_prefetch",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_DEMAND_IOHIT_PRESCIENT_PREFETCH] = {
        .name = "system_zfs_arc_demand_iohit_prescient_prefetch",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_RAW_SIZE] = {
        .name = "system_zfs_arc_raw_size",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_NEED_FREE] = {
        .name = "system_zfs_arc_need_free",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_SYS_FREE] = {
        .name = "system_zfs_arc_sys_free",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ARC_CACHED_ONLY_IN_PROGRESS] = {
        .name = "system_zfs_arc_cached_only_in_progress",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ARC_ABD_CHUNK_WASTE_SIZE] = {
        .name = "system_zfs_arc_abd_chunk_waste_size",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },


    [FAM_ZFS_DBUF_CACHE_COUNT] = {
        .name = "system_zfs_dbuf_cache_count",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_DBUF_CACHE_SIZE] = {
        .name = "system_zfs_dbuf_cache_size",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_DBUF_CACHE_SIZE_MAX] = {
        .name = "system_zfs_dbuf_cache_size_max",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_DBUF_CACHE_MAX_BYTES] = {
        .name = "system_zfs_dbuf_cache_max_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_DBUF_CACHE_LOWATER_BYTES] = {
        .name = "system_zfs_dbuf_cache_lowater_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_DBUF_CACHE_HIWATER_BYTES] = {
        .name = "system_zfs_dbuf_cache_hiwater_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_DBUF_CACHE_TOTAL_EVICTS] = {
        .name = "system_zfs_dbuf_cache_total_evicts",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
// Total number of dbuf cache evictions that have occurred.
    },
    [FAM_ZFS_DBUF_CACHE_LEVEL] = {
        .name = "system_zfs_dbuf_cache_level",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
// The distribution of dbuf levels in the dbuf cache
    },
    [FAM_ZFS_DBUF_CACHE_LEVEL_BYTES] = {
        .name = "system_zfs_dbuf_cache_level_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
// The total size of all dbufs at each level.
    },
    [FAM_ZFS_DBUF_HASH_HITS] = {
        .name = "system_zfs_dbuf_hash_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_DBUF_HASH_MISSES] = {
        .name = "system_zfs_dbuf_hash_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_DBUF_HASH_COLLISIONS] = {
        .name = "system_zfs_dbuf_hash_collisions",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_DBUF_HASH_ELEMENTS] = {
        .name = "system_zfs_dbuf_hash_elements",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_DBUF_HASH_ELEMENTS_MAX] = {
        .name = "system_zfs_dbuf_hash_elements_max",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_DBUF_HASH_CHAINS] = {
        .name = "system_zfs_dbuf_hash_chains",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
// Number of sublists containing more than one dbuf in the dbuf hash table. Keep track of the longest hash chain.
    },
    [FAM_ZFS_DBUF_HASH_CHAIN_MAX] = {
        .name = "system_zfs_dbuf_hash_chain_max",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_DBUF_HASH_INSERT_RACE] = {
        .name = "system_zfs_dbuf_hash_insert_race",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
// Number of times a dbuf_create() discovers that a dbuf was already created and in the dbuf hash table.
    },


    [FAM_ZFS_DBUF_HASH_DBUF_LEVEL] = {
        .name = "system_zfs_dbuf_hash_dbuf_level",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_DBUF_HASH_DBUF_LEVEL_BYTES] = {
        .name = "system_zfs_dbuf_hash_dbuf_level_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },

    [FAM_ZFS_DBUF_HASH_TABLE_COUNT] = {
        .name = "system_zfs_dbuf_hash_table_count",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
// Number of entries in the hash table dbuf and mutex arrays.
    },
    [FAM_ZFS_DBUF_HASH_MUTEX_COUNT] = {
        .name = "system_zfs_dbuf_hash_mutex_count",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
// Number of entries in the hash table dbuf and mutex arrays.
    },

//  Statistics about the size of the metadata dbuf cache.

    [FAM_ZFS_DBUF_METADATA_CACHE_COUNT] = {
        .name = "system_zfs_dbuf_metadata_cache_count",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_DBUF_METADATA_CACHE_SIZE_BYTES] = {
        .name = "system_zfs_dbuf_metadata_cache_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_DBUF_METADATA_CACHE_SIZE_BYTES_MAX] = {
        .name = "system_zfs_dbuf_metadata_cache_size_bytes_max",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_DBUF_METADATA_CACHE_OVERFLOW] = {
        .name = "system_zfs_dbuf_metadata_cache_overflow",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },


    [FAM_ZFS_DMU_TX_ASSIGNED] = {
        .name = "system_zfs_dmu_tx_assigned",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_DMU_TX_DELAY] = {
        .name = "system_zfs_dmu_tx_delay",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_DMU_TX_ERROR] = {
        .name = "system_zfs_dmu_tx_error",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_DMU_TX_SUSPENDED] = {
        .name = "system_zfs_dmu_tx_suspended",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_DMU_TX_GROUP] = {
        .name = "system_zfs_dmu_tx_group",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_DMU_TX_MEMORY_RESERVE] = {
        .name = "system_zfs_dmu_tx_memory_reserve",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
// dmu_tx_memory_reserve counts when memory footprint of the txg exceeds the ARC size
    },
    [FAM_ZFS_DMU_TX_MEMORY_RECLAIM] = {
        .name = "system_zfs_dmu_tx_memory_reclaim",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
// dmu_tx_memory_reclaim counts when memory is low and throttling activity
    },
    [FAM_ZFS_DMU_TX_DIRTY_THROTTLE] = {
        .name = "system_zfs_dmu_tx_dirty_throttle",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
// dmu_tx_dirty_throttle counts when writes are throttled due to the amount of dirty data growing too large
    },
    [FAM_ZFS_DMU_TX_DIRTY_DELAY] = {
        .name = "system_zfs_dmu_tx_dirty_delay",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_DMU_TX_DIRTY_OVER_MAX] = {
        .name = "system_zfs_dmu_tx_dirty_over_max",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_DMU_TX_QUOTA] = {
        .name = "system_zfs_dmu_tx_quota",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },


    [FAM_ZFS_DNODE_HOLD_DBUF_HOLD] = {
        .name = "system_zfs_dnode_hold_dbuf_hold",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of failed attempts to hold a meta dnode dbuf.",
    },
    [FAM_ZFS_DNODE_HOLD_DBUF_READ] = {
        .name = "system_zfs_dnode_hold_dbuf_read",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of failed attempts to read a meta dnode dbuf.",
    },
    [FAM_ZFS_DNODE_HOLD_ALLOC_HITS] = {
        .name = "system_zfs_dnode_hold_alloc_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times dnode_hold(..., DNODE_MUST_BE_ALLOCATED) was able "
                "to hold the requested object number which was allocated.",
    },
    [FAM_ZFS_DNODE_HOLD_ALLOC_MISSES] = {
        .name = "system_zfs_dnode_hold_alloc_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times dnode_hold(..., DNODE_MUST_BE_ALLOCATED) was not able "
                "to hold the request object number because it was not allocated.",
    },
    [FAM_ZFS_DNODE_HOLD_ALLOC_INTERIOR] = {
        .name = "system_zfs_dnode_hold_alloc_interior",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times dnode_hold(..., DNODE_MUST_BE_ALLOCATED) was not "
                "able to hold the request object number because the object number "
                "refers to an interior large dnode slot.",
    },
    [FAM_ZFS_DNODE_HOLD_ALLOC_LOCK_RETRY] = {
        .name = "system_zfs_dnode_hold_alloc_lock_retry",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times dnode_hold(..., DNODE_MUST_BE_ALLOCATED) needed "
                "to retry acquiring slot zrl locks due to contention.",
    },
    [FAM_ZFS_DNODE_HOLD_ALLOC_LOCK_MISSES] = {
        .name = "system_zfs_dnode_hold_alloc_lock_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times dnode_hold(..., DNODE_MUST_BE_ALLOCATED) did not "
                "need to create the dnode because another thread did so after "
                "dropping the read lock but before acquiring the write lock.",
    },
    [FAM_ZFS_DNODE_HOLD_ALLOC_TYPE_NONE] = {
        .name = "system_zfs_dnode_hold_alloc_type_none",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times dnode_hold(..., DNODE_MUST_BE_ALLOCATED) found "
                "a free dnode instantiated by dnode_create() but not yet allocated "
                "by dnode_allocate().",
    },
    [FAM_ZFS_DNODE_HOLD_FREE_HITS] = {
        .name = "system_zfs_dnode_hold_free_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times dnode_hold(..., DNODE_MUST_BE_FREE) was able "
                "to hold the requested range of free dnode slots.",
    },
    [FAM_ZFS_DNODE_HOLD_FREE_MISSES] = {
        .name = "system_zfs_dnode_hold_free_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times dnode_hold(..., DNODE_MUST_BE_FREE) was not "
                "able to hold the requested range of free dnode slots because "
                "at least one slot was allocated.",
    },
    [FAM_ZFS_DNODE_HOLD_FREE_LOCK_MISSES] = {
        .name = "system_zfs_dnode_hold_free_lock_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times dnode_hold(..., DNODE_MUST_BE_FREE) was not "
                "able to hold the requested range of free dnode slots because "
                "after acquiring the zrl lock at least one slot was allocated.",
    },
    [FAM_ZFS_DNODE_HOLD_FREE_LOCK_RETRY] = {
        .name = "system_zfs_dnode_hold_free_lock_retry",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times dnode_hold(..., DNODE_MUST_BE_FREE) needed "
                "to retry acquiring slot zrl locks due to contention.",
    },
    [FAM_ZFS_DNODE_HOLD_FREE_OVERFLOW] = {
        .name = "system_zfs_dnode_hold_free_overflow",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times dnode_hold(..., DNODE_MUST_BE_FREE) requested "
                "a range of dnode slots which would overflow the dnode_phys_t.",
    },
    [FAM_ZFS_DNODE_HOLD_FREE_REFCOUNT] = {
        .name = "system_zfs_dnode_hold_free_refcount",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times dnode_hold(..., DNODE_MUST_BE_FREE) requested "
                "a range of dnode slots which were held by another thread."
    },
    [FAM_ZFS_DNODE_HOLD_FREE_TXG] = {
        .name = "system_zfs_dnode_hold_free_txg",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_DNODE_ALLOCATE] = {
        .name = "system_zfs_dnode_allocate",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of new dnodes allocated by dnode_allocate().",
    },
    [FAM_ZFS_DNODE_REALLOCATE] = {
        .name = "system_zfs_dnode_reallocate",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of dnodes re-allocated by dnode_reallocate().",
    },
    [FAM_ZFS_DNODE_BUF_EVICT] = {
        .name = "system_zfs_dnode_buf_evict",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of meta dnode dbufs evicted.",
    },
    [FAM_ZFS_DNODE_ALLOC_NEXT_CHUNK] = {
        .name = "system_zfs_dnode_alloc_next_chunk",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times dmu_object_alloc*() reached the end of the existing "
                "object ID chunk and advanced to a new one.",
    },
    [FAM_ZFS_DNODE_ALLOC_RACE] = {
        .name = "system_zfs_dnode_alloc_race",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times multiple threads attempted to allocate a dnode "
                "from the same block of free dnodes.",
    },
    [FAM_ZFS_DNODE_ALLOC_NEXT_BLOCK] = {
        .name = "system_zfs_dnode_alloc_next_block",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times dmu_object_alloc*() was forced to advance to the "
                "next meta dnode dbuf due to an error from  dmu_object_next().",
    },
    /* Statistics for tracking dnodes which have been moved. */
    [FAM_ZFS_DNODE_MOVE_INVALID] = {
        .name = "system_zfs_dnode_move_invalid",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_DNODE_MOVE_RECHECK1] = {
        .name = "system_zfs_dnode_move_recheck1",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_DNODE_MOVE_RECHECK2] = {
        .name = "system_zfs_dnode_move_recheck2",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_DNODE_MOVE_SPECIAL] = {
        .name = "system_zfs_dnode_move_special",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_DNODE_MOVE_HANDLE] = {
        .name = "system_zfs_dnode_move_handle",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_DNODE_MOVE_RWLOCK] = {
        .name = "system_zfs_dnode_move_rwlock",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_DNODE_MOVE_ACTIVE] = {
        .name = "system_zfs_dnode_move_active",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },


#if 0
erpt_dropped;       /* num erpts dropped on post */
erpt_set_failed;    /* num erpt set failures */
fmri_set_failed;    /* num fmri set failures */
payload_set_failed; /* num payload set failures */
erpt_duplicates;    /* num duplicate erpts */
#endif
    [FAM_ZFS_FM_ERPT_DROPPED] = {
        .name = "system_zfs_fm_erpt_dropped",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
// fm_erpt-dropped counts when an error report cannot be created (eg available memory is too low)
    },
    [FAM_ZFS_FM_ERPT_SET_FAILED] = {
        .name = "system_zfs_fm_erpt_set_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_FM_FMRI_SET_FAILED] = {
        .name = "system_zfs_fm_fmri_set_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_FM_PAYLOAD_SET_FAILED] = {
        .name = "system_zfs_fm_payload_set_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_FM_ERPT_DUPLICATES] = {
        .name = "system_zfs_fm_erpt_duplicates",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },



    [FAM_ZFS_VDEV_CACHE_DELEGATIONS] = {
        .name = "system_zfs_vdev_cache_delegations",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_VDEV_CACHE_HITS] = {
        .name = "system_zfs_vdev_cache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
// vdev_cache_stats_hits Hits to the vdev (device level) cache.
    },

    [FAM_ZFS_VDEV_CACHE_MISSES] = {
        .name = "system_zfs_vdev_cache_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
// vdev_cache_stats_misses Misses to the vdev (device level) cache.
    },
    [FAM_ZFS_VDEV_MIRROR_ROTATING_LINEAR] = {
        .name = "system_zfs_vdev_mirror_rotating_linear",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
// New I/O follows directly the last I/O
    },
    [FAM_ZFS_VDEV_MIRROR_ROTATING_OFFSET] = {
        .name = "system_zfs_vdev_mirror_rotating_offset",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
// New I/O is within zfs_vdev_mirror_rotating_seek_offset of the last
    },
    [FAM_ZFS_VDEV_MIRROR_ROTATING_SEEK] = {
        .name = "system_zfs_vdev_mirror_rotating_seek",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
// New I/O requires random seek
    },
    [FAM_ZFS_VDEV_MIRROR_NON_ROTATING_LINEAR] = {
        .name = "system_zfs_vdev_mirror_non_rotating_linear",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
// New I/O follows directly the last I/O  (nonrot)
    },
    [FAM_ZFS_VDEV_MIRROR_NON_ROTATING_SEEK] = {
        .name = "system_zfs_vdev_mirror_non_rotating_seek",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
// New I/O requires random seek (nonrot)
    },
    [FAM_ZFS_VDEV_MIRROR_PREFERRED_FOUND] = {
        .name = "system_zfs_vdev_mirror_preferred_found",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
// Preferred child vdev found
    },
    [FAM_ZFS_VDEV_MIRROR_PREFERRED_NOT_FOUND] = {
        .name = "system_zfs_vdev_mirror_preferred_not_found",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
// Preferred child vdev not found or equal load
    },


    [FAM_ZFS_XUIO_ONLOAN_READ_BUF] = {
        .name = "system_zfs_xuio_onloan_read_buf",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_XUIO_ONLOAN_WRITE_BUF] = {
        .name = "system_zfs_xuio_onloan_write_buf",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_XUIO_READ_BUF_COPIED] = {
        .name = "system_zfs_xuio_read_buf_copied",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_XUIO_READ_BUF_NOCOPY] = {
        .name = "system_zfs_xuio_read_buf_nocopy",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_XUIO_WRITE_BUF_COPIED] = {
        .name = "system_zfs_xuio_write_buf_copied",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_XUIO_WRITE_BUF_NOCOPY] = {
        .name = "system_zfs_xuio_write_buf_nocopy",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },


    [FAM_ZFS_ZFETCH_HITS] = {
        .name = "system_zfs_zfetch_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
//zfetchstats_hits Counts the number of cache hits, to items which are in the cache because of the prefetcher.
    },
    [FAM_ZFS_ZFETCH_MISSES] = {
        .name = "system_zfs_zfetch_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
//zfetchstats_misses Counts the number of prefetch cache misses.
    },
    [FAM_ZFS_ZFETCH_COLINEAR_HITS] = {
        .name = "system_zfs_zfetch_colinear_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
// zfetchstats_colinear_hits Counts the number of cache hits, to items which are in the cache because of the prefetcher (prefetched linear reads)
    },
    [FAM_ZFS_ZFETCH_COLINEAR_MISSES] = {
        .name = "system_zfs_zfetch_colinear_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ZFETCH_STRIDE_HITS] = {
        .name = "system_zfs_zfetch_stride_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
// zfetchstats_stride_hits Counts the number of cache hits, to items which are in the cache because of the prefetcher (prefetched stride reads)
    },
    [FAM_ZFS_ZFETCH_STRIDE_MISSES] = {
        .name = "system_zfs_zfetch_stride_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ZFETCH_RECLAIM_SUCCESSES] = {
        .name = "system_zfs_zfetch_reclaim_successes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ZFETCH_RECLAIM_FAILURES] = {
        .name = "system_zfs_zfetch_reclaim_failures",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ZFETCH_STREAMS_RESETS] = {
        .name = "system_zfs_zfetch_streams_resets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ZFETCH_STREAMS_NORESETS] = {
        .name = "system_zfs_zfetch_streams_noresets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ZFETCH_BOGUS_STREAMS] = {
        .name = "system_zfs_zfetch_bogus_streams",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },


    [FAM_ZFS_ZIL_COMMIT] = {
        .name = "system_zfs_zil_commit",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of ZFS transactions committed to a ZIL."
// Number of times a ZIL commit (e.g. fsync) has been requested.
    },
    [FAM_ZFS_ZIL_COMMIT_WRITER_COUNT] = {
        .name = "system_zfs_zil_commit_writer_count",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
//  Number of times the ZIL has been flushed to stable storage.
    [FAM_ZFS_ZIL_ITX_COUNT] = {
        .name = "system_zfs_zil_itx_count",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
// Number of transactions (reads, writes, renames, etc.)  that have been committed.

    [FAM_ZFS_ZIL_ITX_INDIRECT_COUNT] = {
        .name = "system_zfs_zil_itx_indirect_count",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
// See the documentation for itx_wr_state_t above.
    [FAM_ZFS_ZIL_ITX_INDIRECT_BYTES] = {
        .name = "system_zfs_zil_itx_indirect_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ZIL_ITX_COPIED_COUNT] = {
        .name = "system_zfs_zil_itx_copied_count",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ZIL_ITX_COPIED_BYTES] = {
        .name = "system_zfs_zil_itx_copied_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZFS_ZIL_ITX_NEEDCOPY_COUNT] = {
        .name = "system_zfs_zil_itx_needcopy_count",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ZIL_ITX_NEEDCOPY_BYTES] = {
        .name = "system_zfs_zil_itx_needcopy_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },

//  Transactions which have been allocated to the "normal"
//  (i.e. not slog) storage pool. Note that "bytes" accumulate
//  the actual log record sizes - which do not include the actual
//  data in case of indirect writes.  bytes <= write <= alloc.

    [FAM_ZFS_ZIL_ITX_METASLAB_NORMAL_COUNT] = {
        .name = "system_zfs_zil_itx_metaslab_normal_count",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ZIL_ITX_METASLAB_NORMAL_BYTES] = {
        .name = "system_zfs_zil_itx_metaslab_normal_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },

    [FAM_ZFS_ZIL_ITX_METASLAB_NORMAL_WRITE] = {
        .name = "system_zfs_zil_itx_metaslab_normal_write",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ZIL_ITX_METASLAB_NORMAL_ALLOC] = {
        .name = "system_zfs_zil_itx_metaslab_normal_alloc",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },

// Transactions which have been allocated to the "slog" storage pool.
// If there are no separate log devices, this is the same as the
// "normal" pool.  bytes <= write <= alloc.


    [FAM_ZFS_ZIL_ITX_METASLAB_SLOG_COUNT] = {
        .name = "system_zfs_zil_itx_metaslab_slog_count",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ZIL_ITX_METASLAB_SLOG_BYTES] = {
        .name = "system_zfs_zil_itx_metaslab_slog_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },

    [FAM_ZFS_ZIL_ITX_METASLAB_SLOG_WRITE] = {
        .name = "system_zfs_zil_itx_metaslab_slog_write",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZFS_ZIL_ITX_METASLAB_SLOG_ALLOC] = {
        .name = "system_zfs_zil_itx_metaslab_slog_alloc",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },

    [FAM_ZFS_QAT_COMP_REQUESTS] = {
        .name = "system_zfs_qat_comp_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of jobs submitted to QAT compression engine.",
    },
    [FAM_ZFS_QAT_COMP_IN_BYTES] = {
        .name = "system_zfs_qat_comp_in_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total bytes sent to QAT compression engine.",
    },
    [FAM_ZFS_QAT_COMP_OUT_BYTES] = {
        .name = "system_zfs_qat_comp_out_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total bytes output from QAT compression engine.",
    },
    [FAM_ZFS_QAT_DECOMP_REQUESTS] = {
        .name = "system_zfs_qat_decomp_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of jobs submitted to QAT de-compression engine.",
    },
    [FAM_ZFS_QAT_DECOMP_IN_BYTES] = {
        .name = "system_zfs_qat_decomp_in_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total bytes sent to QAT de-compression engine.",
    },
    [FAM_ZFS_QAT_DECOMP_OUT_BYTES] = {
        .name = "system_zfs_qat_decomp_out_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total bytes output from QAT de-compression engine.",
    },
    [FAM_ZFS_QAT_DC_FAILS] = {
        .name = "system_zfs_qat_dc_fails",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of fails in the QAT compression / decompression engine.",
    },
    [FAM_ZFS_QAT_ENCRYPT_REQUESTS] = {
        .name = "system_zfs_qat_encrypt_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of jobs submitted to QAT encryption engine.",
    },
    [FAM_ZFS_QAT_ENCRYPT_IN_BYTES] = {
        .name = "system_zfs_qat_encrypt_in_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total bytes sent to QAT encryption engine.",
    },
    [FAM_ZFS_QAT_ENCRYPT_OUT_BYTES] = {
        .name = "system_zfs_qat_encrypt_out_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total bytes output from QAT encryption engine.",
    },
    [FAM_ZFS_QAT_DECRYPT_REQUESTS] = {
        .name = "system_zfs_qat_decrypt_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of jobs submitted to QAT decryption engine.",
    },
    [FAM_ZFS_QAT_DECRYPT_IN_BYTES] = {
        .name = "system_zfs_qat_decrypt_in_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total bytes sent to QAT decryption engine.",
    },
    [FAM_ZFS_QAT_DECRYPT_OUT_BYTES] = {
        .name = "system_zfs_qat_decrypt_out_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total bytes output from QAT decryption engine.",
    },
    [FAM_ZFS_QAT_CRYPT_FAILS] = {
        .name = "system_zfs_qat_crypt_fails",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of fails in the QAT encryption / decryption engine.",
    },
    [FAM_ZFS_QAT_CKSUM_REQUESTS] = {
        .name = "system_zfs_qat_cksum_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of jobs submitted to QAT checksum engine.",
    },
    [FAM_ZFS_QAT_CKSUM_IN_BYTES] = {
        .name = "system_zfs_qat_cksum_in_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total bytes sent to QAT checksum engine.",
    },
    [FAM_ZFS_QAT_CKSUM_FAILS] = {
        .name = "system_zfs_qat_cksum_fails",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of fails in the QAT checksum engine.",
    },


#if 0
system_zfs_zpool_

nread
nwritten
reads
writes
wtime
wlentime
wupdate
rtime
rlentime
rupdate
wcnt
rcnt

system_zfs_zpool_dataset_

writes
nwritten
reads
nread
nunlinks
nunlinked
#endif
    [FAM_ZFS_ZPOOL_DATASET_WRITES] = {
        .name = "system_zfs_zpool_dataset_writes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of writes in this dataset.",
    },
    [FAM_ZFS_ZPOOL_DATASET_WRITTEN_BYTES] = {
        .name = "system_zfs_zpool_dataset_written_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of written bytes in this dataset.",
    },
    [FAM_ZFS_ZPOOL_DATASET_READS] = {
        .name = "system_zfs_zpool_dataset_reads",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of reads in this dataset.",
    },
    [FAM_ZFS_ZPOOL_DATASET_READ_BYTES] = {
        .name = "system_zfs_zpool_dataset_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of read bytes in this dataset.",
    },
    [FAM_ZFS_ZPOOL_DATASET_UNLINKS] = {
        .name = "system_zfs_zpool_dataset_unlinks",
        .type = METRIC_TYPE_COUNTER,
        .help =  "The number of files, directories, and so on that have been queued "
                 "for deletion in the ZFS delete queue.",
    },
    [FAM_ZFS_ZPOOL_DATASET_UNLINKED] = {
        .name = "system_zfs_zpool_dataset_unlinked",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of things that have actually been deleted.",
    },
    [FAM_ZFS_ZPOOL_STATE] = {
        .name = "system_zfs_zpool_state",
        .type = METRIC_TYPE_STATE_SET,
        .help = NULL,
    },
};
