/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

typedef enum {
    FAM_XFS_ALLOC_EXTENT,
    FAM_XFS_ALLOC_BLOCK,
    FAM_XFS_FREE_EXTENT,
    FAM_XFS_FREE_BLOCK,
    FAM_XFS_ALLOC_BTREE_LOOKUP,
    FAM_XFS_ALLOC_BTREE_COMPARE,
    FAM_XFS_ALLOC_BTREE_INSREC,
    FAM_XFS_ALLOC_BTREE_DELREC,
    FAM_XFS_BLOCK_MAP_READ_OPS,
    FAM_XFS_BLOCK_MAP_WRITE_OPS,
    FAM_XFS_BLOCK_MAP_UNMAP,
    FAM_XFS_BLOCK_MAP_ADD_EXLIST,
    FAM_XFS_BLOCK_MAP_DEL_EXLIST,
    FAM_XFS_BLOCK_MAP_LOOKUP_EXLIST,
    FAM_XFS_BLOCK_MAP_CMP_EXLIST,
    FAM_XFS_BLOCK_MAP_BTREE_LOOKUP,
    FAM_XFS_BLOCK_MAP_BTREE_COMPARE,
    FAM_XFS_BLOCK_MAP_BTREE_INSREC,
    FAM_XFS_BLOCK_MAP_BTREE_DELREC,
    FAM_XFS_DIR_OP_LOOKUP,
    FAM_XFS_DIR_OP_CREATE,
    FAM_XFS_DIR_OP_REMOVE,
    FAM_XFS_DIR_OP_GETDENTS,
    FAM_XFS_TRANSACTION_SYNC,
    FAM_XFS_TRANSACTION_ASYNC,
    FAM_XFS_TRANSACTION_EMPTY,
    FAM_XFS_INODE_OPS_IG_ATTEMPTS,
    FAM_XFS_INODE_OPS_IG_FOUND,
    FAM_XFS_INODE_OPS_IG_FRECYCLE,
    FAM_XFS_INODE_OPS_IG_MISSED,
    FAM_XFS_INODE_OPS_IG_DUP,
    FAM_XFS_INODE_OPS_IG_RECLAIMS,
    FAM_XFS_INODE_OPS_IG_ATTRCHG,
    FAM_XFS_LOG_WRITES,
    FAM_XFS_LOG_BLOCKS,
    FAM_XFS_LOG_NOICLOGS,
    FAM_XFS_LOG_FORCE,
    FAM_XFS_LOG_FORCE_SLEEP,
    FAM_XFS_LOG_TAIL_TRY_LOGSPACE,
    FAM_XFS_LOG_TAIL_SLEEP_LOGSPACE,
    FAM_XFS_LOG_TAIL_PUSH_AIL,
    FAM_XFS_LOG_TAIL_PUSH_AIL_SUCCESS,
    FAM_XFS_LOG_TAIL_PUSH_AIL_PUSHBUF,
    FAM_XFS_LOG_TAIL_PUSH_AIL_PINNED,
    FAM_XFS_LOG_TAIL_PUSH_AIL_LOCKED,
    FAM_XFS_LOG_TAIL_PUSH_AIL_FLUSHING,
    FAM_XFS_LOG_TAIL_PUSH_AIL_RESTARTS,
    FAM_XFS_LOG_TAIL_PUSH_AIL_FLUSH,
    FAM_XFS_XSTRAT_QUICK,
    FAM_XFS_XSTRAT_SPLIT,
    FAM_XFS_WRITE_CALLS,
    FAM_XFS_READ_CALLS,
    FAM_XFS_ATTR_GET,
    FAM_XFS_ATTR_SET,
    FAM_XFS_ATTR_REMOVE,
    FAM_XFS_ATTR_LIST,
    FAM_XFS_IFLUSH_COUNT,
    FAM_XFS_ICLUSTER_FLUSHCNT,
    FAM_XFS_ICLUSTER_FLUSHINODE,
    FAM_XFS_VNODE_ACTIVE,
    FAM_XFS_VNODE_ALLOC,
    FAM_XFS_VNODE_GET,
    FAM_XFS_VNODE_HOLD,
    FAM_XFS_VNODE_RELE,
    FAM_XFS_VNODE_RECLAIM,
    FAM_XFS_VNODE_REMOVE,
    FAM_XFS_VNODE_FREE,
    FAM_XFS_BUFFER_GET,
    FAM_XFS_BUFFER_CREATE,
    FAM_XFS_BUFFER_GET_LOCKED,
    FAM_XFS_BUFFER_GET_LOCKED_WAITED,
    FAM_XFS_BUFFER_BUSY_LOCKED,
    FAM_XFS_BUFFER_MISS_LOCKED,
    FAM_XFS_BUFFER_PAGE_RETRIES,
    FAM_XFS_BUFFER_PAGE_FOUND,
    FAM_XFS_BUFFER_GET_READ,
    FAM_XFS_ABTB2_LOOKUP,
    FAM_XFS_ABTB2_COMPARE,
    FAM_XFS_ABTB2_INSREC,
    FAM_XFS_ABTB2_DELREC,
    FAM_XFS_ABTB2_NEWROOT,
    FAM_XFS_ABTB2_KILLROOT,
    FAM_XFS_ABTB2_INCREMENT,
    FAM_XFS_ABTB2_DECREMENT,
    FAM_XFS_ABTB2_LSHIFT,
    FAM_XFS_ABTB2_RSHIFT,
    FAM_XFS_ABTB2_SPLIT,
    FAM_XFS_ABTB2_JOIN,
    FAM_XFS_ABTB2_ALLOC,
    FAM_XFS_ABTB2_FREE,
    FAM_XFS_ABTB2_MOVE,
    FAM_XFS_ABTC2_LOOKUP,
    FAM_XFS_ABTC2_COMPARE,
    FAM_XFS_ABTC2_INSREC,
    FAM_XFS_ABTC2_DELREC,
    FAM_XFS_ABTC2_NEWROOT,
    FAM_XFS_ABTC2_KILLROOT,
    FAM_XFS_ABTC2_INCREMENT,
    FAM_XFS_ABTC2_DECREMENT,
    FAM_XFS_ABTC2_LSHIFT,
    FAM_XFS_ABTC2_RSHIFT,
    FAM_XFS_ABTC2_SPLIT,
    FAM_XFS_ABTC2_JOIN,
    FAM_XFS_ABTC2_ALLOC,
    FAM_XFS_ABTC2_FREE,
    FAM_XFS_ABTC2_MOVE,
    FAM_XFS_BMBT2_LOOKUP,
    FAM_XFS_BMBT2_COMPARE,
    FAM_XFS_BMBT2_INSREC,
    FAM_XFS_BMBT2_DELREC,
    FAM_XFS_BMBT2_NEWROOT,
    FAM_XFS_BMBT2_KILLROOT,
    FAM_XFS_BMBT2_INCREMENT,
    FAM_XFS_BMBT2_DECREMENT,
    FAM_XFS_BMBT2_LSHIFT,
    FAM_XFS_BMBT2_RSHIFT,
    FAM_XFS_BMBT2_SPLIT,
    FAM_XFS_BMBT2_JOIN,
    FAM_XFS_BMBT2_ALLOC,
    FAM_XFS_BMBT2_FREE,
    FAM_XFS_BMBT2_MOVE,
    FAM_XFS_IBT2_LOOKUP,
    FAM_XFS_IBT2_COMPARE,
    FAM_XFS_IBT2_INSREC,
    FAM_XFS_IBT2_DELREC,
    FAM_XFS_IBT2_NEWROOT,
    FAM_XFS_IBT2_KILLROOT,
    FAM_XFS_IBT2_INCREMENT,
    FAM_XFS_IBT2_DECREMENT,
    FAM_XFS_IBT2_LSHIFT,
    FAM_XFS_IBT2_RSHIFT,
    FAM_XFS_IBT2_SPLIT,
    FAM_XFS_IBT2_JOIN,
    FAM_XFS_IBT2_ALLOC,
    FAM_XFS_IBT2_FREE,
    FAM_XFS_IBT2_MOVE,
    FAM_XFS_FIBT2_LOOKUP,
    FAM_XFS_FIBT2_COMPARE,
    FAM_XFS_FIBT2_INSREC,
    FAM_XFS_FIBT2_DELREC,
    FAM_XFS_FIBT2_NEWROOT,
    FAM_XFS_FIBT2_KILLROOT,
    FAM_XFS_FIBT2_INCREMENT,
    FAM_XFS_FIBT2_DECREMENT,
    FAM_XFS_FIBT2_LSHIFT,
    FAM_XFS_FIBT2_RSHIFT,
    FAM_XFS_FIBT2_SPLIT,
    FAM_XFS_FIBT2_JOIN,
    FAM_XFS_FIBT2_ALLOC,
    FAM_XFS_FIBT2_FREE,
    FAM_XFS_FIBT2_MOVE,
    FAM_XFS_RMAPBT_LOOKUP,
    FAM_XFS_RMAPBT_COMPARE,
    FAM_XFS_RMAPBT_INSREC,
    FAM_XFS_RMAPBT_DELREC,
    FAM_XFS_RMAPBT_NEWROOT,
    FAM_XFS_RMAPBT_KILLROOT,
    FAM_XFS_RMAPBT_INCREMENT,
    FAM_XFS_RMAPBT_DECREMENT,
    FAM_XFS_RMAPBT_LSHIFT,
    FAM_XFS_RMAPBT_RSHIFT,
    FAM_XFS_RMAPBT_SPLIT,
    FAM_XFS_RMAPBT_JOIN,
    FAM_XFS_RMAPBT_ALLOC,
    FAM_XFS_RMAPBT_FREE,
    FAM_XFS_RMAPBT_MOVE,
    FAM_XFS_REFCBT_LOOKUP,
    FAM_XFS_REFCBT_COMPARE,
    FAM_XFS_REFCBT_INSREC,
    FAM_XFS_REFCBT_DELREC,
    FAM_XFS_REFCBT_NEWROOT,
    FAM_XFS_REFCBT_KILLROOT,
    FAM_XFS_REFCBT_INCREMENT,
    FAM_XFS_REFCBT_DECREMENT,
    FAM_XFS_REFCBT_LSHIFT,
    FAM_XFS_REFCBT_RSHIFT,
    FAM_XFS_REFCBT_SPLIT,
    FAM_XFS_REFCBT_JOIN,
    FAM_XFS_REFCBT_ALLOC,
    FAM_XFS_REFCBT_FREE,
    FAM_XFS_REFCBT_MOVE,
    FAM_XFS_QUOTA_RECLAIMS,
    FAM_XFS_QUOTA_RECLAIM_MISSES,
    FAM_XFS_QUOTA_DQUOT_DUPS,
    FAM_XFS_QUOTA_CACHEMISSES,
    FAM_XFS_QUOTA_CACHEHITS,
    FAM_XFS_QUOTA_WANTS,
    FAM_XFS_QUOTA_DQUOT,
    FAM_XFS_QUOTA_DQUOT_UNUSED,
    FAM_XFS_XSTRAT_BYTES,
    FAM_XFS_WRITE_BYTES,
    FAM_XFS_READ_BYTES,
    FAM_XFS_DEFER_RELOG,
    FAM_XFS_MAX,
} fam_xfs_e;

