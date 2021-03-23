// SPDX-License-Identifier: GPL-2.0
#include "collectd.h"
#include "plugin.h"
#include "utils/common/common.h"

static const char *proc_schedstat = "/proc/schedstat";

enum {
  FAM_SCHEDSTAT_RUNNING = 0,
  FAM_SCHEDSTAT_WAITING,
  FAM_SCHEDSTAT_TIMESLICES,
  FAM_SCHEDSTAT_MAX,
};

static int schedstat_read(void)
{
  metric_family_t fams[FAM_SCHEDSTAT_MAX]= {
    [FAM_SCHEDSTAT_RUNNING] = {
      .name = "host_schedstat_running_total",
      .help = "Number of jiffies spent running a process.",
      .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SCHEDSTAT_WAITING] = {
      .name = "host_schedstat_waiting_total",
      .help = "Number of jiffies waiting for this CPU.",
      .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SCHEDSTAT_TIMESLICES] = {
      .name = "host_schedstat_timeslices_total",
      .help = "Number of timeslices executed by CPU.",
      .type = METRIC_TYPE_COUNTER,
    },
  };

  FILE *fh = fopen(proc_schedstat, "r");
  if (fh == NULL) {
    WARNING("schedstat plugin: Unable to open %s", proc_schedstat);
    return EINVAL;
  }

  char buffer[4096];
  char *fields[16];

  while(fgets(buffer, sizeof(buffer), fh) != NULL) {
    int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));

    if (fields_num < 10)
      continue;

    if (strncmp(fields[0], "cpu", 3) != 0)
      continue;

    char *ncpu = fields[0] + 3;
    value_t value; 
   
    value.counter = (counter_t)strtol(fields[7], NULL, 10); 
    metric_family_append(&fams[FAM_SCHEDSTAT_RUNNING], "cpu", ncpu, value, NULL);

    value.counter = (counter_t)strtol(fields[8], NULL, 10); 
    metric_family_append(&fams[FAM_SCHEDSTAT_WAITING], "cpu", ncpu, value, NULL);

    value.counter = (counter_t)strtol(fields[9], NULL, 10); 
    metric_family_append(&fams[FAM_SCHEDSTAT_TIMESLICES], "cpu", ncpu, value, NULL);
  }

  for(size_t i = 0; i < FAM_SCHEDSTAT_MAX; i++) {
    if (fams[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&fams[i]);
      if (status != 0) {
        ERROR("schedstat plugin: plugin_dispatch_metric_family failed: %s", STRERROR(status));
      }
      metric_family_metric_reset(&fams[i]);
    }
  }

  fclose(fh);
  return 0;
}

void module_register(void)
{
  plugin_register_read("schedstat", schedstat_read);
}
