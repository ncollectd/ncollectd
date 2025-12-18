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

static char *path_proc_nfsd;

static const char *nfs2_procedures_names[] = {
    "null", "getattr", "setattr", "root",   "lookup",  "readlink",
    "read", "wrcache", "write",   "create", "remove",  "rename",
    "link", "symlink", "mkdir",   "rmdir",  "readdir", "fsstat"
};
static size_t nfs2_procedures_names_num = STATIC_ARRAY_SIZE(nfs2_procedures_names);

static const char *nfs3_procedures_names[] = {
    "null",   "getattr", "setattr",  "lookup", "access",  "readlink",
    "read",   "write",   "create",   "mkdir",  "symlink", "mknod",
    "remove", "rmdir",   "rename",   "link",   "readdir", "readdirplus",
    "fsstat", "fsinfo",  "pathconf", "commit"
};
static size_t nfs3_procedures_names_num = STATIC_ARRAY_SIZE(nfs3_procedures_names);

static const char *nfs4_procedures_names[] = {
    "null",             "compound",             "reserved",          "access",
    "close",            "commit",               "create",            "delegpurge",
    "delegreturn",      "getattr",              "getfh",             "link",
    "lock",             "lockt",                "locku",             "lookup",
    "lookupp",          "nverify",              "open",              "openattr",
    "open_confirm",     "open_downgrade",       "putfh",             "putpubfh",
    "putrootfh",        "read",                 "readdir",           "readlink",
    "remove",           "rename",               "renew",             "restorefh",
    "savefh",           "secinfo",              "setattr",           "setclientid",
    "setcltid_confirm", "verify",               "write",             "release_lockowner",
    /* NFS 4.1 */
    "backchannel_ctl", "bind_conn_to_session", "exchange_id",        "create_session",
    "destroy_session", "free_stateid",         "get_dir_delegation", "getdeviceinfo",
    "getdevicelist",   "layoutcommit",         "layoutget",          "layoutreturn",
    "secinfo_no_name", "sequence",             "set_ssv",            "test_stateid",
    "want_delegation", "destroy_clientid",     "reclaim_complete",
    /* NFS 4.2 */
    "allocate",        "copy",                 "copy_notify",        "deallocate",
    "ioadvise",        "layouterror",          "layoutstats",        "offloadcancel",
    "offloadstatus",   "readplus",             "seek",               "write_same",
    "clone",
    /* xattr support (RFC8726) */
    "getxattr",        "setxattr",             "listxattrs",         "removexattr"
};
static size_t nfs4_procedures_names_num = STATIC_ARRAY_SIZE(nfs4_procedures_names);

enum {
    FAM_NFSD_REPLY_CACHE_HITS,
    FAM_NFSD_REPLY_CACHE_MISSES,
    FAM_NFSD_REPLY_CACHE_NOCACHE,
    FAM_NFSD_FILE_HANDLES_STALE,
    FAM_NFSD_DISK_BYTES_READ,
    FAM_NFSD_DISK_BYTES_WRITTEN,
    FAM_NFSD_SERVER_THREADS,
    FAM_NFSD_READ_AHEAD_CACHE_SIZE_BLOCKS,
    FAM_NFSD_READ_AHEAD_CACHE_NOT_FOUND,
    FAM_NFSD_PACKETS,
    FAM_NFSD_CONNECTIONS,
    FAM_NFSD_RPC_ERRORS,
    FAM_NFSD_SERVER_RPC_CALLS,
    FAM_NFSD_REQUESTS,
    FAM_NFSD_MAX,
};

