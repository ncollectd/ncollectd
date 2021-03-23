// SPDX-License-Identifier: GPL-2.0
#include "collectd.h"
#include "plugin.h"
#include "utils/common/common.h"

#define PRESSURE_CPU "/proc/pressure/cpu"
#define PRESSURE_IO "/proc/pressure/io"
#define PRESSURE_MEMORY "/proc/pressure/memory"

enum {
  FAM_PRESSURE_CPU_WAITING = 0,
  FAM_PRESSURE_IO_WAITING,
  FAM_PRESSURE_IO_STALLED,
  FAM_PRESSURE_MEMORY_WAITING,
  FAM_PRESSURE_MEMORY_STALLED,
  FAM_PRESSURE_MAX,
};

static int pressure_read_file(const char *filename,
                              metric_family_t *fam_waiting,
                              metric_family_t *fam_stalled)
{
  FILE *fh = fopen(filename, "r");
  if (fh == NULL) {
    ERROR("pressure plugin: fopen(\"%s\") failed: %s", filename, STRERRNO);
    return -1;
  }

  char buffer[256];
  while (fgets(buffer, sizeof(buffer), fh) != NULL) {
    char *fields[5] = {NULL};
    metric_t m = {0};

    int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
    if (fields_num != 5)
      continue;

    if (strncmp(fields[4], "total=", strlen("total=")) != 0)
      continue;

    m.value.counter = atoll(fields[4] + strlen("total="));

    if ((strcmp(fields[0], "some") == 0) && (fam_waiting != NULL)) {
      metric_family_metric_append(fam_waiting, m);
    } else if ((strcmp(fields[0], "full") == 0) && (fam_stalled != NULL)) {
      metric_family_metric_append(fam_stalled, m);
    }
  }

  fclose(fh);

  return 0;
}

static int pressure_read(void)
{
  metric_family_t fams[FAM_PRESSURE_MAX] = {
    [FAM_PRESSURE_CPU_WAITING] = {
      .name = "host_pressure_cpu_waiting_total",
      .type = METRIC_TYPE_COUNTER,
    },
    [FAM_PRESSURE_IO_WAITING] = {
      .name = "host_pressure_io_waiting_total",
      .type = METRIC_TYPE_COUNTER,
    },
    [FAM_PRESSURE_IO_STALLED] = {
      .name = "host_pressure_io_stalled_total",
      .type = METRIC_TYPE_COUNTER,
    },
    [FAM_PRESSURE_MEMORY_WAITING] = {
      .name = "host_pressure_memory_waiting_total",
      .type = METRIC_TYPE_COUNTER,
    },
    [FAM_PRESSURE_MEMORY_STALLED] = {
      .name = "host_pressure_memory_stalled_total",
      .type = METRIC_TYPE_COUNTER,
    },
  };

  int status = 0;

  if (pressure_read_file(PRESSURE_CPU, &fams[FAM_PRESSURE_CPU_WAITING], NULL) < 0)
    status++;

  if (pressure_read_file(PRESSURE_IO, &fams[FAM_PRESSURE_IO_WAITING],
                         &fams[FAM_PRESSURE_IO_STALLED]) < 0)
    status++;

  if (pressure_read_file(PRESSURE_MEMORY, &fams[FAM_PRESSURE_MEMORY_WAITING],
                         &fams[FAM_PRESSURE_MEMORY_STALLED]) < 0)
    status++;

  for (size_t i = 0; i < FAM_PRESSURE_MAX; i++) {
    if (fams[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&fams[i]);
      if (status != 0)
        ERROR("pressure plugin: plugin_dispatch_metric_family failed: %s", STRERROR(status));
      metric_family_metric_reset(&fams[i]);
    }
  }

  return status == 3 ? -1 : 0;
}

void module_register(void)
{
  plugin_register_read("pressure", pressure_read);
}
