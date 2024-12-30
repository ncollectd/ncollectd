// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005,2006 Jason Pepas
// SPDX-FileCopyrightText: Copyright (C) 2012,2013 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Jason Pepas <cell at ices.utexas.edu>
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Cosmin Ioiart <cioiart at gmail.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/avltree.h"

static char *path_proc_nfs;
static char *path_proc_mountstats;

enum {
    FAM_NFS_RPC_CALLS,
    FAM_NFS_RPC_RETRANSMISSIONS,
    FAM_NFS_RPC_AUTHENTICATION_REFRESHES,
    FAM_NFS_REQUESTS,
    FAM_NFS_MAX,
};

static metric_family_t fams[FAM_NFS_MAX] = {
    [FAM_NFS_RPC_CALLS] = {
        .name = "system_nfs_rpc_calls",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of RPC calls performed.",
    },
    [FAM_NFS_RPC_RETRANSMISSIONS] = {
        .name = "system_nfs_rpc_retransmissions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of RPC transmissions performed.",
    },
    [FAM_NFS_RPC_AUTHENTICATION_REFRESHES] = {
        .name = "system_nfs_rpc_authentication_refreshes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of RPC authentication refreshes performed.",
    },
    [FAM_NFS_REQUESTS] = {
        .name = "system_nfs_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of NFS procedures invoked.",
    },
};

enum {
    FAM_NFS_MOUNT_NORMAL_READ_BYTES,
    FAM_NFS_MOUNT_NORMAL_WRITTEN_BYTES,
    FAM_NFS_MOUNT_DIRECT_READ_BYTES,
    FAM_NFS_MOUNT_DIRECT_WRITTEN_BYTES,
    FAM_NFS_MOUNT_SERVER_READ_BYTES,
    FAM_NFS_MOUNT_SERVER_WRITTEN_BYTES,
    FAM_NFS_MOUNT_READ_PAGES,
    FAM_NFS_MOUNT_WRITTEN_PAGES,
    FAM_NFS_MOUNT_FSCACHE_PAGES_READ,
    FAM_NFS_MOUNT_FSCACHE_PAGES_READ_FAIL,
    FAM_NFS_MOUNT_FSCACHE_PAGES_WRITTEN,
    FAM_NFS_MOUNT_FSCACHE_PAGES_WRITTEN_FAIL,
    FAM_NFS_MOUNT_FSCACHE_PAGES_UNCACHED,
    FAM_NFS_MOUNT_INODE_REVALIDATE,
    FAM_NFS_MOUNT_DNODE_REVALIDATE,
    FAM_NFS_MOUNT_DATA_INVALIDATE,
    FAM_NFS_MOUNT_ATTRIBUTE_INVALIDATE,
    FAM_NFS_MOUNT_VFS_OPEN,
    FAM_NFS_MOUNT_VFS_LOOKUP,
    FAM_NFS_MOUNT_VFS_ACCESS,
    FAM_NFS_MOUNT_VFS_UPDATE_PAGE,
    FAM_NFS_MOUNT_VFS_READ_PAGE,
    FAM_NFS_MOUNT_VFS_READ_PAGES,
    FAM_NFS_MOUNT_VFS_WRITE_PAGE,
    FAM_NFS_MOUNT_VFS_WRITE_PAGES,
    FAM_NFS_MOUNT_VFS_GETDENTS,
    FAM_NFS_MOUNT_VFS_SETATTR,
    FAM_NFS_MOUNT_VFS_FLUSH,
    FAM_NFS_MOUNT_VFS_FSYNC,
    FAM_NFS_MOUNT_VFS_LOCK,
    FAM_NFS_MOUNT_VFS_RELEASE,
    FAM_NFS_MOUNT_TRUNCATE,
    FAM_NFS_MOUNT_EXTEND_WRITE,
    FAM_NFS_MOUNT_SILLY_RENAME,
    FAM_NFS_MOUNT_SHORT_READ,
    FAM_NFS_MOUNT_SHORT_WRITE,
    FAM_NFS_MOUNT_DELAY,
    FAM_NFS_MOUNT_PNFS_READ,
    FAM_NFS_MOUNT_PNFS_WRITE,
    FAM_NFS_MOUNT_XPTR_LOCAL_BIND,
    FAM_NFS_MOUNT_XPTR_LOCAL_CONNECT,
    FAM_NFS_MOUNT_XPTR_LOCAL_CONNECT_JIFFIES,
    FAM_NFS_MOUNT_XPTR_LOCAL_IDLE_SECONDS,
    FAM_NFS_MOUNT_XPTR_LOCAL_SENDS,
    FAM_NFS_MOUNT_XPTR_LOCAL_RECVS,
    FAM_NFS_MOUNT_XPTR_LOCAL_BAD_XIDS,
    FAM_NFS_MOUNT_XPTR_LOCAL_REQUEST,
    FAM_NFS_MOUNT_XPTR_LOCAL_BACKLOG,
    FAM_NFS_MOUNT_XPTR_LOCAL_MAX_SLOTS,
    FAM_NFS_MOUNT_XPTR_LOCAL_SENDING_QUEUE,
    FAM_NFS_MOUNT_XPTR_LOCAL_PENDING_QUEUE,
    FAM_NFS_MOUNT_XPTR_UDP_BIND,
    FAM_NFS_MOUNT_XPTR_UDP_SENDS,
    FAM_NFS_MOUNT_XPTR_UDP_RECVS,
    FAM_NFS_MOUNT_XPTR_UDP_BAD_XIDS,
    FAM_NFS_MOUNT_XPTR_UDP_REQUEST,
    FAM_NFS_MOUNT_XPTR_UDP_BACKLOG,
    FAM_NFS_MOUNT_XPTR_UDP_MAX_SLOTS,
    FAM_NFS_MOUNT_XPTR_UDP_SENDING_QUEUE,
    FAM_NFS_MOUNT_XPTR_UDP_PENDING_QUEUE,
    FAM_NFS_MOUNT_XPTR_TCP_BIND,
    FAM_NFS_MOUNT_XPTR_TCP_CONNECT,
    FAM_NFS_MOUNT_XPTR_TCP_CONNECT_JIFFIES,
    FAM_NFS_MOUNT_XPTR_TCP_IDLE_SECONDS,
    FAM_NFS_MOUNT_XPTR_TCP_SENDS,
    FAM_NFS_MOUNT_XPTR_TCP_RECVS,
    FAM_NFS_MOUNT_XPTR_TCP_BAD_XIDS,
    FAM_NFS_MOUNT_XPTR_TCP_REQUEST,
    FAM_NFS_MOUNT_XPTR_TCP_BACKLOG,
    FAM_NFS_MOUNT_XPTR_TCP_MAX_SLOTS,
    FAM_NFS_MOUNT_XPTR_TCP_SENDING_QUEUE,
    FAM_NFS_MOUNT_XPTR_TCP_PENDING_QUEUE,
    FAM_NFS_MOUNT_XPTR_RDMA_BIND,
    FAM_NFS_MOUNT_XPTR_RDMA_CONNECT,
    FAM_NFS_MOUNT_XPTR_RDMA_CONNECT_JIFFIES,
    FAM_NFS_MOUNT_XPTR_RDMA_IDLE_SECONDS,
    FAM_NFS_MOUNT_XPTR_RDMA_SENDS,
    FAM_NFS_MOUNT_XPTR_RDMA_RECVS,
    FAM_NFS_MOUNT_XPTR_RDMA_BAD_XIDS,
    FAM_NFS_MOUNT_XPTR_RDMA_REQUEST,
    FAM_NFS_MOUNT_XPTR_RDMA_BACKLOG ,
    FAM_NFS_MOUNT_XPTR_RDMA_READ_CHUNK,
    FAM_NFS_MOUNT_XPTR_RDMA_WRITE_CHUNK,
    FAM_NFS_MOUNT_XPTR_RDMA_REPLY_CHUNK,
    FAM_NFS_MOUNT_XPTR_RDMA_RDMA_REQUEST,
    FAM_NFS_MOUNT_XPTR_RDMA_RDMA_REPLY,
    FAM_NFS_MOUNT_XPTR_RDMA_PULLUP_COPY,
    FAM_NFS_MOUNT_XPTR_RDMA_FIXUP_COPY,
    FAM_NFS_MOUNT_XPTR_RDMA_HARDWAY_REGISTER,
    FAM_NFS_MOUNT_XPTR_RDMA_FAILED_MARSHAL,
    FAM_NFS_MOUNT_XPTR_RDMA_BAD_REPLY,
    FAM_NFS_MOUNT_XPTR_RDMA_NOMSG_CALL,
    FAM_NFS_MOUNT_XPTR_RDMA_MRS_RECYCLED,
    FAM_NFS_MOUNT_XPTR_RDMA_MRS_ORPHANED,
    FAM_NFS_MOUNT_XPTR_RDMA_MRS_ALLOCATED,
    FAM_NFS_MOUNT_XPTR_RDMA_LOCAL_INV_NEEDED,
    FAM_NFS_MOUNT_XPTR_RDMA_EMPTY_SENDCTX,
    FAM_NFS_MOUNT_XPTR_RDMA_REPLY_WAITS_FOR_SEND,
    FAM_NFS_MOUNT_OPERATION_REQUESTS,
    FAM_NFS_MOUNT_OPERATION_TRANSMISSIONS,
    FAM_NFS_MOUNT_OPERATION_TIMEOUTS,
    FAM_NFS_MOUNT_OPERATION_SEND_BYTES,
    FAM_NFS_MOUNT_OPERATION_RECV_BYTES,
    FAM_NFS_MOUNT_OPERATION_QUEUE_SECONDS,
    FAM_NFS_MOUNT_OPERATION_RESPONSE_SECONDS,
    FAM_NFS_MOUNT_OPERATION_REQUEST_SECONDS,
    FAM_NFS_MOUNT_OPERATION_ERROR,
    FAM_NFS_MOUNT_MAX,
};


