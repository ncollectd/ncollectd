// SPDX-License-Identifier: GPL-2.0-only

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

#if !KERNEL_LINUX
#error "No applicable input method."
#endif

#include "meminfo_fam.h"
#include "meminfo.h"

static int meminfo_read(void)
{
  FILE *fh = fopen("/proc/meminfo", "r");
  if (fh == NULL) {
    ERROR("meminfo plugin: fopen(\"/proc/meminfo\") failed: %s", STRERRNO);
    return -1;
  }

  char buffer[256];
  while (fgets(buffer, sizeof(buffer), fh) != NULL) {
    char *fields[4] = {NULL};

    int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
    if (fields_num < 2)
      continue;
 
    const struct meminfo_metric *mm = meminfo_get_key(fields[0], strlen(fields[0]));
    if (mm == NULL)
      continue;

    double value = atof(fields[1]);

    if (fields_num == 3) {
      if ((fields[2][0] == 'k') && (fields[2][1] == 'B')) {
        value *= 1024.0;
      } else {
        continue;
      }
    }

    if (!isfinite(value))
      continue;
  
    metric_family_metric_append(&fams[mm->fam], (metric_t){.value.gauge.float64 = value});
  }

  for (int i = 0; i < FAM_MEMINFO_MAX; i++) {
    if (fams[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&fams[i]);
      if (status != 0) {
        ERROR("plugin meminfo: plugin_dispatch_metric_family failed: %s", STRERROR(status));
      }
      metric_family_metric_reset(&fams[i]);
    }
  }

  if (fclose(fh))
    WARNING("meminfo plugin: fclose failed: %s", STRERRNO);

  return 0;
}

void module_register(void)
{
  plugin_register_read("meminfo", meminfo_read);
}