static metric_family_t fams[FAM_NFSD_MAX] = {
    [FAM_NFSD_REPLY_CACHE_HITS] = {
        .name = "system_nfsd_reply_cache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NFSd Reply Cache hits (client lost server response)",
    },
    [FAM_NFSD_REPLY_CACHE_MISSES] = {
        .name = "system_nfsd_reply_cache_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NFSd Reply Cache an operation that requires caching (idempotent)",
    },
    [FAM_NFSD_REPLY_CACHE_NOCACHE] = {
        .name = "system_nfsd_reply_cache_nocache",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NFSd Reply Cache non-idempotent operations (rename/delete/…)",
    },
    [FAM_NFSD_FILE_HANDLES_STALE] = {
        .name = "system_nfsd_file_handles_stale",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NFSd stale file handles",
    },
    [FAM_NFSD_DISK_BYTES_READ] = {
        .name = "system_nfsd_disk_bytes_read",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total NFSd bytes read",
    },
    [FAM_NFSD_DISK_BYTES_WRITTEN] = {
        .name = "system_nfsd_disk_bytes_written",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total NFSd bytes written"
    },
    [FAM_NFSD_SERVER_THREADS] = {
        .name = "system_nfsd_server_threads",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total number of NFSd kernel threads that are running",
    },
    [FAM_NFSD_READ_AHEAD_CACHE_SIZE_BLOCKS] = {
        .name = "system_nfsd_read_ahead_cache_size_blocks",
        .type = METRIC_TYPE_GAUGE,
        .help = "How large the read ahead cache is in blocks",
    },
    [FAM_NFSD_READ_AHEAD_CACHE_NOT_FOUND] = {
        .name = "system_nfsd_read_ahead_cache_not_found",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total number of NFSd read ahead cache not found",
    },
    [FAM_NFSD_PACKETS] = {
        .name = "system_nfsd_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total NFSd network packets (sent+received) by protocol type",
    },
    [FAM_NFSD_CONNECTIONS] = {
        .name = "system_nfsd_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NFSd TCP connections",
    },
    [FAM_NFSD_RPC_ERRORS] = {
        .name = "system_nfsd_rpc_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NFSd RPC errors by error type",
    },
    [FAM_NFSD_SERVER_RPC_CALLS] = {
        .name = "system_nfsd_server_rpc_calls",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NFSd RPC calls",
    },
    [FAM_NFSD_REQUESTS] = {
        .name = "system_nfsd_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number NFSd Requests by method and protocol",
    },
};

enum {
    COLLECT_NFS_V2      = (1 <<  0),
    COLLECT_NFS_V3      = (1 <<  1),
    COLLECT_NFS_V4      = (1 <<  2)
};

static cf_flags_t nfsd_flags[] = {
    { "nfs-v2",      COLLECT_NFS_V2      },
    { "nfs-v3",      COLLECT_NFS_V3      },
    { "nfs-v4",      COLLECT_NFS_V4      }
};
static size_t nfsd_flags_size = STATIC_ARRAY_SIZE(nfsd_flags);

static uint64_t flags = COLLECT_NFS_V2 | COLLECT_NFS_V3 | COLLECT_NFS_V4;