static const fam_xfs_e xfs_stats_extent_alloc[] = {
    FAM_XFS_ALLOC_EXTENT,
    FAM_XFS_ALLOC_BLOCK,
    FAM_XFS_FREE_EXTENT,
    FAM_XFS_FREE_BLOCK,
};

static const fam_xfs_e xfs_stats_abt[] = {
    FAM_XFS_ALLOC_BTREE_LOOKUP,
    FAM_XFS_ALLOC_BTREE_COMPARE,
    FAM_XFS_ALLOC_BTREE_INSREC,
    FAM_XFS_ALLOC_BTREE_DELREC,
};

static const fam_xfs_e xfs_stats_blk_map[] = {
    FAM_XFS_BLOCK_MAP_READ_OPS,   FAM_XFS_BLOCK_MAP_WRITE_OPS,
    FAM_XFS_BLOCK_MAP_UNMAP,      FAM_XFS_BLOCK_MAP_ADD_EXLIST,
    FAM_XFS_BLOCK_MAP_DEL_EXLIST, FAM_XFS_BLOCK_MAP_LOOKUP_EXLIST,
    FAM_XFS_BLOCK_MAP_CMP_EXLIST,
};

static const fam_xfs_e xfs_stats_bmbt[] = {
    FAM_XFS_BLOCK_MAP_BTREE_LOOKUP,
    FAM_XFS_BLOCK_MAP_BTREE_COMPARE,
    FAM_XFS_BLOCK_MAP_BTREE_INSREC,
    FAM_XFS_BLOCK_MAP_BTREE_DELREC,
};