static metric_family_t nfs_mountstats_fams[FAM_NFS_MOUNT_MAX] = {
// The number of bytes read by applications via the read(2) system call interfaces.
// The number of bytes written by applications via the write(2) system call interfaces.
    [FAM_NFS_MOUNT_NORMAL_READ_BYTES] = {
        .name = "system_nfs_mount_normal_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of bytes read from the server with simple read().",
    },
    [FAM_NFS_MOUNT_NORMAL_WRITTEN_BYTES] = {
        .name = "system_nfs_mount_normal_written_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of bytes written to the server with simple write().",
    },
// The number of bytes read from files opened with the O_DIRECT flag.
// The number of bytes written to files opened with the O_DIRECT flag.
    [FAM_NFS_MOUNT_DIRECT_READ_BYTES] = {
        .name = "system_nfs_mount_direct_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of bytes read from the server from files opened with the O_DIRECT flag."
    },
    [FAM_NFS_MOUNT_DIRECT_WRITTEN_BYTES] = {
        .name = "system_nfs_mount_direct_written_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of bytes written to the server to files opened with the O_DIRECT flag."
    },
// The number of payload bytes read from the server by the NFS client via an NFS READ request.
// The number of payload bytes written to the server by the NFS client via an NFS WRITE request.
    [FAM_NFS_MOUNT_SERVER_READ_BYTES] = {
        .name = "system_nfs_mount_server_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of bytes read from the NFS server (regardless of how).",
    },
    [FAM_NFS_MOUNT_SERVER_WRITTEN_BYTES] = {
        .name = "system_nfs_mount_server_written_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of bytes written to the NFS server (regardless of how).",
    },
// These count the number of pages read via nfs_readpage(), nfs_readpages()
// These count the number of pages written via nfs_writepage(), nfs_writepages()
    [FAM_NFS_MOUNT_READ_PAGES] = {
        .name = "system_nfs_mount_read_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of pages read via directly mmap()'d files.",
    },
    [FAM_NFS_MOUNT_WRITTEN_PAGES] = {
        .name = "system_nfs_mount_written_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of pages written via directly mmap()'d files.",
    },
    [FAM_NFS_MOUNT_FSCACHE_PAGES_READ] = {
        .name = "system_nfs_mount_fscache_pages_read",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of pages read from the cache.",
    },
    [FAM_NFS_MOUNT_FSCACHE_PAGES_READ_FAIL] = {
        .name = "system_nfs_mount_fscache_pages_read_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of failed reads from  the cache.",
    },
    [FAM_NFS_MOUNT_FSCACHE_PAGES_WRITTEN] = {
        .name = "system_nfs_mount_fscache_pages_written",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of pages written to the cache.",
    },
    [FAM_NFS_MOUNT_FSCACHE_PAGES_WRITTEN_FAIL] = {
        .name = "system_nfs_mount_fscache_pages_written_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of failed writtens to the cache.",
    },
    [FAM_NFS_MOUNT_FSCACHE_PAGES_UNCACHED] = {
        .name = "system_nfs_mount_fscache_pages_uncached",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number for unached pages from the cache.",
    },
    [FAM_NFS_MOUNT_INODE_REVALIDATE] = {
        .name = "system_nfs_mount_inode_revalidate",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many times the cached inode attributes have to be "
                "re-validated from the server.",
    },
    [FAM_NFS_MOUNT_DNODE_REVALIDATE] = {
        .name = "system_nfs_mount_dnode_revalidate",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many times cached dentry nodes have to be "
                "re-validated from the server.",
    },
    [FAM_NFS_MOUNT_DATA_INVALIDATE] = {
        .name = "system_nfs_mount_data_invalidate",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many times an inode had its cached data thrown out.",
    },
    [FAM_NFS_MOUNT_ATTRIBUTE_INVALIDATE] = {
        .name = "system_nfs_mount_attribute_invalidate",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many times an inode has had cached inode attributes invalidated.",
    },
    [FAM_NFS_MOUNT_VFS_OPEN] = {
        .name = "system_nfs_mount_vfs_open",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many times files or directories have been opened.",
    },
    [FAM_NFS_MOUNT_VFS_LOOKUP] = {
        .name = "system_nfs_mount_vfs_lookup",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many name lookups in directories there have been.",
    },
    [FAM_NFS_MOUNT_VFS_ACCESS] = {
        .name = "system_nfs_mount_vfs_access",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many times permissions have been checked "
                "via the internal equivalent of access().",
    },
    [FAM_NFS_MOUNT_VFS_UPDATE_PAGE] = {
        .name = "system_nfs_mount_vfs_update_page",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of updates to pages.",
    },
    [FAM_NFS_MOUNT_VFS_READ_PAGE] = {
        .name = "system_nfs_mount_vfs_read_page",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of pages read via nfs_readpage()"
    },
    [FAM_NFS_MOUNT_VFS_READ_PAGES] = {
        .name = "system_nfs_mount_vfs_read_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many times a group of pages have been read.",
    },
    [FAM_NFS_MOUNT_VFS_WRITE_PAGE] = {
        .name = "system_nfs_mount_vfs_write_page",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of pages written via nfs_writepage()",
    },
    [FAM_NFS_MOUNT_VFS_WRITE_PAGES] = {
        .name = "system_nfs_mount_vfs_write_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "count of grouped page writes.",
    },
    [FAM_NFS_MOUNT_VFS_GETDENTS] = {
        .name = "system_nfs_mount_vfs_getdents",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many times get directory entries was called.",
    },
    [FAM_NFS_MOUNT_VFS_SETATTR] = {
        .name = "system_nfs_mount_vfs_setattr",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many times we've set attributes on inodes.",
    },
    [FAM_NFS_MOUNT_VFS_FLUSH] = {
        .name = "system_nfs_mount_vfs_flush",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many times pending writes have been forcefully flushed to the server.",
    },
    [FAM_NFS_MOUNT_VFS_FSYNC] = {
        .name = "system_nfs_mount_vfs_fsync",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many times fsync() has been called on directories and files.",
    },
    [FAM_NFS_MOUNT_VFS_LOCK] = {
        .name = "system_nfs_mount_vfs_lock",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many times have tried to lock (parts of) a file.",
    },
    [FAM_NFS_MOUNT_VFS_RELEASE] = {
        .name = "system_nfs_mount_vfs_release",
        .type = METRIC_TYPE_COUNTER,
        .help = "how many times files have been closed and released.",
    },
    [FAM_NFS_MOUNT_TRUNCATE] = {
        .name = "system_nfs_mount_truncate",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many times files have had their size truncated.",
    },
    [FAM_NFS_MOUNT_EXTEND_WRITE] = {
        .name = "system_nfs_mount_extend_write",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many times a file has been grown because you're writing beyond "
                "the existing end of the file.",
    },
    [FAM_NFS_MOUNT_SILLY_RENAME] = {
        .name = "system_nfs_mount_silly_rename",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many times you removed a file while it was still open by some process.",
    },
    [FAM_NFS_MOUNT_SHORT_READ] = {
        .name = "system_nfs_mount_short_read",
        .type = METRIC_TYPE_COUNTER,
        .help = "The NFS server gave us less data than we asked for "
                "when we tried to read something.",
    },
    [FAM_NFS_MOUNT_SHORT_WRITE] = {
        .name = "system_nfs_mount_short_write",
        .type = METRIC_TYPE_COUNTER,
        .help = "The NFS server wrote less data than we asked it to.",
    },
    [FAM_NFS_MOUNT_DELAY] = {
        .name = "system_nfs_mount_delay",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many times the NFS server told us EJUKEBOX.",
    },
    [FAM_NFS_MOUNT_PNFS_READ] = {
        .name = "system_nfs_mount_pnfs_read",
        .type = METRIC_TYPE_COUNTER,
        .help = "count the number of NFS v4.1+ pNFS reads.",
    },
    [FAM_NFS_MOUNT_PNFS_WRITE] = {
        .name = "system_nfs_mount_pnfs_write",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count the number of NFS v5.1+ pNFS writes.",
    },
    [FAM_NFS_MOUNT_XPTR_LOCAL_BIND] = {
        .name = "system_nfs_mount_xptr_local_bind",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_LOCAL_CONNECT] = {
        .name = "system_nfs_mount_xptr_local_connect",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_LOCAL_CONNECT_JIFFIES] = {
        .name = "system_nfs_mount_xptr_local_connect_jiffies",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_LOCAL_IDLE_SECONDS] = {
        .name = "system_nfs_mount_xptr_local_idle_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_LOCAL_SENDS] = {
        .name = "system_nfs_mount_xptr_local_sends",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_LOCAL_RECVS] = {
        .name = "system_nfs_mount_xptr_local_recvs",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_LOCAL_BAD_XIDS] = {
        .name = "system_nfs_mount_xptr_local_bad_xids",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_LOCAL_REQUEST] = {
        .name = "system_nfs_mount_xptr_local_request",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_LOCAL_BACKLOG] = {
        .name = "system_nfs_mount_xptr_local_backlog",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_LOCAL_MAX_SLOTS] = {
        .name = "system_nfs_mount_xptr_local_max_slots",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_LOCAL_SENDING_QUEUE] = {
        .name = "system_nfs_mount_xptr_local_sending_queue",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_LOCAL_PENDING_QUEUE] = {
        .name = "system_nfs_mount_xptr_local_pending_queue",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_UDP_BIND] = {
        .name = "system_nfs_mount_xptr_udp_bind",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_UDP_SENDS] = {
        .name = "system_nfs_mount_xptr_udp_sends",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_UDP_RECVS] = {
        .name = "system_nfs_mount_xptr_udp_recvs",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_UDP_BAD_XIDS] = {
        .name = "system_nfs_mount_xptr_udp_bad_xids",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_UDP_REQUEST] = {
        .name = "system_nfs_mount_xptr_udp_request",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_UDP_BACKLOG] = {
        .name = "system_nfs_mount_xptr_udp_backlog",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_UDP_MAX_SLOTS] = {
        .name = "system_nfs_mount_xptr_udp_max_slots",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_UDP_SENDING_QUEUE] = {
        .name = "system_nfs_mount_xptr_udp_sending_queue",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_UDP_PENDING_QUEUE] = {
        .name = "system_nfs_mount_xptr_udp_pending_queue",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_TCP_BIND] = {
        .name = "system_nfs_mount_xptr_tcp_bind",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_TCP_CONNECT] = {
        .name = "system_nfs_mount_xptr_tcp_connect",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_TCP_CONNECT_JIFFIES] = {
        .name = "system_nfs_mount_xptr_tcp_connect_jiffies",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_TCP_IDLE_SECONDS] = {
        .name = "system_nfs_mount_xptr_tcp_idle_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_TCP_SENDS] = {
        .name = "system_nfs_mount_xptr_tcp_sends",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_TCP_RECVS] = {
        .name = "system_nfs_mount_xptr_tcp_recvs",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_TCP_BAD_XIDS] = {
        .name = "system_nfs_mount_xptr_tcp_bad_xids",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_TCP_REQUEST] = {
        .name = "system_nfs_mount_xptr_tcp_request",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_TCP_BACKLOG] = {
        .name = "system_nfs_mount_xptr_tcp_backlog",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_TCP_MAX_SLOTS] = {
        .name = "system_nfs_mount_xptr_tcp_max_slots",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_TCP_SENDING_QUEUE] = {
        .name = "system_nfs_mount_xptr_tcp_sending_queue",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_TCP_PENDING_QUEUE] = {
        .name = "system_nfs_mount_xptr_tcp_pending_queue",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_BIND] = {
        .name = "system_nfs_mount_xptr_rdma_bind",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_CONNECT] = {
        .name = "system_nfs_mount_xptr_rdma_connect",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_CONNECT_JIFFIES] = {
        .name = "system_nfs_mount_xptr_rdma_connect_jiffies",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_IDLE_SECONDS] = {
        .name = "system_nfs_mount_xptr_rdma_idle_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_SENDS] = {
        .name = "system_nfs_mount_xptr_rdma_sends",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_RECVS] = {
        .name = "system_nfs_mount_xptr_rdma_recvs",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_BAD_XIDS] = {
        .name = "system_nfs_mount_xptr_rdma_bad_xids",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_REQUEST] = {
        .name = "system_nfs_mount_xptr_rdma_request",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_BACKLOG ] = {
        .name = "system_nfs_mount_xptr_rdma_backlog ",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_READ_CHUNK] = {
        .name = "system_nfs_mount_xptr_rdma_read_chunk",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_WRITE_CHUNK] = {
        .name = "system_nfs_mount_xptr_rdma_write_chunk",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_REPLY_CHUNK] = {
        .name = "system_nfs_mount_xptr_rdma_reply_chunk",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_RDMA_REQUEST] = {
        .name = "system_nfs_mount_xptr_rdma_rdma_request",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_RDMA_REPLY] = {
        .name = "system_nfs_mount_xptr_rdma_rdma_reply",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_PULLUP_COPY] = {
        .name = "system_nfs_mount_xptr_rdma_pullup_copy",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_FIXUP_COPY] = {
        .name = "system_nfs_mount_xptr_rdma_fixup_copy",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_HARDWAY_REGISTER] = {
        .name = "system_nfs_mount_xptr_rdma_hardway_register",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_FAILED_MARSHAL] = {
        .name = "system_nfs_mount_xptr_rdma_failed_marshal",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_BAD_REPLY] = {
        .name = "system_nfs_mount_xptr_rdma_bad_reply",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_NOMSG_CALL] = {
        .name = "system_nfs_mount_xptr_rdma_nomsg_call",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_MRS_RECYCLED] = {
        .name = "system_nfs_mount_xptr_rdma_mrs_recycled",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_MRS_ORPHANED] = {
        .name = "system_nfs_mount_xptr_rdma_mrs_orphaned",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_MRS_ALLOCATED] = {
        .name = "system_nfs_mount_xptr_rdma_mrs_allocated",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_LOCAL_INV_NEEDED] = {
        .name = "system_nfs_mount_xptr_rdma_local_inv_needed",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_EMPTY_SENDCTX] = {
        .name = "system_nfs_mount_xptr_rdma_empty_sendctx",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_XPTR_RDMA_REPLY_WAITS_FOR_SEND] = {
        .name = "system_nfs_mount_xptr_rdma_reply_waits_for_send",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NFS_MOUNT_OPERATION_REQUESTS] = {
        .name = "system_nfs_mount_operation_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many requests we've done for this operation.",
    },
    [FAM_NFS_MOUNT_OPERATION_TRANSMISSIONS] = {
        .name = "system_nfs_mount_operation_transmissions",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many times we've actually transmitted a RPC request for this operation.",
    },
    [FAM_NFS_MOUNT_OPERATION_TIMEOUTS] = {
        .name = "system_nfs_mount_operation_timeouts",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many times a request has had a major timeout.",
    },
    [FAM_NFS_MOUNT_OPERATION_SEND_BYTES] = {
        .name = "system_nfs_mount_operation_send_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total bytes send, his includes not just the RPC payload "
                "but also the RPC headers.",
    },
    [FAM_NFS_MOUNT_OPERATION_RECV_BYTES] = {
        .name = "system_nfs_mount_operation_recv_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total bytes received, his includes not just the RPC payload "
                "but also the RPC headers.",
    },
    [FAM_NFS_MOUNT_OPERATION_QUEUE_SECONDS] = {
        .name = "system_nfs_mount_operation_queue_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "How long (in seconds) all requests spent queued for "
                "transmission before they were sent.",
    },
    [FAM_NFS_MOUNT_OPERATION_RESPONSE_SECONDS] = {
        .name = "system_nfs_mount_operation_response_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "How long (in seconds) it took to get a reply back after "
                "the request was transmitted.",
    },
    [FAM_NFS_MOUNT_OPERATION_REQUEST_SECONDS] = {
        .name = "system_nfs_mount_operation_request_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "How long (in seconds) all requests took from when they were initially queued "
                "to when they were completely handled.",
    },
    [FAM_NFS_MOUNT_OPERATION_ERROR] = {
        .name = "system_nfs_mount_operation_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "The count of operations that complete with tk_status < 0, "
                "usually indicate error conditions.",
    },
};

