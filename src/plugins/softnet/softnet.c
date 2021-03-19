#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

static const char *proc_softnet = "/proc/net/softnet_stat";

enum {
  FAM_SOFTNET_PROCESSED = 0,
  FAM_SOFTNET_DROPPED,
  FAM_SOFTNET_TIMES_SQUEEZED,
  FAM_SOFTNET_RECEIVED_RPS,
  FAM_SOFTNET_FLOW_LIMIT,
  FAM_SOFTNET_BACKLOG_LENGTH,
  FAM_SOFTNET_MAX,
};

static int softnet_read(void)
{
  metric_family_t fams[FAM_SOFTNET_MAX]= {
    [FAM_SOFTNET_PROCESSED] = {
      .name = "host_softnet_processed_total",
      .help = "Number of processed packets",
      .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SOFTNET_DROPPED] = {
      .name = "host_softnet_dropped_total",
      .help = "Number of dropped packets",
      .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SOFTNET_TIMES_SQUEEZED] = {
      .name = "host_softnet_times_squeezed_total",
      .help = "Number of times processing packets ran out of quota",
      .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SOFTNET_RECEIVED_RPS] = {
      .name = "host_softnet_received_rps_total",
      .help = "Number of steering packets received",
      .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SOFTNET_FLOW_LIMIT] = {
      .name = "host_softnet_flow_limit_total",
      .help = "Number of times processing packets hit flow limit",
      .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SOFTNET_BACKLOG_LENGTH] = {
      .name = "host_softnet_backlog_length",
      .help = "Number of packets in backlog queue, sum of input queue and process queue",
      .type = METRIC_TYPE_GAUGE,
    },
  };

  FILE *fh = fopen(proc_softnet, "r");
  if (fh == NULL) {
    WARNING("softnet plugin: Unable to open %s", proc_softnet);
    return EINVAL;
  }

  char buffer[256];
  char *fields[16];
  for (int ncpu = 0; fgets(buffer, sizeof(buffer), fh) != NULL ; ncpu++) {
    int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));

    if (fields_num < 6)
      continue;

    value_t value;
    char cpu[64];

    if (fields_num >= 14) {
      int fcpu =  (int)strtol(fields[12], NULL, 16); 
      ssnprintf(cpu, sizeof(cpu), "%d", fcpu);

      value.counter = (counter_t)strtol(fields[13], NULL, 16); 
      metric_family_append(&fams[FAM_SOFTNET_BACKLOG_LENGTH], "cpu", cpu, value, NULL);
    } else {
      ssnprintf(cpu, sizeof(cpu), "%d", ncpu);
    }

    if (fields_num >= 3) {
      value.counter = (counter_t)strtol(fields[0], NULL, 16); 
      metric_family_append(&fams[FAM_SOFTNET_PROCESSED], "cpu", cpu, value, NULL);

      value.counter = (counter_t)strtol(fields[1], NULL, 16); 
      metric_family_append(&fams[FAM_SOFTNET_DROPPED], "cpu", cpu, value, NULL);

      value.counter = (counter_t)strtol(fields[2], NULL, 16); 
      metric_family_append(&fams[FAM_SOFTNET_TIMES_SQUEEZED], "cpu", cpu, value, NULL);

      if (fields_num >= 10) {
        value.counter = (counter_t)strtol(fields[9], NULL, 16); 
        metric_family_append(&fams[FAM_SOFTNET_RECEIVED_RPS], "cpu", cpu, value, NULL);

        if (fields_num >= 11) {
          value.gauge = (gauge_t)strtol(fields[10], NULL, 16); 
          metric_family_append(&fams[FAM_SOFTNET_BACKLOG_LENGTH], "cpu", cpu, value, NULL);
        }
      }
    }
  }

  for(size_t i = 0; i < FAM_SOFTNET_MAX; i++) {
    if (fams[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&fams[i]);
      if (status != 0) {
        ERROR("softnet plugin: plugin_dispatch_metric_family failed: %s", STRERROR(status));
      }
      metric_family_metric_reset(&fams[i]);
    }
  }

  fclose(fh);
  return 0;
}

void module_register(void)
{
  plugin_register_read("softnet", softnet_read);
}