static const fam_xfs_e xfs_stats_dir[] = {
    FAM_XFS_DIR_OP_LOOKUP,
    FAM_XFS_DIR_OP_CREATE,
    FAM_XFS_DIR_OP_REMOVE,
    FAM_XFS_DIR_OP_GETDENTS,
};

static const fam_xfs_e xfs_stats_trans[] = {
    FAM_XFS_TRANSACTION_SYNC,
    FAM_XFS_TRANSACTION_ASYNC,
    FAM_XFS_TRANSACTION_EMPTY,
};

static const fam_xfs_e xfs_stats_ig[] = {
    FAM_XFS_INODE_OPS_IG_ATTEMPTS, FAM_XFS_INODE_OPS_IG_FOUND,
    FAM_XFS_INODE_OPS_IG_FRECYCLE, FAM_XFS_INODE_OPS_IG_MISSED,
    FAM_XFS_INODE_OPS_IG_DUP,      FAM_XFS_INODE_OPS_IG_RECLAIMS,
    FAM_XFS_INODE_OPS_IG_ATTRCHG,
};

static const fam_xfs_e xfs_stats_log[] = {
    FAM_XFS_LOG_WRITES, FAM_XFS_LOG_BLOCKS,          FAM_XFS_LOG_NOICLOGS,
    FAM_XFS_LOG_FORCE,  FAM_XFS_LOG_FORCE_SLEEP,
};

static const fam_xfs_e xfs_stats_push_ail[] = {
    FAM_XFS_LOG_TAIL_TRY_LOGSPACE,      FAM_XFS_LOG_TAIL_SLEEP_LOGSPACE,
    FAM_XFS_LOG_TAIL_PUSH_AIL,          FAM_XFS_LOG_TAIL_PUSH_AIL_SUCCESS,
    FAM_XFS_LOG_TAIL_PUSH_AIL_PUSHBUF,  FAM_XFS_LOG_TAIL_PUSH_AIL_PINNED,
    FAM_XFS_LOG_TAIL_PUSH_AIL_LOCKED,   FAM_XFS_LOG_TAIL_PUSH_AIL_FLUSHING,
    FAM_XFS_LOG_TAIL_PUSH_AIL_RESTARTS, FAM_XFS_LOG_TAIL_PUSH_AIL_FLUSH,
};

static const fam_xfs_e xfs_stats_xstrat[] = {
    FAM_XFS_XSTRAT_QUICK,
    FAM_XFS_XSTRAT_SPLIT,
};

static const fam_xfs_e xfs_stats_rw[] = {
    FAM_XFS_WRITE_CALLS,
    FAM_XFS_READ_CALLS,
};

static const fam_xfs_e xfs_stats_attr[] = {
    FAM_XFS_ATTR_GET,
    FAM_XFS_ATTR_SET,
    FAM_XFS_ATTR_REMOVE,
    FAM_XFS_ATTR_LIST,
};

static const fam_xfs_e xfs_stats_icluster[] = {
    FAM_XFS_IFLUSH_COUNT,
    FAM_XFS_ICLUSTER_FLUSHCNT,
    FAM_XFS_ICLUSTER_FLUSHINODE,
};

static const fam_xfs_e xfs_stats_vnodes[] = {
    FAM_XFS_VNODE_ACTIVE, FAM_XFS_VNODE_ALLOC, FAM_XFS_VNODE_GET,
    FAM_XFS_VNODE_HOLD,   FAM_XFS_VNODE_RELE,  FAM_XFS_VNODE_RECLAIM,
    FAM_XFS_VNODE_REMOVE, FAM_XFS_VNODE_FREE,
};

static const fam_xfs_e xfs_stats_buf[] = {
    FAM_XFS_BUFFER_GET,          FAM_XFS_BUFFER_CREATE,
    FAM_XFS_BUFFER_GET_LOCKED,   FAM_XFS_BUFFER_GET_LOCKED_WAITED,
    FAM_XFS_BUFFER_BUSY_LOCKED,  FAM_XFS_BUFFER_MISS_LOCKED,
    FAM_XFS_BUFFER_PAGE_RETRIES, FAM_XFS_BUFFER_PAGE_FOUND,
    FAM_XFS_BUFFER_GET_READ,
};

static const fam_xfs_e xfs_stats_abtb2[] = {
    FAM_XFS_ABTB2_LOOKUP,        FAM_XFS_ABTB2_COMPARE,   FAM_XFS_ABTB2_INSREC,
    FAM_XFS_ABTB2_DELREC,        FAM_XFS_ABTB2_NEWROOT,   FAM_XFS_ABTB2_KILLROOT,
    FAM_XFS_ABTB2_INCREMENT,     FAM_XFS_ABTB2_DECREMENT, FAM_XFS_ABTB2_LSHIFT,
    FAM_XFS_ABTB2_RSHIFT,        FAM_XFS_ABTB2_SPLIT,     FAM_XFS_ABTB2_JOIN,
    FAM_XFS_ABTB2_ALLOC,         FAM_XFS_ABTB2_FREE,      FAM_XFS_ABTB2_MOVE,
};

static const fam_xfs_e xfs_stats_abtc2[] = {
    FAM_XFS_ABTC2_LOOKUP,    FAM_XFS_ABTC2_COMPARE,   FAM_XFS_ABTC2_INSREC,
    FAM_XFS_ABTC2_DELREC,    FAM_XFS_ABTC2_NEWROOT,   FAM_XFS_ABTC2_KILLROOT,
    FAM_XFS_ABTC2_INCREMENT, FAM_XFS_ABTC2_DECREMENT, FAM_XFS_ABTC2_LSHIFT,
    FAM_XFS_ABTC2_RSHIFT,    FAM_XFS_ABTC2_SPLIT,     FAM_XFS_ABTC2_JOIN,
    FAM_XFS_ABTC2_ALLOC,     FAM_XFS_ABTC2_FREE,      FAM_XFS_ABTC2_MOVE,
};