static const char *nfs2_procedures_names[] = {
    "null", "getattr", "setattr", "root",   "lookup",  "readlink",
    "read", "wrcache", "write",   "create", "remove",  "rename",
    "link", "symlink", "mkdir",   "rmdir",  "readdir", "fsstat"};
static size_t nfs2_procedures_names_num = STATIC_ARRAY_SIZE(nfs2_procedures_names);

static const char *nfs3_procedures_names[] = {
    "null",   "getattr", "setattr",  "lookup", "access",  "readlink",
    "read",   "write",   "create",   "mkdir",  "symlink", "mknod",
    "remove", "rmdir",   "rename",   "link",   "readdir", "readdirplus",
    "fsstat", "fsinfo",  "pathconf", "commit"
};
static size_t nfs3_procedures_names_num = STATIC_ARRAY_SIZE(nfs3_procedures_names);

static const char *nfs4_procedures_names[] = {
    "null",           "read",                "write",                "commit",
    "open",           "open_confirm",        "open_noattr",          "open_downgrade",
    "close",          "setattr",             "fsinfo",               "renew",
    "setclientid",    "setclientid_confirm", "lock",                 "lockt",
    "locku",          "access",              "getattr",              "lookup",
    "lookup_root",    "remove",              "rename",               "link",
    "symlink",        "create",              "pathconf",             "statfs",
    "readlink",       "readdir",             "server_caps",          "delegreturn",
    "getacl",         "setacl",              "fs_locations",         "release_lockowner",
    "secinfo",        "fsid_present",
     /* NFS 4.1 */
    "exchange_id",    "create_session",      "destroy_session",      "sequence",
    "get_lease_time", "reclaim_complete",    "layoutget",            "getdeviceinfo",
    "layoutcommit",   "layoutreturn",        "secinfo_no_name",      "test_stateid",
    "free_stateid",   "getdevicelist",       "bind_conn_to_session", "destroy_clientid",
     /* NFS 4.2 */
    "seek",           "allocate",            "deallocate",           "layoutstats",
    "clone",          "copy",                "offload_cancel",       "lookupp",
    "layouterror",    "copy_notify",
    /* xattr support (RFC8726) */
    "getxattr",       "setxattr",            "listxattrs",           "removexattr",
    "read_plus"
};
static size_t nfs4_procedures_names_num = STATIC_ARRAY_SIZE(nfs4_procedures_names);

