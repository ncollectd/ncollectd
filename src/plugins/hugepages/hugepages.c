// SPDX-License-Identifier: GPL-2.0-only OR MIT
// Copyright(c) 2016 Intel Corporation. All rights reserved.
// Authors:
//   Jaroslav Safka <jaroslavx.safka@intel.com>
//   Kim-Marie Jones <kim-marie.jones@intel.com>
//   Florian Forster <octo at collectd.org>

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

static bool report_numa = true;
static bool report_mm = true;

static bool values_pages = true;
static bool values_bytes;

static const char *sys_mm_hugepages = "/sys/kernel/mm/hugepages";
static const char *sys_node = "/sys/devices/system/node";

enum {
  FAM_HOST_HUGEPAGES_NR,
  FAM_HOST_HUGEPAGES_FREE,
  FAM_HOST_HUGEPAGES_RESERVED,
  FAM_HOST_HUGEPAGES_SURPLUS,
  FAM_HOST_HUGEPAGES_NR_BYTES,
  FAM_HOST_HUGEPAGES_FREE_BYTES,
  FAM_HOST_HUGEPAGES_RESERVED_BYTES,
  FAM_HOST_HUGEPAGES_SURPLUS_BYTES,
  FAM_HOST_HUGEPAGES_NODE_NR,
  FAM_HOST_HUGEPAGES_NODE_FREE,
  FAM_HOST_HUGEPAGES_NODE_SURPLUS,
  FAM_HOST_HUGEPAGES_NODE_NR_BYTES,
  FAM_HOST_HUGEPAGES_NODE_FREE_BYTES,
  FAM_HOST_HUGEPAGES_NODE_SURPLUS_BYTES,
  FAM_HOST_HUGEPAGES_MAX,
};

static metric_family_t fams[FAM_HOST_HUGEPAGES_MAX] = {
  [FAM_HOST_HUGEPAGES_NR] = {
    .name = "host_hugepages_nr",
    .type = METRIC_TYPE_GAUGE,
    .help = "The current number of \"persistent\" huge pages in the kernel's huge page pool"
  },
  [FAM_HOST_HUGEPAGES_FREE] = {
    .name = "host_hugepages_free",
    .type = METRIC_TYPE_GAUGE,
    .help = "The number of huge pages in the pool that are not yet allocated"
  },
  [FAM_HOST_HUGEPAGES_RESERVED] = {
    .name = "host_hugepages_reserved",
    .type = METRIC_TYPE_GAUGE,
    .help = "The number of huge pages for which a commitment to allocate from the pool has been made, but no allocation has yet been made"
  },
  [FAM_HOST_HUGEPAGES_SURPLUS] = {
    .name = "host_hugepages_surplus",
    .type = METRIC_TYPE_GAUGE,
    .help = "The number of huge pages in the pool above the value in /proc/sys/vm/nr_hugepages"
  },
  [FAM_HOST_HUGEPAGES_NR_BYTES] = {
    .name = "host_hugepages_nr_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "The current size in bytes of \"persistent\" huge pages in the kernel's huge page pool"
  },
  [FAM_HOST_HUGEPAGES_FREE_BYTES] = {
    .name = "host_hugepages_free_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "The sizie in bytes of huge pages in the pool that are not yet allocated"
  },
  [FAM_HOST_HUGEPAGES_RESERVED_BYTES] = {
    .name = "host_hugepages_reserved_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "The size in bytes of huge pages for which a commitment to allocate from the pool has been made, but no allocation has yet been made"
  },
  [FAM_HOST_HUGEPAGES_SURPLUS_BYTES] = {
    .name = "host_hugepages_surplus_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "The size in bytes of huge pages in the pool above the value in /proc/sys/vm/nr_hugepages"
  },
  [FAM_HOST_HUGEPAGES_NODE_NR] = {
    .name = "host_hugepages_node_nr",
    .type = METRIC_TYPE_GAUGE,
    .help = "The current number of \"persistent\" huge pages in the kernel's huge page pool"
  },
  [FAM_HOST_HUGEPAGES_NODE_FREE] = {
    .name = "host_hugepages_node_free",
    .type = METRIC_TYPE_GAUGE,
    .help = "The number of huge pages in the pool that are not yet allocated"
  },
  [FAM_HOST_HUGEPAGES_NODE_SURPLUS] = {
    .name = "host_hugepages_node_surplus",
    .type = METRIC_TYPE_GAUGE,
    .help = "The number of huge pages in the pool above the value in /proc/sys/vm/nr_hugepages"
  },
  [FAM_HOST_HUGEPAGES_NODE_NR_BYTES] = {
    .name = "host_hugepages_node_nr_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "The current size in bytes of \"persistent\" huge pages in the kernel's huge page pool"
  },
  [FAM_HOST_HUGEPAGES_NODE_FREE_BYTES] = {
    .name = "host_hugepages_node_free_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "The size in bytes of huge pages in the pool that are not yet allocated"
  },
  [FAM_HOST_HUGEPAGES_NODE_SURPLUS_BYTES] = {
    .name = "host_hugepages_node_surplus_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "The size in bytes of huge pages in the pool above the value in /proc/sys/vm/nr_hugepages"
  },
};

