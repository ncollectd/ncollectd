// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2005,2006  Jason Pepas
// Copyright (C) 2012,2013  Florian Forster
// Authors:
//   Jason Pepas <cell at ices.utexas.edu>
//   Florian octo Forster <octo at collectd.org>
//   Cosmin Ioiart <cioiart at gmail.com>

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

static const char *config_keys[] = {"ReportV2", "ReportV3", "ReportV4"};
static int config_keys_num = STATIC_ARRAY_SIZE(config_keys);

static bool report_v2 = true;
static bool report_v3 = true;
static bool report_v4 = true;

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
  "null", "compound", "reserved", "access", "close", "commit",
  "create", "delegpurge", "delegreturn", "getattr", "getfh", "link",
  "lock", "lockt", "locku", "lookup", "lookupp", "nverify",
  "open", "openattr", "open_confirm", "open_downgrade", "putfh", "putpubfh",
  "putrootfh", "read", "readdir", "readlink", "remove", "rename",
  "renew", "restorefh", "savefh", "secinfo", "setattr", "setclientid",
  "setcltid_confirm", "verify", "write", "release_lockowner",
  /* NFS 4.1 */
  "backchannel_ctl", "bind_conn_to_session", "exchange_id", "create_session",
  "destroy_session", "free_stateid", "get_dir_delegation", "getdeviceinfo",
  "getdevicelist", "layoutcommit", "layoutget", "layoutreturn",
  "secinfo_no_name", "sequence", "set_ssv", "test_stateid", "want_delegation",
  "destroy_clientid", "reclaim_complete",
  /* NFS 4.2 */
  "allocate", "copy", "copy_notify", "deallocate", "ioadvise", "layouterror",
  "layoutstats", "offloadcancel", "offloadstatus", "readplus", "seek",
  "write_same", "clone",
  /* xattr support (RFC8726) */
  "getxattr", "setxattr", "listxattrs", "removexattr"
};
static size_t nfs4_procedures_names_num = STATIC_ARRAY_SIZE(nfs4_procedures_names);

enum {
  FAM_HOST_NFSD_REPLY_CACHE_HITS_TOTAL,
  FAM_HOST_NFSD_REPLY_CACHE_MISSES_TOTAL,
  FAM_HOST_NFSD_REPLY_CACHE_NOCACHE_TOTAL,
  FAM_HOST_NFSD_FILE_HANDLES_STALE_TOTAL,
  FAM_HOST_NFSD_DISK_BYTES_READ_TOTAL,
  FAM_HOST_NFSD_DISK_BYTES_WRITTEN_TOTAL,
  FAM_HOST_NFSD_SERVER_THREADS,
  FAM_HOST_NFSD_READ_AHEAD_CACHE_SIZE_BLOCKS,
  FAM_HOST_NFSD_READ_AHEAD_CACHE_NOT_FOUND_TOTAL,
  FAM_HOST_NFSD_PACKETS_TOTAL,
  FAM_HOST_NFSD_CONNECTIONS_TOTAL,
  FAM_HOST_NFSD_RPC_ERRORS_TOTAL,
  FAM_HOST_NFSD_SERVER_RPC_CALLS_TOTAL,
  FAM_HOST_NFSD_REQUESTS_TOTAL,
  FAM_HOST_NFSD_MAX,
};