enum {
    COLLECT_NFS_V2      = (1 <<  0),
    COLLECT_NFS_V3      = (1 <<  1),
    COLLECT_NFS_V4      = (1 <<  2),
    COLLECT_MOUNT_STATS = (1 <<  3)
};

static cf_flags_t nfs_flags[] = {
    { "nfs-v2",      COLLECT_NFS_V2      },
    { "nfs-v3",      COLLECT_NFS_V3      },
    { "nfs-v4",      COLLECT_NFS_V4      },
    { "mount-stats", COLLECT_MOUNT_STATS }
};
static size_t nfs_flags_size = STATIC_ARRAY_SIZE(nfs_flags);

static uint64_t flags = COLLECT_NFS_V2 | COLLECT_NFS_V3 | COLLECT_NFS_V4 | COLLECT_MOUNT_STATS;

typedef struct {
    int64_t age;
    metric_family_t fams[FAM_NFS_MOUNT_MAX];
} nfs_mountstats_t;

static void nfs_mountstats_reset(nfs_mountstats_t *nfs)
{
    if (nfs == NULL)
        return;

    for (size_t i=0; i < FAM_NFS_MOUNT_MAX; i++) {
        if (nfs->fams[i].metric.num == 0)
            continue;
        metric_family_metric_reset(&nfs->fams[i]);
    }
}