static const fam_xfs_e xfs_stats_bmbt2[] = {
    FAM_XFS_BMBT2_LOOKUP,    FAM_XFS_BMBT2_COMPARE,   FAM_XFS_BMBT2_INSREC,
    FAM_XFS_BMBT2_DELREC,    FAM_XFS_BMBT2_NEWROOT,   FAM_XFS_BMBT2_KILLROOT,
    FAM_XFS_BMBT2_INCREMENT, FAM_XFS_BMBT2_DECREMENT, FAM_XFS_BMBT2_LSHIFT,
    FAM_XFS_BMBT2_RSHIFT,    FAM_XFS_BMBT2_SPLIT,     FAM_XFS_BMBT2_JOIN,
    FAM_XFS_BMBT2_ALLOC,     FAM_XFS_BMBT2_FREE,      FAM_XFS_BMBT2_MOVE,
};

static const fam_xfs_e xfs_stats_ibt2[] = {
    FAM_XFS_IBT2_LOOKUP,    FAM_XFS_IBT2_COMPARE,   FAM_XFS_IBT2_INSREC,
    FAM_XFS_IBT2_DELREC,    FAM_XFS_IBT2_NEWROOT,   FAM_XFS_IBT2_KILLROOT,
    FAM_XFS_IBT2_INCREMENT, FAM_XFS_IBT2_DECREMENT, FAM_XFS_IBT2_LSHIFT,
    FAM_XFS_IBT2_RSHIFT,    FAM_XFS_IBT2_SPLIT,     FAM_XFS_IBT2_JOIN,
    FAM_XFS_IBT2_ALLOC,     FAM_XFS_IBT2_FREE,      FAM_XFS_IBT2_MOVE,
};

static const fam_xfs_e xfs_stats_fibt2[] = {
    FAM_XFS_FIBT2_LOOKUP,    FAM_XFS_FIBT2_COMPARE,   FAM_XFS_FIBT2_INSREC,
    FAM_XFS_FIBT2_DELREC,    FAM_XFS_FIBT2_NEWROOT,   FAM_XFS_FIBT2_KILLROOT,
    FAM_XFS_FIBT2_INCREMENT, FAM_XFS_FIBT2_DECREMENT, FAM_XFS_FIBT2_LSHIFT,
    FAM_XFS_FIBT2_RSHIFT,    FAM_XFS_FIBT2_SPLIT,     FAM_XFS_FIBT2_JOIN,
    FAM_XFS_FIBT2_ALLOC,     FAM_XFS_FIBT2_FREE,      FAM_XFS_FIBT2_MOVE,
};

static const fam_xfs_e xfs_stats_rmapbt[] = {
    FAM_XFS_RMAPBT_LOOKUP,    FAM_XFS_RMAPBT_COMPARE,   FAM_XFS_RMAPBT_INSREC,
    FAM_XFS_RMAPBT_DELREC,    FAM_XFS_RMAPBT_NEWROOT,   FAM_XFS_RMAPBT_KILLROOT,
    FAM_XFS_RMAPBT_INCREMENT, FAM_XFS_RMAPBT_DECREMENT, FAM_XFS_RMAPBT_LSHIFT,
    FAM_XFS_RMAPBT_RSHIFT,    FAM_XFS_RMAPBT_SPLIT,     FAM_XFS_RMAPBT_JOIN,
    FAM_XFS_RMAPBT_ALLOC,     FAM_XFS_RMAPBT_FREE,      FAM_XFS_RMAPBT_MOVE,
};

static const fam_xfs_e xfs_stats_refcntbt[] = {
    FAM_XFS_REFCBT_LOOKUP,    FAM_XFS_REFCBT_COMPARE,   FAM_XFS_REFCBT_INSREC,
    FAM_XFS_REFCBT_DELREC,    FAM_XFS_REFCBT_NEWROOT,   FAM_XFS_REFCBT_KILLROOT,
    FAM_XFS_REFCBT_INCREMENT, FAM_XFS_REFCBT_DECREMENT, FAM_XFS_REFCBT_LSHIFT,
    FAM_XFS_REFCBT_RSHIFT,    FAM_XFS_REFCBT_SPLIT,     FAM_XFS_REFCBT_JOIN,
    FAM_XFS_REFCBT_ALLOC,     FAM_XFS_REFCBT_FREE,      FAM_XFS_REFCBT_MOVE,
};

static const fam_xfs_e xfs_stats_qm[] = {
    FAM_XFS_QUOTA_RECLAIMS,   FAM_XFS_QUOTA_RECLAIM_MISSES,
    FAM_XFS_QUOTA_DQUOT_DUPS, FAM_XFS_QUOTA_CACHEMISSES,
    FAM_XFS_QUOTA_CACHEHITS,  FAM_XFS_QUOTA_WANTS,
    FAM_XFS_QUOTA_DQUOT,      FAM_XFS_QUOTA_DQUOT_UNUSED,
};

static const fam_xfs_e xfs_stats_xpc[] = {
    FAM_XFS_XSTRAT_BYTES,
    FAM_XFS_WRITE_BYTES,
    FAM_XFS_READ_BYTES,
};

static const fam_xfs_e xfs_stats_defer_relog[] = {
    FAM_XFS_DEFER_RELOG,
};

