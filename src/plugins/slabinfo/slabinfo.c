// SPDX-License-Identifier: GPL-2.0-only

#include "collectd.h"
#include "plugin.h"
#include "utils/common/common.h"

static const char *proc_slabinfo = "/proc/slabinfo";

enum {
  FAM_SLABINFO_OBJECTS_ACTIVE,
  FAM_SLABINFO_OBJECTS,
  FAM_SLABINFO_OBJECT_BYTES,
  FAM_SLABINFO_SLAB_OBJECTS,
  FAM_SLABINFO_SLAB_BYTES,
  FAM_SLABINFO_SLABS_ACTIVE,
  FAM_SLABINFO_SLABS,
  FAM_SLABINFO_MAX,
};

static metric_family_t fams[FAM_SLABINFO_MAX] = {
  [FAM_SLABINFO_OBJECTS_ACTIVE] = {
    .name = "host_slabinfo_objects_active",
    .type = METRIC_TYPE_GAUGE,
    .help = "The number of objects that are currently active (i.e., in use).",
  },
  [FAM_SLABINFO_OBJECTS] = {
    .name = "host_slabinfo_objects",
    .type = METRIC_TYPE_GAUGE,
    .help = "The total number of allocated objects (i.e., objects that are both in use and not in use).",
  },
  [FAM_SLABINFO_OBJECT_BYTES] = {
    .name = "host_slabinfo_object_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "The size of objects in this slab, in bytes.",
  },
  [FAM_SLABINFO_SLAB_OBJECTS] = {
    .name = "host_slabinfo_slab_objects",
    .type = METRIC_TYPE_GAUGE,
    .help = "The number of objects stored in each slab.",
  },
  [FAM_SLABINFO_SLAB_BYTES] = {
    .name = "host_slabinfo_slab_bytes",
    .type = METRIC_TYPE_GAUGE,
    .help = "The number of pages allocated for each slab.",
  },
  [FAM_SLABINFO_SLABS_ACTIVE] = {
    .name = "host_slabinfo_slabs_active",
    .type = METRIC_TYPE_GAUGE,
    .help = "The number of active slabs.",
  },
  [FAM_SLABINFO_SLABS] = {
    .name = "host_slabinfo_slabs",
    .type = METRIC_TYPE_GAUGE,
    .help = "The total number of slabs.",
  },
};

static int64_t pagesize;

static int slabinfo_read(void)
{
  FILE *fh = fopen(proc_slabinfo, "r");
  if (fh == NULL) {
    WARNING("slabinfo plugin: Unable to open %s", proc_slabinfo);
    return -1;
  }

  char buffer[512];
  char *fields[16];
  if (fgets(buffer, sizeof(buffer), fh) == NULL) {
    WARNING("slabinfo plugin: Unable to read %s", proc_slabinfo);
    fclose(fh);
    return -1;
  }

  if (strcmp(buffer, "slabinfo - version: 2.1\n") != 0) {
    WARNING("slabinfo plugin: invalid version");
    fclose(fh);
    return -1;
  }

  while(fgets(buffer, sizeof(buffer), fh) != NULL) {
    if (buffer[0] == '#')
      continue;

    int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));

    if (fields_num < 16)
      continue;
    value_t value = {0}; 
   
    value.gauge.float64 = (double)atof(fields[1]);
    metric_family_append(&fams[FAM_SLABINFO_OBJECTS_ACTIVE], "cache_name", fields[0], value, NULL);
    value.gauge.float64 = (double)atof(fields[2]);
    metric_family_append(&fams[FAM_SLABINFO_OBJECTS], "cache_name", fields[0], value, NULL);
    value.gauge.float64 = (double)atof(fields[3]);
    metric_family_append(&fams[FAM_SLABINFO_OBJECT_BYTES], "cache_name", fields[0], value, NULL);
    value.gauge.float64 = (double)atof(fields[4]);
    metric_family_append(&fams[FAM_SLABINFO_SLAB_OBJECTS], "cache_name", fields[0], value, NULL);
    value.gauge.float64 = (double)atof(fields[5]) * pagesize;
    metric_family_append(&fams[FAM_SLABINFO_SLAB_BYTES], "cache_name", fields[0], value, NULL);
    value.gauge.float64 = (double)atof(fields[13]);
    metric_family_append(&fams[FAM_SLABINFO_SLABS_ACTIVE], "cache_name", fields[0], value, NULL);
    value.gauge.float64 = (double)atof(fields[14]);
    metric_family_append(&fams[FAM_SLABINFO_SLABS], "cache_name", fields[0], value, NULL);
  }

  for(size_t i = 0; i < FAM_SLABINFO_MAX; i++) {
    if (fams[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&fams[i]);
      if (status != 0) {
        ERROR("slabinfo plugin: plugin_dispatch_metric_family failed: %s", STRERROR(status));
      }
      metric_family_metric_reset(&fams[i]);
    }
  }

  fclose(fh);
  return 0;
}

static int slabinfo_init(void)
{
  pagesize = (int64_t)sysconf(_SC_PAGESIZE);
  return 0;
}

void module_register(void)
{
  plugin_register_init("slabinfo", slabinfo_init);
  plugin_register_read("slabinfo", slabinfo_read);
}