static void nfs_read_mountstats_events(char *line, nfs_mountstats_t *nfs, label_set_t *labels)
{
    char *fields[28];
    int fields_num = strsplit(line, fields, STATIC_ARRAY_SIZE(fields));
    if (fields_num < 29)
        return;

    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_INODE_REVALIDATE],
                         VALUE_COUNTER(atoull(fields[1])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_DNODE_REVALIDATE],
                         VALUE_COUNTER(atoull(fields[2])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_DATA_INVALIDATE],
                         VALUE_COUNTER(atoull(fields[3])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_ATTRIBUTE_INVALIDATE],
                         VALUE_COUNTER(atoull(fields[4])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_VFS_OPEN],
                         VALUE_COUNTER(atoull(fields[5])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_VFS_LOOKUP],
                         VALUE_COUNTER(atoull(fields[6])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_VFS_ACCESS],
                         VALUE_COUNTER(atoull(fields[7])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_VFS_UPDATE_PAGE],
                         VALUE_COUNTER(atoull(fields[8])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_VFS_READ_PAGE],
                         VALUE_COUNTER(atoull(fields[9])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_VFS_READ_PAGES],
                         VALUE_COUNTER(atoull(fields[10])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_VFS_WRITE_PAGE],
                         VALUE_COUNTER(atoull(fields[11])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_VFS_WRITE_PAGES],
                         VALUE_COUNTER(atoull(fields[12])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_VFS_GETDENTS],
                         VALUE_COUNTER(atoull(fields[13])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_VFS_SETATTR],
                         VALUE_COUNTER(atoull(fields[14])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_VFS_FLUSH],
                         VALUE_COUNTER(atoull(fields[15])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_VFS_FSYNC],
                         VALUE_COUNTER(atoull(fields[16])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_VFS_LOCK],
                         VALUE_COUNTER(atoull(fields[17])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_VFS_RELEASE],
                         VALUE_COUNTER(atoull(fields[18])), labels, NULL);
    // no data in NFSIOS_CONGESTIONWAIT
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_TRUNCATE],
                         VALUE_COUNTER(atoull(fields[20])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_EXTEND_WRITE],
                         VALUE_COUNTER(atoull(fields[21])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_SILLY_RENAME],
                         VALUE_COUNTER(atoull(fields[22])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_SHORT_READ],
                         VALUE_COUNTER(atoull(fields[23])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_SHORT_WRITE],
                         VALUE_COUNTER(atoull(fields[24])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_DELAY],
                         VALUE_COUNTER(atoull(fields[25])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_PNFS_READ],
                         VALUE_COUNTER(atoull(fields[26])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_PNFS_WRITE],
                         VALUE_COUNTER(atoull(fields[27])), labels, NULL);

}

static void nfs_read_mountstats_bytes(char *line, nfs_mountstats_t *nfs, label_set_t *labels)
{
    char *fields[9];
    int fields_num = strsplit(line, fields, STATIC_ARRAY_SIZE(fields));
    if (fields_num < 9)
        return;

    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_NORMAL_READ_BYTES],
                         VALUE_COUNTER(atoull(fields[1])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_NORMAL_WRITTEN_BYTES],
                         VALUE_COUNTER(atoull(fields[2])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_DIRECT_READ_BYTES],
                         VALUE_COUNTER(atoull(fields[3])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_DIRECT_WRITTEN_BYTES],
                         VALUE_COUNTER(atoull(fields[4])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_SERVER_READ_BYTES],
                         VALUE_COUNTER(atoull(fields[5])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_SERVER_WRITTEN_BYTES],
                         VALUE_COUNTER(atoull(fields[6])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_READ_PAGES],
                         VALUE_COUNTER(atoull(fields[7])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_WRITTEN_PAGES],
                         VALUE_COUNTER(atoull(fields[8])), labels, NULL);
}