static int hp_config(oconfig_item_t *ci)
{
  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = ci->children + i;
    if (strcasecmp("ReportPerNodeHP", child->key) == 0)
      cf_util_get_boolean(child, &report_numa);
    else if (strcasecmp("ReportRootHP", child->key) == 0)
      cf_util_get_boolean(child, &report_mm);
    else if (strcasecmp("ValuesPages", child->key) == 0)
      cf_util_get_boolean(child, &values_pages);
    else if (strcasecmp("ValuesBytes", child->key) == 0)
      cf_util_get_boolean(child, &values_bytes);
    else
      ERROR("hugepages plugin : Invalid configuration option: \"%s\".", child->key);
  }

  return 0;
}

static int hp_read_hugepages(int dirfd, const char *path, const char *entry, void *ud)
{
  if (strncmp("hugepages-", entry, strlen("hugepages-")) != 0)
    return 0;

  char *node = ud;

  char *hpage = (char *)entry + strlen("hugepages-");
  long hpage_size = strtol(hpage, NULL, 10);

  metric_t m = {0};
  metric_label_set(&m, "page_size", hpage);
  if (node != NULL)
    metric_label_set(&m, "node", node);

  char hpath[PATH_MAX];

  ssnprintf(hpath, sizeof(hpath), "%s/%s/nr_hugepages", path, entry);
  if (parse_double_file(hpath, &m.value.gauge.float64) == 0) {
    if (values_bytes)
      m.value.gauge.float64 *= hpage_size * 1024;
    if (node == NULL) {
      if (values_pages)
        metric_family_metric_append(&fams[FAM_HOST_HUGEPAGES_NR], m);
      if (values_bytes)
        metric_family_metric_append(&fams[FAM_HOST_HUGEPAGES_NR_BYTES], m);
    } else {
      if (values_pages)
        metric_family_metric_append(&fams[FAM_HOST_HUGEPAGES_NODE_NR], m);
      if (values_bytes)
        metric_family_metric_append(&fams[FAM_HOST_HUGEPAGES_NODE_NR_BYTES], m);
    }
  }

  ssnprintf(hpath, sizeof(hpath), "%s/%s/free_hugepages", path, entry);
  if (parse_double_file(hpath, &m.value.gauge.float64) == 0) {
    if (values_bytes)
      m.value.gauge.float64 *= hpage_size * 1024;
    if (node == NULL) {
      if (values_pages)
        metric_family_metric_append(&fams[FAM_HOST_HUGEPAGES_FREE], m);
      if (values_bytes)
        metric_family_metric_append(&fams[FAM_HOST_HUGEPAGES_FREE_BYTES], m);
    } else {
      if (values_pages)
        metric_family_metric_append(&fams[FAM_HOST_HUGEPAGES_NODE_FREE], m);
      if (values_bytes)
        metric_family_metric_append(&fams[FAM_HOST_HUGEPAGES_NODE_FREE_BYTES], m);
    }
  }

  ssnprintf(hpath, sizeof(hpath), "%s/%s/surplus_hugepages", path, entry);
  if (parse_double_file(hpath, &m.value.gauge.float64) == 0) {
    if (values_bytes)
      m.value.gauge.float64 *= hpage_size * 1024;
    if (node == NULL) {
      if (values_pages)
        metric_family_metric_append(&fams[FAM_HOST_HUGEPAGES_SURPLUS], m);
      if (values_bytes)
        metric_family_metric_append(&fams[FAM_HOST_HUGEPAGES_SURPLUS_BYTES], m);
    } else {
      if (values_pages)
        metric_family_metric_append(&fams[FAM_HOST_HUGEPAGES_NODE_SURPLUS], m);
      if (values_bytes)
        metric_family_metric_append(&fams[FAM_HOST_HUGEPAGES_NODE_SURPLUS_BYTES], m);
    }
  }

  if (node == NULL) {
    ssnprintf(hpath, sizeof(hpath), "%s/%s/resv_hugepages", path, entry);
    if (parse_double_file(hpath, &m.value.gauge.float64) == 0) {
      if (values_pages)
        metric_family_metric_append(&fams[FAM_HOST_HUGEPAGES_RESERVED], m);
      if (values_bytes) {
        m.value.gauge.float64 *= hpage_size * 1024;
        metric_family_metric_append(&fams[FAM_HOST_HUGEPAGES_RESERVED_BYTES], m);
      }
    }
  }

  metric_reset(&m);

  return 0;
}

static int hp_read_node(int dirfd, const char *path, const char *entry, void *ud)
{
  if (strncmp(entry, "node", strlen("node")) != 0)
    return 0;
  char *node = (char *)entry + strlen("node");

  char npath[PATH_MAX];
  ssnprintf(npath, sizeof(npath), "%s/%s/hugepages", path, entry);

  return walk_directory(npath, hp_read_hugepages, node, 0);
}

static int hp_read(void)
{
  if (report_mm)
    walk_directory(sys_mm_hugepages, hp_read_hugepages, NULL, 0);
 
  if (report_numa)
    walk_directory(sys_node, hp_read_node, NULL, 0);

  for (size_t i = 0; i < FAM_HOST_HUGEPAGES_MAX; i++) {
    if (fams[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&fams[i]);
      if (status != 0) {
        ERROR("hugepages plugin: plugin_dispatch_metric_family failed: %s",
              STRERROR(status));
      }
      metric_family_metric_reset(&fams[i]);
    }
  }

  return 0;
}

void module_register(void)
{
  plugin_register_complex_config("hugepages", hp_config);
  plugin_register_read("hugepages", hp_read);
}
