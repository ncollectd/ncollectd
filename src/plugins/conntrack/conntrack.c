// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2009  Tomasz Pala
// Authors:
//   Tomasz Pala <gotar at pld-linux.org>
// based on entropy.c by:
//   Florian octo Forster <octo at collectd.org>

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

#if !KERNEL_LINUX
#error "No applicable input method."
#endif

#define CONNTRACK_FILE "/proc/sys/net/netfilter/nf_conntrack_count"
#define CONNTRACK_MAX_FILE "/proc/sys/net/netfilter/nf_conntrack_max"
#define CONNTRACK_FILE_OLD "/proc/sys/net/ipv4/netfilter/ip_conntrack_count"
#define CONNTRACK_MAX_FILE_OLD "/proc/sys/net/ipv4/netfilter/ip_conntrack_max"

static const char *config_keys[] = {"OldFiles"};
static int config_keys_num = STATIC_ARRAY_SIZE(config_keys);
/*
    Each table/chain combo that will be queried goes into this list
*/

static int old_files;

static int conntrack_config(const char *key, const char *value)
{
  if (strcmp(key, "OldFiles") == 0)
    old_files = 1;

  return 0;
}

static void conntrack_submit(char *fam_name, double value)
{
  metric_family_t fam = {
      .name = fam_name,
      .type = METRIC_TYPE_GAUGE,
  };

  metric_family_metric_append(&fam, (metric_t){ .value.gauge.float64 = value, });

  int status = plugin_dispatch_metric_family(&fam);
  if (status != 0)
    ERROR("conntrack plugin: plugin_dispatch_metric_family failed: %s", STRERROR(status));

  metric_family_metric_reset(&fam);
}

static int conntrack_read(void)
{
  double conntrack, conntrack_max, conntrack_pct;

  char const *path = old_files ? CONNTRACK_FILE_OLD : CONNTRACK_FILE;
  if (parse_double_file(path, &conntrack) != 0) {
    ERROR("conntrack plugin: Reading \"%s\" failed.", path);
    return -1;
  }

  path = old_files ? CONNTRACK_MAX_FILE_OLD : CONNTRACK_MAX_FILE;
  if (parse_double_file(path, &conntrack_max) != 0) {
    ERROR("conntrack plugin: Reading \"%s\" failed.", path);
    return -1;
  }

  conntrack_pct = (conntrack / conntrack_max) * 100;

  conntrack_submit("host_conntrack_used", conntrack);
  conntrack_submit("host_conntrack_max", conntrack_max);
  conntrack_submit("host_conntrack_used_percent", conntrack_pct);

  return 0;
}

void module_register(void)
{
  plugin_register_config("conntrack", conntrack_config, config_keys, config_keys_num);
  plugin_register_read("conntrack", conntrack_read);
}