static void nfs_read_mountstats_fsc(char *line, nfs_mountstats_t *nfs, label_set_t *labels)
{
    char *fields[6];
    int fields_num = strsplit(line, fields, STATIC_ARRAY_SIZE(fields));
    if (fields_num < 6)
        return;

    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_FSCACHE_PAGES_READ],
                         VALUE_COUNTER(atoull(fields[1])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_FSCACHE_PAGES_READ_FAIL],
                         VALUE_COUNTER(atoull(fields[2])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_FSCACHE_PAGES_WRITTEN],
                         VALUE_COUNTER(atoull(fields[3])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_FSCACHE_PAGES_WRITTEN_FAIL],
                         VALUE_COUNTER(atoull(fields[4])), labels, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_FSCACHE_PAGES_UNCACHED],
                         VALUE_COUNTER(atoull(fields[5])), labels, NULL);

}

static void nfs_read_mountstats_xprt(char *line, nfs_mountstats_t *nfs, label_set_t *labels)
{
    char *fields[29];
    int fields_num = strsplit(line, fields, STATIC_ARRAY_SIZE(fields));
    if (fields_num < 2)
        return;

    if (strcmp(fields[1], "local") == 0) {
        if (fields_num < 11)
            return;
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_LOCAL_BIND],
                             VALUE_COUNTER(atoull(fields[2])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_LOCAL_CONNECT],
                             VALUE_COUNTER(atoull(fields[3])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_LOCAL_CONNECT_JIFFIES],
                             VALUE_COUNTER(atoull(fields[4])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_LOCAL_IDLE_SECONDS],
                             VALUE_COUNTER(atoull(fields[5])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_LOCAL_SENDS],
                             VALUE_COUNTER(atoull(fields[6])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_LOCAL_RECVS],
                             VALUE_COUNTER(atoull(fields[7])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_LOCAL_BAD_XIDS],
                             VALUE_COUNTER(atoull(fields[8])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_LOCAL_REQUEST],
                             VALUE_COUNTER(atoull(fields[9])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_LOCAL_BACKLOG],
                             VALUE_COUNTER(atoull(fields[10])), labels, NULL);
        if (fields_num < 14)
            return;
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_LOCAL_MAX_SLOTS],
                             VALUE_GAUGE((double)atoull(fields[11])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_LOCAL_SENDING_QUEUE],
                             VALUE_COUNTER(atoull(fields[12])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_LOCAL_PENDING_QUEUE],
                             VALUE_COUNTER(atoull(fields[13])), labels, NULL);
    } else if (strcmp(fields[1], "tcp") == 0) {
        if (fields_num < 12)
            return;
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_TCP_BIND],
                             VALUE_COUNTER(atoull(fields[3])), labels,
                             &(label_pair_const_t){.name="port", .value=fields[2]}, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_TCP_CONNECT],
                             VALUE_COUNTER(atoull(fields[4])), labels,
                             &(label_pair_const_t){.name="port", .value=fields[2]}, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_TCP_CONNECT_JIFFIES],
                             VALUE_COUNTER(atoull(fields[5])), labels,
                             &(label_pair_const_t){.name="port", .value=fields[2]}, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_TCP_IDLE_SECONDS],
                             VALUE_COUNTER(atoull(fields[6])), labels,
                             &(label_pair_const_t){.name="port", .value=fields[2]}, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_TCP_SENDS],
                             VALUE_COUNTER(atoull(fields[7])), labels,
                             &(label_pair_const_t){.name="port", .value=fields[2]}, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_TCP_RECVS],
                             VALUE_COUNTER(atoull(fields[8])), labels,
                             &(label_pair_const_t){.name="port", .value=fields[2]}, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_TCP_BAD_XIDS],
                             VALUE_COUNTER(atoull(fields[9])), labels,
                             &(label_pair_const_t){.name="port", .value=fields[2]}, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_TCP_REQUEST],
                             VALUE_COUNTER(atoull(fields[10])), labels,
                             &(label_pair_const_t){.name="port", .value=fields[2]}, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_TCP_BACKLOG],
                             VALUE_COUNTER(atoull(fields[11])), labels,
                             &(label_pair_const_t){.name="port", .value=fields[2]}, NULL);
        if (fields_num < 15)
            return;
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_TCP_MAX_SLOTS],
                             VALUE_GAUGE((double)(atoull(fields[12]))), labels,
                             &(label_pair_const_t){.name="port", .value=fields[2]}, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_TCP_SENDING_QUEUE],
                             VALUE_COUNTER(atoull(fields[13])), labels,
                             &(label_pair_const_t){.name="port", .value=fields[2]}, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_TCP_PENDING_QUEUE],
                             VALUE_COUNTER(atoull(fields[14])), labels,
                             &(label_pair_const_t){.name="port", .value=fields[2]}, NULL);
    } else if (strcmp(fields[1], "udp") == 0) {
        if (fields_num < 9)
            return;
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_UDP_BIND],
                             VALUE_COUNTER(atoull(fields[3])), labels,
                             &(label_pair_const_t){.name="port", .value=fields[2]}, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_UDP_SENDS],
                             VALUE_COUNTER(atoull(fields[4])), labels,
                             &(label_pair_const_t){.name="port", .value=fields[2]}, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_UDP_RECVS],
                             VALUE_COUNTER(atoull(fields[5])), labels,
                             &(label_pair_const_t){.name="port", .value=fields[2]}, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_UDP_BAD_XIDS],
                             VALUE_COUNTER(atoull(fields[6])), labels,
                             &(label_pair_const_t){.name="port", .value=fields[2]}, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_UDP_REQUEST],
                             VALUE_COUNTER(atoull(fields[7])), labels,
                             &(label_pair_const_t){.name="port", .value=fields[2]}, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_UDP_BACKLOG],
                             VALUE_COUNTER(atoull(fields[8])), labels,
                             &(label_pair_const_t){.name="port", .value=fields[2]}, NULL);
        if (fields_num < 12)
            return;
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_UDP_MAX_SLOTS],
                             VALUE_GAUGE((double)atoull(fields[9])), labels,
                             &(label_pair_const_t){.name="port", .value=fields[2]}, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_UDP_SENDING_QUEUE],
                             VALUE_COUNTER(atoull(fields[10])), labels,
                             &(label_pair_const_t){.name="port", .value=fields[2]}, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_UDP_PENDING_QUEUE],
                             VALUE_COUNTER(atoull(fields[11])), labels,
                             &(label_pair_const_t){.name="port", .value=fields[2]}, NULL);
    } else if (strcmp(fields[1], "rdma") == 0) {
        if (fields_num < 12)
            return;
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_BIND],
                             VALUE_COUNTER(atoull(fields[3])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_CONNECT],
                             VALUE_COUNTER(atoull(fields[4])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_CONNECT_JIFFIES],
                             VALUE_COUNTER(atoull(fields[5])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_IDLE_SECONDS],
                             VALUE_COUNTER(atoull(fields[6])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_SENDS],
                             VALUE_COUNTER(atoull(fields[7])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_RECVS],
                             VALUE_COUNTER(atoull(fields[8])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_BAD_XIDS],
                             VALUE_COUNTER(atoull(fields[9])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_REQUEST],
                             VALUE_COUNTER(atoull(fields[10])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_BACKLOG ],
                             VALUE_COUNTER(atoull(fields[11])), labels, NULL);
        if (fields_num < 29)
            return;
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_READ_CHUNK],
                             VALUE_COUNTER(atoull(fields[12])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_WRITE_CHUNK],
                             VALUE_COUNTER(atoull(fields[13])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_REPLY_CHUNK],
                             VALUE_COUNTER(atoull(fields[14])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_RDMA_REQUEST],
                             VALUE_COUNTER(atoull(fields[15])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_RDMA_REPLY],
                             VALUE_COUNTER(atoull(fields[16])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_PULLUP_COPY],
                             VALUE_COUNTER(atoull(fields[17])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_FIXUP_COPY],
                             VALUE_COUNTER(atoull(fields[18])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_HARDWAY_REGISTER],
                             VALUE_COUNTER(atoull(fields[19])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_FAILED_MARSHAL],
                             VALUE_COUNTER(atoull(fields[20])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_BAD_REPLY],
                             VALUE_COUNTER(atoull(fields[21])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_NOMSG_CALL],
                             VALUE_COUNTER(atoull(fields[22])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_MRS_RECYCLED],
                             VALUE_COUNTER(atoull(fields[23])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_MRS_ORPHANED],
                             VALUE_COUNTER(atoull(fields[24])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_MRS_ALLOCATED],
                             VALUE_COUNTER(atoull(fields[25])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_LOCAL_INV_NEEDED],
                             VALUE_COUNTER(atoull(fields[26])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_EMPTY_SENDCTX],
                             VALUE_COUNTER(atoull(fields[27])), labels, NULL);
        metric_family_append(&nfs->fams[FAM_NFS_MOUNT_XPTR_RDMA_REPLY_WAITS_FOR_SEND],
                             VALUE_COUNTER(atoull(fields[28])), labels, NULL);
    }
}