static metric_family_t fams[FAM_HOST_NFSD_MAX] = {
  [FAM_HOST_NFSD_REPLY_CACHE_HITS_TOTAL] = {
    .name = "host_nfsd_reply_cache_hits_total",
    .type = METRIC_TYPE_COUNTER,
    .help = "Total number of NFSd Reply Cache hits (client lost server response)",
  },
  [FAM_HOST_NFSD_REPLY_CACHE_MISSES_TOTAL] = {
    .name = "host_nfsd_reply_cache_misses_total",
    .type = METRIC_TYPE_COUNTER,
    .help = "Total number of NFSd Reply Cache an operation that requires caching (idempotent)",
  },
  [FAM_HOST_NFSD_REPLY_CACHE_NOCACHE_TOTAL] = {
    .name = "host_nfsd_reply_cache_nocache_total",
    .type = METRIC_TYPE_COUNTER,
    .help = "Total number of NFSd Reply Cache non-idempotent operations (rename/delete/???)",
  },
  [FAM_HOST_NFSD_FILE_HANDLES_STALE_TOTAL] = {
    .name = "host_nfsd_file_handles_stale_total",
    .type = METRIC_TYPE_COUNTER,
    .help = "Total number of NFSd stale file handles",
  },
  [FAM_HOST_NFSD_DISK_BYTES_READ_TOTAL] = {
    .name = "host_nfsd_disk_bytes_read_total",
    .type = METRIC_TYPE_COUNTER,
    .help = "Total NFSd bytes read",
  },
  [FAM_HOST_NFSD_DISK_BYTES_WRITTEN_TOTAL] = {
    .name = "host_nfsd_disk_bytes_written_total",
    .type = METRIC_TYPE_COUNTER,
    .help = "Total NFSd bytes written"
  },
  [FAM_HOST_NFSD_SERVER_THREADS] = {
    .name = "host_nfsd_server_threads",
    .type = METRIC_TYPE_GAUGE,
    .help = "Total number of NFSd kernel threads that are running",
  },
  [FAM_HOST_NFSD_READ_AHEAD_CACHE_SIZE_BLOCKS] = {
    .name = "host_nfsd_read_ahead_cache_size_blocks",
    .type = METRIC_TYPE_GAUGE,
    .help = "How large the read ahead cache is in blocks",
  },
  [FAM_HOST_NFSD_READ_AHEAD_CACHE_NOT_FOUND_TOTAL] = {
    .name = "host_nfsd_read_ahead_cache_not_found_total",
    .type = METRIC_TYPE_GAUGE,
    .help = "Total number of NFSd read ahead cache not found",
  },
  [FAM_HOST_NFSD_PACKETS_TOTAL] = {
    .name = "host_nfsd_packets_total",
    .type = METRIC_TYPE_COUNTER,
    .help = "Total NFSd network packets (sent+received) by protocol type",
  },
  [FAM_HOST_NFSD_CONNECTIONS_TOTAL] = {
    .name = "host_nfsd_connections_total",
    .type = METRIC_TYPE_COUNTER,
    .help = "Total number of NFSd TCP connections",
  },
  [FAM_HOST_NFSD_RPC_ERRORS_TOTAL] = {
    .name = "host_nfsd_rpc_errors_total",
    .type = METRIC_TYPE_COUNTER,
    .help = "Total number of NFSd RPC errors by error type",
  },
  [FAM_HOST_NFSD_SERVER_RPC_CALLS_TOTAL] = {
    .name = "host_nfsd_server_rpc_calls_total",
    .type = METRIC_TYPE_COUNTER,
    .help = "Total number of NFSd RPC calls",
  },
  [FAM_HOST_NFSD_REQUESTS_TOTAL] = {
    .name = "host_nfsd_requests_total",
    .type = METRIC_TYPE_COUNTER,
    .help = "Total number NFSd Requests by method and protocol", // {"proto", "method"}
  },
};

static int nfsd_config(const char *key, const char *value) {
  if (strcasecmp(key, "ReportV2") == 0)
    report_v2 = IS_TRUE(value);
  else if (strcasecmp(key, "ReportV3") == 0)
    report_v3 = IS_TRUE(value);
  else if (strcasecmp(key, "ReportV4") == 0)
    report_v4 = IS_TRUE(value);
  else
    return -1;

  return 0;
}