static  metric_family_t fams[FAM_XFS_MAX] = {
    [FAM_XFS_ALLOC_EXTENT] = {
        .name = "system_xfs_alloc_extent",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of file system extents allocated over all XFS filesystems.",
    },
    [FAM_XFS_ALLOC_BLOCK] = {
        .name = "system_xfs_alloc_block",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of file system blocks allocated over all XFS filesystems.",
    },
    [FAM_XFS_FREE_EXTENT] = {
        .name = "system_xfs_free_extent",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of file system extents freed over all XFS filesystems.",
    },
    [FAM_XFS_FREE_BLOCK] = {
        .name = "system_xfs_free_block",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of file system blocks freed over all XFS filesystems.",
    },
    [FAM_XFS_ALLOC_BTREE_LOOKUP] = {
        .name = "system_xfs_alloc_btree_lookup",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of lookup operations in XFS filesystem allocation btrees.",
    },
    [FAM_XFS_ALLOC_BTREE_COMPARE] = {
        .name = "system_xfs_alloc_btree_compare",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of compares in XFS filesystem allocation btree lookups.",
    },
    [FAM_XFS_ALLOC_BTREE_INSREC] = {
        .name = "system_xfs_alloc_btree_insrec",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of extent records inserted into XFS filesystem allocation btrees.",
    },
    [FAM_XFS_ALLOC_BTREE_DELREC] = {
        .name = "system_xfs_alloc_btree_delrec",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of extent records deleted from XFS filesystem allocation btrees.",
    },
    [FAM_XFS_BLOCK_MAP_READ_OPS] = {
        .name = "system_xfs_block_map_read_ops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of block map for read operations performed on XFS files.",
    },
    [FAM_XFS_BLOCK_MAP_WRITE_OPS] = {
        .name = "system_xfs_block_map_write_ops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of block map for write operations performed on XFS files.",
    },
    [FAM_XFS_BLOCK_MAP_UNMAP] = {
        .name = "system_xfs_block_map_unmap",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of block unmap (delete) operations performed on XFS files.",
    },
    [FAM_XFS_BLOCK_MAP_ADD_EXLIST] = {
        .name = "system_xfs_block_map_add_exlist",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of extent list insertion operations for XFS files.",
    },
    [FAM_XFS_BLOCK_MAP_DEL_EXLIST] = {
        .name = "system_xfs_block_map_del_exlist",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of extent list deletion operations for XFS files.",
    },
    [FAM_XFS_BLOCK_MAP_LOOKUP_EXLIST] = {
        .name = "system_xfs_block_map_lookup_exlist",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of extent list lookup operations for XFS files.",
    },
    [FAM_XFS_BLOCK_MAP_CMP_EXLIST] = {
        .name = "system_xfs_block_map_cmp_exlist",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of extent list comparisons in XFS extent list lookups.",
    },
    [FAM_XFS_BLOCK_MAP_BTREE_LOOKUP] = {
        .name = "system_xfs_block_map_btree_lookup",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of block map btree lookup operations on XFS files.",
    },
    [FAM_XFS_BLOCK_MAP_BTREE_COMPARE] = {
        .name = "system_xfs_block_map_btree_compare",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of block map btree compare operations in XFS block map lookups.",
    },
    [FAM_XFS_BLOCK_MAP_BTREE_INSREC] = {
        .name = "system_xfs_block_map_btree_insrec",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of block map btree records inserted for XFS files.",
    },
    [FAM_XFS_BLOCK_MAP_BTREE_DELREC] = {
        .name = "system_xfs_block_map_btree_delrec",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of block map btree records deleted for XFS files.",
    },
    [FAM_XFS_DIR_OP_LOOKUP] = {
        .name = "system_xfs_dir_op_lookup",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of file name directory lookups in XFS filesystems.",
    },
    [FAM_XFS_DIR_OP_CREATE] = {
        .name = "system_xfs_dir_op_create",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times a new directory entry was created in XFS filesystems.",
    },
    [FAM_XFS_DIR_OP_REMOVE] = {
        .name = "system_xfs_dir_op_remove",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times an existing directory entry was removed in XFS filesystems.",
    },
    [FAM_XFS_DIR_OP_GETDENTS] = {
        .name = "system_xfs_dir_op_getdents",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times the XFS directory getdents operation was performed.",
    },
    [FAM_XFS_TRANSACTION_SYNC] = {
        .name = "system_xfs_transaction_sync",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of meta-data transactions which waited to be committed to the "
                "on-disk log before allowing the process performing the transaction to continue.",
    },
    [FAM_XFS_TRANSACTION_ASYNC] = {
        .name = "system_xfs_transaction_async",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of meta-data transactions which did not wait to be committed to "
                "the on-disk log before allowing the process performing the transaction to continue.",
    },
    [FAM_XFS_TRANSACTION_EMPTY] = {
        .name = "system_xfs_transaction_empty",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of meta-data transactions which did not actually change anything.",
    },
    [FAM_XFS_INODE_OPS_IG_ATTEMPTS] = {
        .name = "system_xfs_inode_ops_ig_attempts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times the operating system looked for an XFS inode "
                "in the inode cache.",
    },
    [FAM_XFS_INODE_OPS_IG_FOUND] = {
        .name = "system_xfs_inode_ops_ig_found",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times the operating system looked for an XFS inode "
                "in the inode cache and found it.",
    },
    [FAM_XFS_INODE_OPS_IG_FRECYCLE] = {
        .name = "system_xfs_inode_ops_ig_frecycle",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times the operating system looked for an XFS inode in the inode "
                "cache and saw that it was there but was unable to use the in memory inode "
                "because it was being recycled by another process.",
    },
    [FAM_XFS_INODE_OPS_IG_MISSED] = {
        .name = "system_xfs_inode_ops_ig_missed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times the operating system looked for an XFS inode "
                " in the inode cache and the inode was not there.",
    },
    [FAM_XFS_INODE_OPS_IG_DUP] = {
        .name = "system_xfs_inode_ops_ig_dup",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times the operating system looked for an XFS inode "
                "in the inode cache and found that it was not there but upon attempting "
                "to add the inode to the cache found that another process had already inserted it.",
    },
    [FAM_XFS_INODE_OPS_IG_RECLAIMS] = {
        .name = "system_xfs_inode_ops_ig_reclaims",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times the operating system recycled an XFS inode "
                "from the inode cache in order to use the memory for that inode for another purpose.",
    },
    [FAM_XFS_INODE_OPS_IG_ATTRCHG] = {
        .name = "system_xfs_inode_ops_ig_attrchg",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times the operating system explicitly changed "
                "the attributes of an XFS inode.",
    },
    [FAM_XFS_LOG_WRITES] = {
        .name = "system_xfs_log_writes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of log buffer writes going to the physical log partitions "
                "of all XFS filesystems.",
    },
    [FAM_XFS_LOG_BLOCKS] = {
        .name = "system_xfs_log_blocks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Counts (in 512-byte units) the information being written to the physical "
                "log partitions of all XFS filesystems.",
    },
    [FAM_XFS_LOG_NOICLOGS] = {
        .name = "system_xfs_log_noiclogs",
        .type = METRIC_TYPE_COUNTER,
        .help = "Times when a logged transaction can not get any log buffer space.",
    },
    [FAM_XFS_LOG_FORCE] = {
        .name = "system_xfs_log_force",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times the in-core log is forced to disk.",
    },
    [FAM_XFS_LOG_FORCE_SLEEP] = {
        .name = "system_xfs_log_force_sleep",
        .type = METRIC_TYPE_COUNTER,
        .help = "Value exported from the xs_log_force_sleep field of struct xfsstats.",
    },
    [FAM_XFS_LOG_TAIL_TRY_LOGSPACE] = {
        .name = "system_xfs_log_tail_try_logspace",
        .type = METRIC_TYPE_COUNTER,
        .help = "Value from the xs_try_logspace field of struct xfsstats.",
    },
    [FAM_XFS_LOG_TAIL_SLEEP_LOGSPACE] = {
        .name = "system_xfs_log_tail_sleep_logspace",
        .type = METRIC_TYPE_COUNTER,
        .help = "Value from the xs_sleep_logspace field of struct xfsstats.",
    },
    [FAM_XFS_LOG_TAIL_PUSH_AIL] = {
        .name = "system_xfs_log_tail_push_ail",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times the tail of the AIL is moved forward. "
                "It is equivalent to the number of successful calls to the function xfs_trans_push_ail().",
    },
    [FAM_XFS_LOG_TAIL_PUSH_AIL_SUCCESS] = {
        .name = "system_xfs_log_tail_push_ail_success",
        .type = METRIC_TYPE_COUNTER,
        .help = "Value from xs_push_ail_success field of struct xfsstats.",
    },
    [FAM_XFS_LOG_TAIL_PUSH_AIL_PUSHBUF] = {
        .name = "system_xfs_log_tail_push_ail_pushbuf",
        .type = METRIC_TYPE_COUNTER,
        .help = "Value from xs_push_ail_pushbuf field of struct xfsstats.",
    },
    [FAM_XFS_LOG_TAIL_PUSH_AIL_PINNED] = {
        .name = "system_xfs_log_tail_push_ail_pinned",
        .type = METRIC_TYPE_COUNTER,
        .help = "Value from xs_push_ail_pinned field of struct xfsstats.",
    },
    [FAM_XFS_LOG_TAIL_PUSH_AIL_LOCKED] = {
        .name = "system_xfs_log_tail_push_ail_locked",
        .type = METRIC_TYPE_COUNTER,
        .help = "Value from xs_push_ail_locked field of struct xfsstats.",
    },
    [FAM_XFS_LOG_TAIL_PUSH_AIL_FLUSHING] = {
        .name = "system_xfs_log_tail_push_ail_flushing",
        .type = METRIC_TYPE_COUNTER,
        .help = "Value from xs_push_ail_flushing field of struct xfsstats.",
    },
    [FAM_XFS_LOG_TAIL_PUSH_AIL_RESTARTS] = {
        .name = "system_xfs_log_tail_push_ail_restarts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Value from xs_push_ail_restarts field of struct xfsstats.",
    },
    [FAM_XFS_LOG_TAIL_PUSH_AIL_FLUSH] = {
        .name = "system_xfs_log_tail_push_ail_flush",
        .type = METRIC_TYPE_COUNTER,
        .help = "Value from xs_push_ail_flush field of struct xfsstats.",
    },
    [FAM_XFS_XSTRAT_QUICK] = {
        .name = "system_xfs_xstrat_quick",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of buffers flushed out by the XFS flushing daemons "
                "which are written to contiguous space on disk.",
    },
    [FAM_XFS_XSTRAT_SPLIT] = {
        .name = "system_xfs_xstrat_split",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number f buffers flushed out by the XFS flushing daemons "
                "which are written to non-contiguous space on disk.",
    },
    [FAM_XFS_WRITE_CALLS] = {
        .name = "system_xfs_write_calls",
        .type = METRIC_TYPE_COUNTER,
        .help = "This is the number of write(2) system calls made to files in XFS file systems.",
    },
    [FAM_XFS_READ_CALLS] = {
        .name = "system_xfs_read_calls",
        .type = METRIC_TYPE_COUNTER,
        .help = "This is the number of read(2) system calls made to files in XFS file systems.",
    },
    [FAM_XFS_ATTR_GET] = {
        .name = "system_xfs_attr_get",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of \"get\" operations performed on extended file attributes "
                "within XFS filesystems.",
    },
    [FAM_XFS_ATTR_SET] = {
        .name = "system_xfs_attr_set",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of \"set\" operations performed on extended file attributes "
                "within XFS filesystems.",
    },
    [FAM_XFS_ATTR_REMOVE] = {
        .name = "system_xfs_attr_remove",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of \"remove\" operations performed on extended file attributes "
                "within XFS filesystems.",
    },
    [FAM_XFS_ATTR_LIST] = {
        .name = "system_xfs_attr_list",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of \"list\" operations performed on extended file attributes "
                "within XFS filesystems.",
    },
    [FAM_XFS_IFLUSH_COUNT] = {
        .name = "system_xfs_iflush_count",
        .type = METRIC_TYPE_COUNTER,
        .help = "This is the number of calls to xfs_iflush which gets called when an inode "
                "is being flushed (such as by bdflush or tail pushing).",
    },
    [FAM_XFS_ICLUSTER_FLUSHCNT] = {
        .name = "system_xfs_icluster_flushcnt",
        .type = METRIC_TYPE_COUNTER,
        .help = "Value from xs_icluster_flushcnt field of struct xfsstats.",
    },
    [FAM_XFS_ICLUSTER_FLUSHINODE] = {
        .name = "system_xfs_icluster_flushinode",
        .type = METRIC_TYPE_COUNTER,
        .help = "This is the number of times that the inode clustering was not able "
                "to flush anything but the one inode it was called with."
    },
    [FAM_XFS_VNODE_ACTIVE] = {
        .name = "system_xfs_vnode_active",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of vnodes not on free lists.",
    },
    [FAM_XFS_VNODE_ALLOC] = {
        .name = "system_xfs_vnode_alloc",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times vn_alloc called.",
    },
    [FAM_XFS_VNODE_GET] = {
        .name = "system_xfs_vnode_get",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times vn_get called.",
    },
    [FAM_XFS_VNODE_HOLD] = {
        .name = "system_xfs_vnode_hold",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times vn_hold called.",
    },
    [FAM_XFS_VNODE_RELE] = {
        .name = "system_xfs_vnode_rele",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times vn_rele called.",
    },
    [FAM_XFS_VNODE_RECLAIM] = {
        .name = "system_xfs_vnode_reclaim",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times vn_reclaim called.",
    },
    [FAM_XFS_VNODE_REMOVE] = {
        .name = "system_xfs_vnode_remove",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times vn_remove called.",
    },
    [FAM_XFS_VNODE_FREE] = {
        .name = "system_xfs_vnode_free",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times vn_free called.",
    },
    [FAM_XFS_BUFFER_GET] = {
        .name = "system_xfs_buffer_get",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_BUFFER_CREATE] = {
        .name = "system_xfs_buffer_create",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_BUFFER_GET_LOCKED] = {
        .name = "system_xfs_buffer_get_locked",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_BUFFER_GET_LOCKED_WAITED] = {
        .name = "system_xfs_buffer_get_locked_waited",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_BUFFER_BUSY_LOCKED] = {
        .name = "system_xfs_buffer_busy_locked",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_BUFFER_MISS_LOCKED] = {
        .name = "system_xfs_buffer_miss_locked",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_BUFFER_PAGE_RETRIES] = {
        .name = "system_xfs_buffer_page_retries",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_BUFFER_PAGE_FOUND] = {
        .name = "system_xfs_buffer_page_found",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_BUFFER_GET_READ] = {
        .name = "system_xfs_buffer_get_read",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTB2_LOOKUP] = {
        .name = "system_xfs_abtb2_lookup",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTB2_COMPARE] = {
        .name = "system_xfs_abtb2_compare",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTB2_INSREC] = {
        .name = "system_xfs_abtb2_insrec",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTB2_DELREC] = {
        .name = "system_xfs_abtb2_delrec",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTB2_NEWROOT] = {
        .name = "system_xfs_abtb2_newroot",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTB2_KILLROOT] = {
        .name = "system_xfs_abtb2_killroot",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTB2_INCREMENT] = {
        .name = "system_xfs_abtb2_increment",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTB2_DECREMENT] = {
        .name = "system_xfs_abtb2_decrement",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTB2_LSHIFT] = {
        .name = "system_xfs_abtb2_lshift",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTB2_RSHIFT] = {
        .name = "system_xfs_abtb2_rshift",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTB2_SPLIT] = {
        .name = "system_xfs_abtb2_split",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTB2_JOIN] = {
        .name = "system_xfs_abtb2_join",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTB2_ALLOC] = {
        .name = "system_xfs_abtb2_alloc",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTB2_FREE] = {
        .name = "system_xfs_abtb2_free",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTB2_MOVE] = {
        .name = "system_xfs_abtb2_move",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTC2_LOOKUP] = {
        .name = "system_xfs_abtc2_lookup",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTC2_COMPARE] = {
        .name = "system_xfs_abtc2_compare",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTC2_INSREC] = {
        .name = "system_xfs_abtc2_insrec",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTC2_DELREC] = {
        .name = "system_xfs_abtc2_delrec",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTC2_NEWROOT] = {
        .name = "system_xfs_abtc2_newroot",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTC2_KILLROOT] = {
        .name = "system_xfs_abtc2_killroot",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTC2_INCREMENT] = {
        .name = "system_xfs_abtc2_increment",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTC2_DECREMENT] = {
        .name = "system_xfs_abtc2_decrement",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTC2_LSHIFT] = {
        .name = "system_xfs_abtc2_lshift",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTC2_RSHIFT] = {
        .name = "system_xfs_abtc2_rshift",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTC2_SPLIT] = {
        .name = "system_xfs_abtc2_split",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTC2_JOIN] = {
        .name = "system_xfs_abtc2_join",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTC2_ALLOC] = {
        .name = "system_xfs_abtc2_alloc",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTC2_FREE] = {
        .name = "system_xfs_abtc2_free",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_ABTC2_MOVE] = {
        .name = "system_xfs_abtc2_move",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_BMBT2_LOOKUP] = {
        .name = "system_xfs_bmbt2_lookup",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_BMBT2_COMPARE] = {
        .name = "system_xfs_bmbt2_compare",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_BMBT2_INSREC] = {
        .name = "system_xfs_bmbt2_insrec",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_BMBT2_DELREC] = {
        .name = "system_xfs_bmbt2_delrec",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_BMBT2_NEWROOT] = {
        .name = "system_xfs_bmbt2_newroot",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_BMBT2_KILLROOT] = {
        .name = "system_xfs_bmbt2_killroot",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_BMBT2_INCREMENT] = {
        .name = "system_xfs_bmbt2_increment",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_BMBT2_DECREMENT] = {
        .name = "system_xfs_bmbt2_decrement",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_BMBT2_LSHIFT] = {
        .name = "system_xfs_bmbt2_lshift",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_BMBT2_RSHIFT] = {
        .name = "system_xfs_bmbt2_rshift",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_BMBT2_SPLIT] = {
        .name = "system_xfs_bmbt2_split",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_BMBT2_JOIN] = {
        .name = "system_xfs_bmbt2_join",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_BMBT2_ALLOC] = {
        .name = "system_xfs_bmbt2_alloc",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_BMBT2_FREE] = {
        .name = "system_xfs_bmbt2_free",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_BMBT2_MOVE] = {
        .name = "system_xfs_bmbt2_move",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_IBT2_LOOKUP] = {
        .name = "system_xfs_ibt2_lookup",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_IBT2_COMPARE] = {
        .name = "system_xfs_ibt2_compare",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_IBT2_INSREC] = {
        .name = "system_xfs_ibt2_insrec",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_IBT2_DELREC] = {
        .name = "system_xfs_ibt2_delrec",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_IBT2_NEWROOT] = {
        .name = "system_xfs_ibt2_newroot",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_IBT2_KILLROOT] = {
        .name = "system_xfs_ibt2_killroot",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_IBT2_INCREMENT] = {
        .name = "system_xfs_ibt2_increment",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_IBT2_DECREMENT] = {
        .name = "system_xfs_ibt2_decrement",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_IBT2_LSHIFT] = {
        .name = "system_xfs_ibt2_lshift",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_IBT2_RSHIFT] = {
        .name = "system_xfs_ibt2_rshift",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_IBT2_SPLIT] = {
        .name = "system_xfs_ibt2_split",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_IBT2_JOIN] = {
        .name = "system_xfs_ibt2_join",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_IBT2_ALLOC] = {
        .name = "system_xfs_ibt2_alloc",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_IBT2_FREE] = {
        .name = "system_xfs_ibt2_free",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_IBT2_MOVE] = {
        .name = "system_xfs_ibt2_move",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_FIBT2_LOOKUP] = {
        .name = "system_xfs_fibt2_lookup",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_FIBT2_COMPARE] = {
        .name = "system_xfs_fibt2_compare",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_FIBT2_INSREC] = {
        .name = "system_xfs_fibt2_insrec",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_FIBT2_DELREC] = {
        .name = "system_xfs_fibt2_delrec",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_FIBT2_NEWROOT] = {
        .name = "system_xfs_fibt2_newroot",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_FIBT2_KILLROOT] = {
        .name = "system_xfs_fibt2_killroot",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_FIBT2_INCREMENT] = {
        .name = "system_xfs_fibt2_increment",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_FIBT2_DECREMENT] = {
        .name = "system_xfs_fibt2_decrement",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_FIBT2_LSHIFT] = {
        .name = "system_xfs_fibt2_lshift",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_FIBT2_RSHIFT] = {
        .name = "system_xfs_fibt2_rshift",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_FIBT2_SPLIT] = {
        .name = "system_xfs_fibt2_split",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_FIBT2_JOIN] = {
        .name = "system_xfs_fibt2_join",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_FIBT2_ALLOC] = {
        .name = "system_xfs_fibt2_alloc",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_FIBT2_FREE] = {
        .name = "system_xfs_fibt2_free",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_FIBT2_MOVE] = {
        .name = "system_xfs_fibt2_move",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_RMAPBT_LOOKUP] = {
        .name = "system_xfs_rmapbt_lookup",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_RMAPBT_COMPARE] = {
        .name = "system_xfs_rmapbt_compare",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_RMAPBT_INSREC] = {
        .name = "system_xfs_rmapbt_insrec",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_RMAPBT_DELREC] = {
        .name = "system_xfs_rmapbt_delrec",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_RMAPBT_NEWROOT] = {
        .name = "system_xfs_rmapbt_newroot",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_RMAPBT_KILLROOT] = {
        .name = "system_xfs_rmapbt_killroot",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_RMAPBT_INCREMENT] = {
        .name = "system_xfs_rmapbt_increment",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_RMAPBT_DECREMENT] = {
        .name = "system_xfs_rmapbt_decrement",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_RMAPBT_LSHIFT] = {
        .name = "system_xfs_rmapbt_lshift",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_RMAPBT_RSHIFT] = {
        .name = "system_xfs_rmapbt_rshift",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_RMAPBT_SPLIT] = {
        .name = "system_xfs_rmapbt_split",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_RMAPBT_JOIN] = {
        .name = "system_xfs_rmapbt_join",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_RMAPBT_ALLOC] = {
        .name = "system_xfs_rmapbt_alloc",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_RMAPBT_FREE] = {
        .name = "system_xfs_rmapbt_free",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_RMAPBT_MOVE] = {
        .name = "system_xfs_rmapbt_move",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_REFCBT_LOOKUP] = {
        .name = "system_xfs_refcbt_lookup",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_REFCBT_COMPARE] = {
        .name = "system_xfs_refcbt_compare",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_REFCBT_INSREC] = {
        .name = "system_xfs_refcbt_insrec",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_REFCBT_DELREC] = {
        .name = "system_xfs_refcbt_delrec",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_REFCBT_NEWROOT] = {
        .name = "system_xfs_refcbt_newroot",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_REFCBT_KILLROOT] = {
        .name = "system_xfs_refcbt_killroot",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_REFCBT_INCREMENT] = {
        .name = "system_xfs_refcbt_increment",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_REFCBT_DECREMENT] = {
        .name = "system_xfs_refcbt_decrement",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_REFCBT_LSHIFT] = {
        .name = "system_xfs_refcbt_lshift",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_REFCBT_RSHIFT] = {
        .name = "system_xfs_refcbt_rshift",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_REFCBT_SPLIT] = {
        .name = "system_xfs_refcbt_split",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_REFCBT_JOIN] = {
        .name = "system_xfs_refcbt_join",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_REFCBT_ALLOC] = {
        .name = "system_xfs_refcbt_alloc",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_REFCBT_FREE] = {
        .name = "system_xfs_refcbt_free",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_REFCBT_MOVE] = {
        .name = "system_xfs_refcbt_move",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_QUOTA_RECLAIMS] = {
        .name = "system_xfs_quota_reclaims",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_QUOTA_RECLAIM_MISSES] = {
        .name = "system_xfs_quota_reclaim_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_QUOTA_DQUOT_DUPS] = {
        .name = "system_xfs_quota_dquot_dups",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_QUOTA_CACHEMISSES] = {
        .name = "system_xfs_quota_cachemisses",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_QUOTA_CACHEHITS] = {
        .name = "system_xfs_quota_cachehits",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_QUOTA_WANTS] = {
        .name = "system_xfs_quota_wants",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_QUOTA_DQUOT] = {
        .name = "system_xfs_quota_dquot",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_QUOTA_DQUOT_UNUSED] = {
        .name = "system_xfs_quota_dquot_unused",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_XFS_XSTRAT_BYTES] = {
        .name = "system_xfs_xstrat_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "This is a count of bytes of file data flushed out by the XFS flushing daemons.",
    },
    [FAM_XFS_WRITE_BYTES] = {
        .name = "system_xfs_write_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "This is a count of bytes written via write(2) system calls to files "
                "in XFS file systems.",
    },
    [FAM_XFS_READ_BYTES] = {
        .name = "system_xfs_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "This is a count of bytes read via read(2) system calls to files "
                "in XFS file systems.",
    },
    [FAM_XFS_DEFER_RELOG] = {
        .name = "system_xfs_defer_relog",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
};