static void nfs_read_mountstats_ops(char *line, nfs_mountstats_t *nfs, label_set_t *labels)
{
    char *fields[10];
    int fields_num = strsplit(line, fields, STATIC_ARRAY_SIZE(fields));

    if (fields_num < 9)
        return;

    size_t op_size = strlen(fields[0]);
    if ((op_size > 0) && (fields[0][op_size-1] == ':'))
        fields[0][op_size-1] = '\0';
    for (size_t i = 0; i < op_size; i++)
        fields[0][i] = tolower(fields[0][i]);

    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_OPERATION_REQUESTS],
                         VALUE_COUNTER(atoull(fields[1])), labels,
                         &(label_pair_const_t){.name="operation", .value=fields[0]}, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_OPERATION_TRANSMISSIONS],
                         VALUE_COUNTER(atoull(fields[2])), labels,
                         &(label_pair_const_t){.name="operation", .value=fields[0]}, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_OPERATION_TIMEOUTS],
                         VALUE_COUNTER(atoull(fields[3])), labels,
                         &(label_pair_const_t){.name="operation", .value=fields[0]}, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_OPERATION_SEND_BYTES],
                         VALUE_COUNTER(atoull(fields[4])), labels,
                         &(label_pair_const_t){.name="operation", .value=fields[0]}, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_OPERATION_RECV_BYTES],
                         VALUE_COUNTER(atoull(fields[5])), labels,
                         &(label_pair_const_t){.name="operation", .value=fields[0]}, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_OPERATION_QUEUE_SECONDS],
                         VALUE_COUNTER_FLOAT64(((double)atoull(fields[6]))/1000.0), labels,
                         &(label_pair_const_t){.name="operation", .value=fields[0]}, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_OPERATION_RESPONSE_SECONDS],
                         VALUE_COUNTER_FLOAT64(((double)atoull(fields[7]))/1000.0), labels,
                         &(label_pair_const_t){.name="operation", .value=fields[0]}, NULL);
    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_OPERATION_REQUEST_SECONDS],
                         VALUE_COUNTER_FLOAT64(((double)atoull(fields[8]))/1000.0), labels,
                         &(label_pair_const_t){.name="operation", .value=fields[0]}, NULL);
    if (fields_num < 10)
        return;

    metric_family_append(&nfs->fams[FAM_NFS_MOUNT_OPERATION_ERROR],
                         VALUE_COUNTER(atoull(fields[9])), labels,
                         &(label_pair_const_t){.name="operation", .value=fields[0]}, NULL);

}

static int nfs_read_mountstats_device(FILE *fh, nfs_mountstats_t *nfs, label_set_t *labels)
{
    char line[1024];
    while (fgets(line, sizeof(line), fh) != NULL) {
        char *str = line;
        while ((*str == ' ') || (*str == '\t')) str++;
        if (strncmp(str, "age:", strlen("age:")) == 0) {
            char *fields[2];
            int fields_num = strsplit(str, fields, STATIC_ARRAY_SIZE(fields));
            if (fields_num < 2)
                continue;
            nfs->age = atoll(fields[1]);
        } else if (strncmp(str, "events:", strlen("events:")) == 0) {
            nfs_read_mountstats_events(str, nfs, labels);
        } else if (strncmp(str, "bytes:", strlen("bytes:")) == 0) {
            nfs_read_mountstats_bytes(str, nfs, labels);
        } else if (strncmp(str, "fsc:", strlen("fsc:")) == 0) {
            nfs_read_mountstats_fsc(str, nfs, labels);
        } else if (strncmp(str, "xprt:", strlen("xprt:")) == 0) {
            nfs_read_mountstats_xprt(str, nfs, labels);
        } else if (strncmp(str, "per-op statistics", strlen("per-op statistics")) == 0) {
            char *rline = NULL;
            while ((rline = fgets(line, sizeof(line), fh)) != NULL) {
                if (line[0] == '\n')
                    return 0;
                nfs_read_mountstats_ops(line, nfs, labels);
            }
            if (rline == NULL)
                return 0;
        } else if (line[0] == '\n') {
            return 0;
        }
    }

    return 0;
}

