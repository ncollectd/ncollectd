// SPDX-License-Identifier: GPL-2.0-only

#include "collectd.h"
#include "plugin.h"
#include "utils/common/common.h"

static const char *sys_avc_cache_stats = "/sys/fs/selinux/avc/cache_stats";

enum {
  FAM_AVC_CACHE_LOOKUPS,
  FAM_AVC_CACHE_HITS,
  FAM_AVC_CACHE_MISSES,
  FAM_AVC_CACHE_ALLOCATIONS,
  FAM_AVC_CACHE_RECLAIMS,
  FAM_AVC_CACHE_FREES,
  FAM_AVC_CACHE_MAX,
};

static metric_family_t fams[FAM_AVC_CACHE_MAX] = {
  [FAM_AVC_CACHE_LOOKUPS] = {
    .name = "host_avc_cache_lookups",
    .type = METRIC_TYPE_COUNTER, 
    .help = "Number of access vector lookups.",
  },
  [FAM_AVC_CACHE_HITS] = {
    .name = "host_avc_cache_hits",
    .type = METRIC_TYPE_COUNTER, 
    .help = "Number of access vector hits.",
  },
  [FAM_AVC_CACHE_MISSES] = {
    .name = "host_avc_cache_misses",
    .type = METRIC_TYPE_COUNTER, 
    .help = "Number of cache misses.",
  },
  [FAM_AVC_CACHE_ALLOCATIONS] = {
    .name = "host_avc_cache_allocations",
    .type = METRIC_TYPE_COUNTER, 
    .help = "Number of AVC entries allocated.",
  },
  [FAM_AVC_CACHE_RECLAIMS] = {
    .name = "host_avc_cache_reclaims",
    .type = METRIC_TYPE_COUNTER, 
    .help = "Number of current total reclaimed AVC entries.",
  },
  [FAM_AVC_CACHE_FREES] = {
    .name = "host_avc_cache_frees",
    .type = METRIC_TYPE_COUNTER, 
    .help = "Number of free AVC entries.",
  },
};


static int avccache_read(void)
{
  uint64_t total_lookups = 0;
  uint64_t total_hits = 0;
  uint64_t total_misses = 0;
  uint64_t total_allocations = 0;
  uint64_t total_reclaims = 0;
  uint64_t total_frees = 0;

  FILE *fh = fopen(sys_avc_cache_stats, "r");
  if (fh == NULL) {
    WARNING("avccache plugin: Unable to open %s", sys_avc_cache_stats);
    return EINVAL;
  }

  char buffer[256];
  char *fields[8];

  if (fgets(buffer, sizeof(buffer), fh) == NULL) {
    return 0;
  }

  while(fgets(buffer, sizeof(buffer), fh) != NULL) {
    int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));

    if (fields_num < 6)
      continue;

    total_lookups +=  (uint64_t)strtol(fields[0], NULL, 10);
    total_hits += (uint64_t)strtol(fields[1], NULL, 10);
    total_misses += (uint64_t)strtol(fields[2], NULL, 10);
    total_allocations += (uint64_t)strtol(fields[3], NULL, 10);
    total_reclaims += (uint64_t)strtol(fields[4], NULL, 10);
    total_frees += (uint64_t)strtol(fields[5], NULL, 10);
  }

  metric_family_metric_append(&fams[FAM_AVC_CACHE_LOOKUPS],
                              (metric_t){.value.counter.uint64 = total_lookups});
  metric_family_metric_append(&fams[FAM_AVC_CACHE_HITS],
                              (metric_t){.value.counter.uint64 = total_hits});
  metric_family_metric_append(&fams[FAM_AVC_CACHE_MISSES],
                              (metric_t){.value.counter.uint64 = total_misses});
  metric_family_metric_append(&fams[FAM_AVC_CACHE_ALLOCATIONS],
                              (metric_t){.value.counter.uint64 = total_allocations});
  metric_family_metric_append(&fams[FAM_AVC_CACHE_RECLAIMS],
                              (metric_t){.value.counter.uint64 = total_reclaims});
  metric_family_metric_append(&fams[FAM_AVC_CACHE_FREES],
                              (metric_t){.value.counter.uint64 = total_frees});

  for(size_t i = 0; i < FAM_AVC_CACHE_MAX; i++) {
    if (fams[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&fams[i]);
      if (status != 0) {
        ERROR("avccache plugin: plugin_dispatch_metric_family failed: %s", STRERROR(status));
      }
      metric_family_metric_reset(&fams[i]);
    }
  }

  fclose(fh);
  return 0;
}

void module_register(void)
{
  plugin_register_read("avccache", avccache_read);
}