static int nfsd_read(void)
{
    FILE *fh = fopen(path_proc_nfsd, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Cannot open '%s': %s", path_proc_nfsd, STRERRNO);
        return -1;
    }

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *fields[nfs4_procedures_names_num + 2];

        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));

        if (fields_num < 2)
            continue;

        if (strcmp(fields[0], "rc") == 0) {
            uint64_t value;
            if (fields_num < 4)
                continue;
            if (strtouint(fields[1], &value) == 0)
                metric_family_append(&fams[FAM_NFSD_REPLY_CACHE_HITS],
                                     VALUE_COUNTER(value), NULL, NULL);
            if (strtouint(fields[2], &value) == 0)
                metric_family_append(&fams[FAM_NFSD_REPLY_CACHE_MISSES],
                                     VALUE_COUNTER(value), NULL, NULL);
            if (strtouint(fields[3], &value) == 0)
                metric_family_append(&fams[FAM_NFSD_REPLY_CACHE_NOCACHE],
                                     VALUE_COUNTER(value), NULL, NULL);
        } else if (strcmp(fields[0], "fh") == 0) {
            uint64_t value;
            if (fields_num < 6)
                continue;
            if (strtouint(fields[1], &value) == 0)
                metric_family_append(&fams[FAM_NFSD_FILE_HANDLES_STALE],
                                     VALUE_COUNTER(value), NULL, NULL);
        } else if (strcmp(fields[0], "io") == 0) {
            uint64_t value;
            if (fields_num < 3)
                continue;
            if (strtouint(fields[1], &value) == 0)
                metric_family_append(&fams[FAM_NFSD_DISK_BYTES_READ],
                                     VALUE_COUNTER(value), NULL, NULL);
            if (strtouint(fields[2], &value) == 0)
                metric_family_append(&fams[FAM_NFSD_DISK_BYTES_WRITTEN],
                                     VALUE_COUNTER(value), NULL, NULL);
        } else if (strcmp(fields[0], "th") == 0) {
            double value;
            if (strtodouble(fields[1], &value) == 0)
                metric_family_append(&fams[FAM_NFSD_SERVER_THREADS],
                                     VALUE_GAUGE(value), NULL, NULL);
        } else if (strcmp(fields[0], "ra") == 0) {
            if (fields_num < 13)
                continue;
            double gauge;
            if (strtodouble(fields[1], &gauge) == 0)
                metric_family_append(&fams[FAM_NFSD_READ_AHEAD_CACHE_SIZE_BLOCKS],
                                     VALUE_GAUGE(gauge), NULL, NULL);
            uint64_t counter;
            if (strtouint(fields[12], &counter) == 0)
                metric_family_append(&fams[FAM_NFSD_READ_AHEAD_CACHE_NOT_FOUND],
                                     VALUE_COUNTER(counter), NULL, NULL);
        } else if (strcmp(fields[0], "net") == 0) {
            uint64_t value;
            if (fields_num < 5)
                continue;
            if (strtouint(fields[2], &value) == 0)
                metric_family_append(&fams[FAM_NFSD_PACKETS], VALUE_COUNTER(value), NULL,
                                     &(label_pair_const_t){.name="protocol", .value="udp"}, NULL);
            if (strtouint(fields[3], &value) == 0)
                metric_family_append(&fams[FAM_NFSD_PACKETS], VALUE_COUNTER(value) , NULL,
                                     &(label_pair_const_t){.name="protocol", .value="tcp"}, NULL);
            if (strtouint(fields[4], &value) == 0)
                metric_family_append(&fams[FAM_NFSD_CONNECTIONS], VALUE_COUNTER(value), NULL, NULL);
        } else if (strcmp(fields[0], "rpc") == 0) {
            uint64_t value;
            if (fields_num < 6)
                continue;
            if (strtouint(fields[1], &value) == 0)
                metric_family_append(&fams[FAM_NFSD_SERVER_RPC_CALLS],
                                            VALUE_COUNTER(value), NULL, NULL);
            if (strtouint(fields[3], &value) == 0)
                metric_family_append(&fams[FAM_NFSD_RPC_ERRORS], VALUE_COUNTER(value), NULL,
                                     &(label_pair_const_t){.name="error", .value="fmt"}, NULL);
            if (strtouint(fields[4], &value) == 0)
                metric_family_append(&fams[FAM_NFSD_RPC_ERRORS], VALUE_COUNTER(value), NULL,
                                     &(label_pair_const_t){.name="error", .value="auth"}, NULL);
            if (strtouint(fields[5], &value) == 0)
                metric_family_append(&fams[FAM_NFSD_RPC_ERRORS], VALUE_COUNTER(value), NULL,
                                     &(label_pair_const_t){.name="error", .value="cInt"}, NULL);
        } else if (strncmp(fields[0], "proc", strlen("proc")) == 0) {
            const char **procedures_names;
            int procedures_names_num = 0;
            char *proto = NULL;
            switch (*(fields[0] + strlen("proc"))) {
            case '2':
                if (!(flags & COLLECT_NFS_V2))
                    continue;
                proto = "2";
                procedures_names = nfs2_procedures_names;
                procedures_names_num = nfs2_procedures_names_num;
                break;
            case '3':
                if (!(flags & COLLECT_NFS_V3))
                    continue;
                proto = "3";
                procedures_names = nfs3_procedures_names;
                procedures_names_num = nfs3_procedures_names_num;
                break;
            case '4':
                if (strcmp(fields[0], "proc4ops") != 0)
                    continue;
                if (!(flags & COLLECT_NFS_V4))
                    continue;
                proto = "4";
                procedures_names = nfs4_procedures_names;
                procedures_names_num = nfs4_procedures_names_num;
                break;
            default:
                continue;
                break;
            }

            fields_num = fields_num - 2;
            if (fields_num > procedures_names_num)
                fields_num = procedures_names_num;

            for (int i = 0; i < fields_num; i++) {
                uint64_t value;
                if (strtouint(fields[2+i], &value) == 0) {
                     metric_family_append(&fams[FAM_NFSD_REQUESTS], VALUE_COUNTER(value), NULL,
                                &(label_pair_const_t){.name="method", .value=procedures_names[i]},
                                &(label_pair_const_t){.name="proto", .value=proto},
                                NULL);
                }
            }
        }
    }
    fclose(fh);

    plugin_dispatch_metric_family_array(fams, FAM_NFSD_MAX, 0);

    return 0;
}

static int nfsd_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("collect", child->key) == 0) {
            status = cf_util_get_flags(child, nfsd_flags, nfsd_flags_size, &flags);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int nfsd_init(void)
{
    path_proc_nfsd = plugin_procpath("net/rpc/nfsd");
    if (path_proc_nfsd == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int nfsd_shutdown(void)
{
    free(path_proc_nfsd);
    return 0;
}

void module_register(void)
{
    plugin_register_init("nfsd", nfsd_init);
    plugin_register_config("nfsd", nfsd_config);
    plugin_register_read("nfsd", nfsd_read);
    plugin_register_shutdown("nfsd", nfsd_shutdown);
}