static int nfs_read_mountstats(void)
{
    FILE *fh = fopen(path_proc_mountstats, "r");
    if (unlikely(fh == NULL)) {
        PLUGIN_ERROR("Unable to open '%s': %s", path_proc_mountstats, STRERRNO);
        return -1;
    }

    c_avl_tree_t *tree = c_avl_create((int (*)(const void *, const void *))strcmp);
    if (unlikely(tree == NULL)) {
        PLUGIN_ERROR("Failed to create avl tree");
        fclose(fh);
        return -1;
    }

    char line[1024];
    while (fgets(line, sizeof(line), fh) != NULL) {
        if (strncmp(line, "device", strlen("device")) == 0) {
            char *str = strstr(line, " with fstype ");
            if (str == NULL)
                continue;
            str += strlen(" with fstype ");
            while ((*str == ' ') || (*str == '\t')) str++;
            if (*str == '\0')
                continue;
            if (strncmp(str, "nfs", strlen("nfs")) != 0)
                continue;

            if ((str[3] != '2') && (str[3] != '3') && (str[3] != '4') &&
                (str[3] != ' ') && (str[3] != '\n') && (str[3] != '\0'))
                continue;

            char *mount = strstr(line, " mounted on ");
            if (mount == NULL)
                continue;
            mount += strlen(" mounted on ");
            while ((*mount == ' ') || (*mount == '\t')) str++;
            if (*mount == '\0')
                continue;
            str = mount;
            while ((*str != ' ') && (*str != '\0')) str++;
            size_t mount_size = str - mount;

            char *export = line + strlen("device ");
            while ((*export == ' ') || (*export == '\t')) str++;
            if (*export == '\0')
                continue;
            str = export;
            while ((*str != ' ') && (*str != '\0')) str++;
            size_t export_size = str - export;

            nfs_mountstats_t *nfs = malloc(sizeof(*nfs));
            if (nfs == NULL) {
                PLUGIN_ERROR("malloc failed");
                continue;
            }
            memcpy(nfs->fams, nfs_mountstats_fams, FAM_NFS_MOUNT_MAX*sizeof(nfs->fams[0]));
            nfs->age = INT64_MAX;

            export[export_size] = '\0';
            mount[mount_size] = '\0';

            label_set_t labels = {
                .num = 2,
                .ptr = (label_pair_t[]) {
                    {.name = "export", .value = export},
                    {.name = "mount",  .value = mount },
                }
            };

            nfs_read_mountstats_device(fh, nfs, &labels);

            nfs_mountstats_t *nfs_found = NULL;
            int status = c_avl_get(tree, mount, (void *)&nfs_found);
            if (status == 0) {
                if (nfs->age < nfs_found->age) {
                    nfs_found->age = nfs->age;
                    nfs_mountstats_reset(nfs_found);
                    memcpy(nfs_found->fams, nfs->fams, FAM_NFS_MOUNT_MAX*sizeof(nfs->fams[0]));
                    free(nfs);
                } else {
                    nfs_mountstats_reset(nfs);
                    free(nfs);
                }
            } else {
                char *mount_dup = strdup(mount);
                if (mount_dup == NULL) {
                    nfs_mountstats_reset(nfs);
                    free(nfs);
                    PLUGIN_ERROR("strdup failed");
                    continue;
                }
                status = c_avl_insert(tree, mount_dup, nfs);
                if (unlikely(status != 0)) {
                    PLUGIN_ERROR("Failed insertion in avl tree");
                    nfs_mountstats_reset(nfs);
                    free(nfs);
                    free(mount_dup);
                    continue;
                }
            }

            if (feof(fh))
                break;
        }
    }

    fclose(fh);

    while (true) {
        nfs_mountstats_t *nfs = NULL;
        char *mount = NULL;
        int status = c_avl_pick(tree, (void *)&mount, (void *)&nfs);
        if (status != 0)
            break;
        plugin_dispatch_metric_family_array(nfs->fams, FAM_NFS_MOUNT_MAX, 0);
        free(nfs);
        free(mount);
    }
    c_avl_destroy(tree);

    return 0;
}

static int nfs_read_net_rpc_nfs(void)
{
    FILE *fh = fopen(path_proc_nfs, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Unable to open '%s': %s", path_proc_nfs, STRERRNO);
        return -1;
    }

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *fields[nfs4_procedures_names_num + 2];

        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));

        if (fields_num < 3)
            continue;

        uint64_t value;

        if (strcmp(fields[0], "rpc") == 0) {
            if (strtouint(fields[1], &value) == 0)
                metric_family_append(&fams[FAM_NFS_RPC_CALLS],
                                     VALUE_COUNTER(value), NULL, NULL);
            if (strtouint(fields[2], &value) == 0)
                metric_family_append(&fams[FAM_NFS_RPC_RETRANSMISSIONS],
                                     VALUE_COUNTER(value), NULL, NULL);
            if (strtouint(fields[3], &value) == 0)
                metric_family_append(&fams[FAM_NFS_RPC_AUTHENTICATION_REFRESHES],
                                     VALUE_COUNTER(value), NULL, NULL);
        } else if (strncmp(fields[0], "proc", strlen("proc")) == 0) {
            const char **procedures_names;
            int procedures_names_num = 0;
            switch (*(fields[0] + strlen("proc"))) {
            case '2':
                if (!(flags & COLLECT_NFS_V2))
                    continue;
                procedures_names = nfs2_procedures_names;
                procedures_names_num = nfs2_procedures_names_num;
                break;
            case '3':
                if (!(flags & COLLECT_NFS_V3))
                    continue;
                procedures_names = nfs3_procedures_names;
                procedures_names_num = nfs3_procedures_names_num;
                break;
            case '4':
                if (!(flags & COLLECT_NFS_V4))
                    continue;
                procedures_names = nfs4_procedures_names;
                procedures_names_num = nfs4_procedures_names_num;
                break;
            default:
                continue;
                break;
            }

            char *proto = fields[0] + strlen("proc");

            fields_num = fields_num - 2;
            if (fields_num > procedures_names_num)
                fields_num = procedures_names_num;

            for (int i = 0; i < fields_num; i++) {
                if (strtouint(fields[2+i], &value) == 0) {
                     metric_family_append(&fams[FAM_NFS_REQUESTS],
                                VALUE_COUNTER(value), NULL,
                                &(label_pair_const_t){.name="method", .value=procedures_names[i]},
                                &(label_pair_const_t){.name="proto", .value=proto}, NULL);
                }
            }
        }
    }

    fclose(fh);

    plugin_dispatch_metric_family_array(fams, FAM_NFS_MAX, 0);

    return 0;
}

static int nfs_read(void)
{
    int status = nfs_read_net_rpc_nfs();
    if (flags & COLLECT_MOUNT_STATS)
        status |= nfs_read_mountstats();
    return status;
}

static int nfs_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp("collect", child->key) == 0) {
            status = cf_util_get_flags(child, nfs_flags, nfs_flags_size, &flags);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0)
        return -1;

    return 0;
}

static int nfs_init(void)
{
    path_proc_nfs = plugin_procpath("net/rpc/nfs");
    if (path_proc_nfs == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    path_proc_mountstats = plugin_procpath("self/mountstats");
    if (path_proc_mountstats == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int nfs_shutdown(void)
{
    free(path_proc_nfs);
    free(path_proc_mountstats);
    return 0;
}

void module_register(void)
{
    plugin_register_init("nfs", nfs_init);
    plugin_register_config("nfs", nfs_config);
    plugin_register_read("nfs", nfs_read);
    plugin_register_shutdown("nfs", nfs_shutdown);
}