static int nfsd_read(void)
{

  FILE *fh = fopen("/proc/net/rpc/nfsd", "r");
  if (fh == NULL)
    return -1;

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
        metric_family_metric_append(&fams[FAM_HOST_NFSD_REPLY_CACHE_HITS_TOTAL],
                                    (metric_t){ .value.counter.uint64 = value });
      if (strtouint(fields[2], &value) == 0)
        metric_family_metric_append(&fams[FAM_HOST_NFSD_REPLY_CACHE_MISSES_TOTAL],
                                    (metric_t){ .value.counter.uint64 = value });
      if (strtouint(fields[3], &value) == 0)
        metric_family_metric_append(&fams[FAM_HOST_NFSD_REPLY_CACHE_NOCACHE_TOTAL],
                                    (metric_t){ .value.counter.uint64 = value });
    } else if (strcmp(fields[0], "fh") == 0) {
      uint64_t value;
      if (fields_num < 6)
        continue;
      if (strtouint(fields[1], &value) == 0)
        metric_family_metric_append(&fams[FAM_HOST_NFSD_FILE_HANDLES_STALE_TOTAL],
                                    (metric_t){ .value.counter.uint64 = value });
    } else if (strcmp(fields[0], "io") == 0) {
      uint64_t value;
      if (fields_num < 3)
        continue;
      if (strtouint(fields[1], &value) == 0)
        metric_family_metric_append(&fams[FAM_HOST_NFSD_DISK_BYTES_READ_TOTAL],
                                    (metric_t){ .value.counter.uint64 = value });
      if (strtouint(fields[2], &value) == 0)
        metric_family_metric_append(&fams[FAM_HOST_NFSD_DISK_BYTES_WRITTEN_TOTAL],
                                    (metric_t){ .value.counter.uint64 = value });
    } else if (strcmp(fields[0], "th") == 0) {
      double value;
      if (fields_num < 2)
        continue;
      if (strtodouble(fields[1], &value) == 0)
        metric_family_metric_append(&fams[FAM_HOST_NFSD_SERVER_THREADS],
                                    (metric_t){ .value.gauge.float64 = value });
    } else if (strcmp(fields[0], "ra") == 0) {
      if (fields_num < 13)
        continue;
      double gauge;
      if (strtodouble(fields[1], &gauge) == 0)
        metric_family_metric_append(&fams[FAM_HOST_NFSD_READ_AHEAD_CACHE_SIZE_BLOCKS],
                                    (metric_t){ .value.gauge.float64 = gauge});
      uint64_t counter;
      if (strtouint(fields[12], &counter) == 0)
        metric_family_metric_append(&fams[FAM_HOST_NFSD_READ_AHEAD_CACHE_NOT_FOUND_TOTAL],
                                    (metric_t){ .value.counter.uint64 = counter});
    } else if (strcmp(fields[0], "net") == 0) {
      uint64_t value;
      if (fields_num < 5)
        continue;
      if (strtouint(fields[2], &value) == 0)
        metric_family_append(&fams[FAM_HOST_NFSD_PACKETS_TOTAL],
                             "protocol", "udp", (value_t){.counter.uint64 = value}, NULL);
      if (strtouint(fields[3], &value) == 0)
        metric_family_append(&fams[FAM_HOST_NFSD_PACKETS_TOTAL],
                             "protocol", "tcp", (value_t){.counter.uint64 = value} , NULL);
      if (strtouint(fields[4], &value) == 0)
        metric_family_metric_append(&fams[FAM_HOST_NFSD_CONNECTIONS_TOTAL],
                                    (metric_t){ .value.counter.uint64 = value });
    } else if (strcmp(fields[0], "rpc") == 0) {
      uint64_t value;
      if (fields_num < 6)
        continue;
      if (strtouint(fields[1], &value) == 0)
        metric_family_metric_append(&fams[FAM_HOST_NFSD_SERVER_RPC_CALLS_TOTAL],
                                    (metric_t){ .value.counter.uint64 = value });
      if (strtouint(fields[3], &value) == 0)
        metric_family_append(&fams[FAM_HOST_NFSD_RPC_ERRORS_TOTAL],
                             "error", "fmt", (value_t){.counter.uint64 = value}, NULL);
      if (strtouint(fields[4], &value) == 0)
        metric_family_append(&fams[FAM_HOST_NFSD_RPC_ERRORS_TOTAL],
                             "error", "auth", (value_t){.counter.uint64 = value}, NULL);
      if (strtouint(fields[5], &value) == 0)
        metric_family_append(&fams[FAM_HOST_NFSD_RPC_ERRORS_TOTAL],
                             "error", "cInt", (value_t){.counter.uint64 = value}, NULL);
    } if (strncmp(fields[0], "proc", strlen("proc")) == 0) {
      const char **procedures_names;
      int procedures_names_num = 0;
      char *proto = NULL;
      switch (*(fields[0] + strlen("proc"))) {
        case '2':
          if (!report_v2)
            continue;
          proto = "2";
          procedures_names = nfs2_procedures_names;
          procedures_names_num = nfs2_procedures_names_num;
          break;
        case '3':
          if (!report_v3)
            continue;
          proto = "3";
          procedures_names = nfs3_procedures_names;
          procedures_names_num = nfs3_procedures_names_num;
          break;
        case '4':
          if (strcmp(fields[0], "proc4ops") != 0)
            continue;
          if (!report_v4)
            continue;
          proto = "4";
          procedures_names = nfs4_procedures_names;
          procedures_names_num = nfs4_procedures_names_num;
          break;
        default:
          continue;
          break;
      }

      metric_t tmpl = {0};
      metric_label_set(&tmpl, "proto", proto);
      
      fields_num = fields_num - 2;
      if (fields_num > procedures_names_num)
        fields_num = procedures_names_num;

      for (size_t i = 0; i < fields_num; i++) {
        uint64_t value;
        if (strtouint(fields[2+i], &value) == 0) {
           metric_family_append(&fams[FAM_HOST_NFSD_REQUESTS_TOTAL], 
                                "method", procedures_names[i], (value_t){.counter.uint64 = value}, &tmpl);
        }
      }

      metric_reset(&tmpl);
    }
  } 

  for (size_t i = 0; i < FAM_HOST_NFSD_MAX; i++) {
    if (fams[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&fams[i]);
      if (status != 0) {
        ERROR("nfsd plugin: plugin_dispatch_metric_family failed: %s",
              STRERROR(status));
      }
      metric_family_metric_reset(&fams[i]);
    }
  }

  return 0;
}

void module_register(void)
{
  plugin_register_config("nfsd", nfsd_config, config_keys, config_keys_num);
  plugin_register_read("nfsd", nfsd_read);
}
