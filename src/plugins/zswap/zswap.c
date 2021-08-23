// SPDX-License-Identifier: GPL-2.0-only

#include "collectd.h"
#include "plugin.h"
#include "utils/common/common.h"

#if !KERNEL_LINUX
#error "No applicable input method."
#endif

enum {
  FAM_ZSWAP_POOL_LIMIT_HIT,
  FAM_ZSWAP_REJECT_RECLAIM_FAIL,
  FAM_ZSWAP_REJECT_ALLOC_FAIL,
  FAM_ZSWAP_REJECT_KMEMCACHE_FAIL,
  FAM_ZSWAP_REJECT_COMPRESS_POOR,
  FAM_ZSWAP_WRITTEN_BACK_PAGES,
  FAM_ZSWAP_DUPLICATE_ENTRY,
  FAM_ZSWAP_STORED_BYTES,
  FAM_ZSWAP_POOL_TOTAL_BYTES,
  FAM_ZSWAP_SAME_FILLED_BYTES,
  FAM_ZSWAP_MAX,
};

static metric_family_t fams[FAM_ZSWAP_MAX] = {
  [FAM_ZSWAP_POOL_LIMIT_HIT] = {
    .name = "host_zswap_pool_limit_hit",
    .type = METRIC_TYPE_COUNTER,
    .help = "Pool limit was hit (see zswap_max_pool_percent module parameter).",
  },
  [FAM_ZSWAP_REJECT_RECLAIM_FAIL] = {
    .name = "host_zswap_reject_reclaim_fail",
    .type = METRIC_TYPE_COUNTER,
    .help = "Store failed due to a reclaim failure after pool limit was reached.",
  },
  [FAM_ZSWAP_REJECT_ALLOC_FAIL] = {
    .name = "host_zswap_reject_alloc_fail",
    .type = METRIC_TYPE_COUNTER,
    .help = "Store failed because underlying allocator could not get memory.",
  },
  [FAM_ZSWAP_REJECT_KMEMCACHE_FAIL] = {
    .name = "host_zswap_reject_kmemcache_fail",
    .type = METRIC_TYPE_COUNTER,
    .help = "Store failed because the entry metadata could not be allocated (rare).",
  },
  [FAM_ZSWAP_REJECT_COMPRESS_POOR] = {
    .name = "host_zswap_reject_compress_poor",
    .type = METRIC_TYPE_COUNTER,
    .help = "Compressed page was too big for the allocator to (optimally) store.",
  },
  [FAM_ZSWAP_WRITTEN_BACK_PAGES] = {
    .name = "host_zswap_written_back_pages",
    .type = METRIC_TYPE_COUNTER,
    .help = "Pages written back when pool limit was reached.",
  },
  [FAM_ZSWAP_DUPLICATE_ENTRY] = {
    .name = "host_zswap_duplicate_entry",
    .type = METRIC_TYPE_COUNTER,
    .help = "Duplicate store was encountered (rare).",
  },
  [FAM_ZSWAP_STORED_BYTES] = {
    .name = "host_zswap_stored_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Compressed bytes currently stored in zswap.",
  },
  [FAM_ZSWAP_POOL_TOTAL_BYTES] = {
    .name = "host_zswap_pool_total_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Total bytes used by the compressed storage.",
  },
  [FAM_ZSWAP_SAME_FILLED_BYTES] = {
    .name = "host_zswap_same_filled_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "Same-value filled pages stored in zswap in bytes.",
  },
};

static uint64_t pagesize;

struct zswap_files {
  bool page;
  char *file;
  unsigned int fam;
};
static struct zswap_files zswap_files[] = {
  { false, "duplicate_entry",       FAM_ZSWAP_DUPLICATE_ENTRY       },
  { false, "pool_limit_hit",        FAM_ZSWAP_POOL_LIMIT_HIT        },
  { false, "reject_alloc_fail",     FAM_ZSWAP_REJECT_ALLOC_FAIL     },
  { false, "reject_compress_poor",  FAM_ZSWAP_REJECT_COMPRESS_POOR  },
  { false, "reject_kmemcache_fail", FAM_ZSWAP_REJECT_KMEMCACHE_FAIL },
  { false, "reject_reclaim_fail",   FAM_ZSWAP_REJECT_RECLAIM_FAIL   },
  { false, "written_back_pages",    FAM_ZSWAP_WRITTEN_BACK_PAGES    },
  { false, "pool_total_size",       FAM_ZSWAP_POOL_TOTAL_BYTES      },
  { true , "same_filled_pages",     FAM_ZSWAP_SAME_FILLED_BYTES     },
  { true , "stored_pages",          FAM_ZSWAP_STORED_BYTES          },
  { true , "pool_pages",            FAM_ZSWAP_POOL_TOTAL_BYTES      },
};

static int zswap_read(void)
{
  int zswap_dir = open("/sys/kernel/debug/zswap", O_RDONLY | O_DIRECTORY);
  if (zswap_dir < 0) {
    ERROR("plugin zswap: error open  %s", STRERROR(errno));
    return -1;
  }

  for(size_t i = 0; i < STATIC_ARRAY_SIZE(zswap_files); i++) {
     value_t value = {0};
     metric_family_t *fam = &fams[zswap_files[i].fam];
     if (fam->type == METRIC_TYPE_COUNTER) {
       int status = filetouint_at(zswap_dir, zswap_files[i].file, &value.counter.uint64);   
       if (status != 0)
         continue;
       if (zswap_files[i].page)
         value.counter.uint64 *= pagesize;
     } else if (fam->type == METRIC_TYPE_GAUGE) {
       int status = filetodouble_at(zswap_dir, zswap_files[i].file, &value.gauge.float64);   
       if (status != 0)
         continue;
       if (zswap_files[i].page)
         value.gauge.float64 *= pagesize;
     }
     metric_family_metric_append(fam, (metric_t){.value = value});
  }

  for (size_t i = 0; i < FAM_ZSWAP_MAX; i++) {
    if (fams[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&fams[i]);
      if (status != 0) {
        ERROR("plugin zswap: plugin_dispatch_metric_family failed: %s",
              STRERROR(status));
      }
      metric_family_metric_reset(&fams[i]);
    }
  }

  close(zswap_dir);
  return 0;
}

static int zswap_init(void)
{
  pagesize = (int64_t)sysconf(_SC_PAGESIZE);
  return 0;
}

void module_register(void)
{
  plugin_register_init("zswap", zswap_init);
  plugin_register_read("zswap", zswap_read);
}
