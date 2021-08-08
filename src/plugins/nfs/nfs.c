/**
 * collectd - src/nfs.c
 * Copyright (C) 2005,2006  Jason Pepas
 * Copyright (C) 2012,2013  Florian Forster
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; only version 2 of the License is applicable.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Authors:
 *   Jason Pepas <cell at ices.utexas.edu>
 *   Florian octo Forster <octo at collectd.org>
 *   Cosmin Ioiart <cioiart at gmail.com>
 **/

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
  "null", "read", "write", "commit", "open", "open_confirm",
  "open_noattr", "open_downgrade", "close", "setattr", "fsinfo",
  "renew", "setclientid", "setclientid_confirm", "lock", "lockt",
  "locku", "access", "getattr", "lookup", "lookup_root", "remove",
  "rename", "link", "symlink", "create", "pathconf", "statfs",
  "readlink", "readdir", "server_caps", "delegreturn", "getacl",
  "setacl", "fs_locations", "release_lockowner", "secinfo", "fsid_present",
   /* NFS 4.1 */
  "exchange_id", "create_session", "destroy_session", "sequence",
  "get_lease_time", "reclaim_complete", "layoutget", "getdeviceinfo",
  "layoutcommit", "layoutreturn", "secinfo_no_name", "test_stateid",
  "free_stateid", "getdevicelist", "bind_conn_to_session", "destroy_clientid",
   /* NFS 4.2 */
  "seek", "allocate", "deallocate", "layoutstats", "clone", "copy",
  "offload_cancel", "lookupp", "layouterror", "copy_notify",
  /* xattr support (RFC8726) */
  "getxattr", "setxattr", "listxattrs", "removexattr", "read_plus"
};
static size_t nfs4_procedures_names_num = STATIC_ARRAY_SIZE(nfs4_procedures_names);

enum {
  FAM_HOST_NFS_PACKETS_TOTAL,
  FAM_HOST_NFS_RPC_CALLS_TOTAL,
  FAM_HOST_NFS_RPC_RETRANSMISSIONS_TOTAL,
  FAM_HOST_NFS_RPC_AUTHENTICATION_REFRESHES_TOTAL,
  FAM_HOST_NFS_REQUESTS_TOTAL,
  FAM_HOST_NFS_MAX,
};

static metric_family_t fams[FAM_HOST_NFS_MAX] = {
  [FAM_HOST_NFS_PACKETS_TOTAL] = {
    .name = "host_nfs_packets_total",
    .type = METRIC_TYPE_COUNTER,
    .help = "Total NFSd network packets (sent+received) by protocol type.", //"protocol"
  },
  [FAM_HOST_NFS_RPC_CALLS_TOTAL] = {
    .name = "host_nfs_rpc_calls_total",
    .type = METRIC_TYPE_COUNTER,
    .help = "Total number of RPC calls performed.",
  },
  [FAM_HOST_NFS_RPC_RETRANSMISSIONS_TOTAL] = {
    .name = "host_nfs_rpc_retransmissions_total",
    .type = METRIC_TYPE_COUNTER,
    .help = "Number of RPC transmissions performed.",
  },
  [FAM_HOST_NFS_RPC_AUTHENTICATION_REFRESHES_TOTAL] = {
    .name = "host_nfs_rpc_authentication_refreshes_total",
    .type = METRIC_TYPE_COUNTER,
    .help = "Number of RPC authentication refreshes performed.",
  },
  [FAM_HOST_NFS_REQUESTS_TOTAL] = {
    .name = "host_nfs_requests_total",
    .type = METRIC_TYPE_COUNTER,
    .help = "Number of NFS procedures invoked.",
  },
};

static int nfs_config(const char *key, const char *value) {
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

static int nfs_read(void)
{
  FILE *fh = fopen("/proc/net/rpc/nfs", "r");
  if (fh == NULL)
    return -1;

  char buffer[4096];
  while (fgets(buffer, sizeof(buffer), fh) != NULL) {
    char *fields[nfs4_procedures_names_num + 2];

    int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));

    if (fields_num < 3)
      continue;

    uint64_t value;

    if (strcmp(fields[0], "rpc") == 0) {
      if (strtouint(fields[1], &value) == 0)
        metric_family_metric_append(&fams[FAM_HOST_NFS_RPC_CALLS_TOTAL],
                                    (metric_t){ .value.counter.uint64 = value });
      if (strtouint(fields[2], &value) == 0)
        metric_family_metric_append(&fams[FAM_HOST_NFS_RPC_RETRANSMISSIONS_TOTAL],
                                    (metric_t){ .value.counter.uint64 = value });
      if (strtouint(fields[3], &value) == 0)
        metric_family_metric_append(&fams[FAM_HOST_NFS_RPC_AUTHENTICATION_REFRESHES_TOTAL],
                                    (metric_t){ .value.counter.uint64 = value });
    } if (strncmp(fields[0], "proc", strlen("proc")) == 0) {
      const char **procedures_names;
      int procedures_names_num = 0;
      switch (*(fields[0] + strlen("proc"))) {
        case '2':
          if (!report_v2)
            continue;
          procedures_names = nfs2_procedures_names;
          procedures_names_num = nfs2_procedures_names_num;
          break;
        case '3':
          if (!report_v3)
            continue;
          procedures_names = nfs3_procedures_names;
          procedures_names_num = nfs3_procedures_names_num;
          break;
        case '4':
          if (!report_v4)
            continue;
          procedures_names = nfs4_procedures_names;
          procedures_names_num = nfs4_procedures_names_num;
          break;
        default:
          continue;
          break;
      }

      metric_t tmpl = {0};
      metric_label_set(&tmpl, "proto", fields[0] + strlen("proc"));
      
      fields_num = fields_num - 2;
      if (fields_num > procedures_names_num)
        fields_num = procedures_names_num;

      for (size_t i = 0; i < fields_num; i++) {
        if (strtouint(fields[2+i], &value) == 0) {
           metric_family_append(&fams[FAM_HOST_NFS_REQUESTS_TOTAL], 
                                "method", procedures_names[i], (value_t){.counter.uint64 = value}, &tmpl);
        }
      }

      metric_reset(&tmpl);
    }
  } 

  for (size_t i = 0; i < FAM_HOST_NFS_MAX; i++) {
    if (fams[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&fams[i]);
      if (status != 0) {
        ERROR("nfs plugin: plugin_dispatch_metric_family failed: %s",
              STRERROR(status));
      }
      metric_family_metric_reset(&fams[i]);
    }
  }

  return 0;
}

void module_register(void)
{
  plugin_register_config("nfs", nfs_config, config_keys, config_keys_num);
  plugin_register_read("nfs", nfs_read);
}
