// SPDX-License-Identifier: GPL-2.0-only

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"
#include "utils/ignorelist/ignorelist.h"

#if !KERNEL_LINUX
#error "No applicable input method."
#endif

static const char *config_keys[] = {"SoftIrq", "IgnoreSelected"};
static int config_keys_num = STATIC_ARRAY_SIZE(config_keys);

static ignorelist_t *ignorelist;

static int softirq_config(const char *key, const char *value)
{
  if (ignorelist == NULL)
    ignorelist = ignorelist_create(/* invert = */ 1);

  if (strcasecmp(key, "SoftIrq") == 0) {
    ignorelist_add(ignorelist, value);
  } else if (strcasecmp(key, "IgnoreSelected") == 0) {
    int invert = 1;
    if (IS_TRUE(value))
      invert = 0;
    ignorelist_set_invert(ignorelist, invert);
  } else {
    return -1;
  }

  return 0;
}

static int softirq_read_data(metric_family_t *fam)
{
  /*
   * Example content:
   *                    CPU0       CPU1       CPU2       CPU3
   *          HI:          0          0          0          0
   *       TIMER:     472857     485158     495586     959256
   *      NET_TX:       1024        843        952      50626
   *      NET_RX:      11825      12586      11671      32979
   *       BLOCK:      36247      45217      32037      31845
   *    IRQ_POLL:          0          0          0          0
   *     TASKLET:          1          1          1          1
   *       SCHED:    9109146    3315427    2641233    4153576
   *     HRTIMER:          0          0          2         76
   *         RCU:    3282442    3150050    3131744    4257753
   */

  FILE *fh = fopen("/proc/softirqs", "r");
  if (fh == NULL) {
    ERROR("softirq plugin: fopen (/proc/softirqs): %s", STRERRNO);
    return -1;
  }

  /* Get CPU count from the first line */
  char cpu_buffer[1024];
  char *cpu_fields[256];
  int cpu_count;

  if (fgets(cpu_buffer, sizeof(cpu_buffer), fh) != NULL) {
    cpu_count = strsplit(cpu_buffer, cpu_fields, STATIC_ARRAY_SIZE(cpu_fields));
    for (int i = 0; i < cpu_count; i++) {
      if (strncmp(cpu_fields[i], "CPU", 3) == 0)
        cpu_fields[i] += 3;
    }
  } else {
    ERROR("softirq plugin: unable to get CPU count from first line "
          "of /proc/softirqs");
    fclose(fh);
    return -1;
  }

  metric_t m = {0};

  char buffer[1024];
  char *fields[256];

  while (fgets(buffer, sizeof(buffer), fh) != NULL) {
    int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
    if (fields_num < 2)
      continue;

    int softirq_values_to_parse = fields_num - 1;
    if (fields_num >= cpu_count + 1)
      softirq_values_to_parse = cpu_count;

    /* First field is softirq name and colon */
    char *softirq_name = fields[0];
    size_t softirq_name_len = strlen(softirq_name);
    if (softirq_name_len < 2)
      continue;

    if (softirq_name[softirq_name_len - 1] != ':')
      continue;

    softirq_name[softirq_name_len - 1] = '\0';
    softirq_name_len--;

    if (ignorelist_match(ignorelist, softirq_name) != 0)
      continue;

    metric_label_set(&m, "softirq",softirq_name);

    for (int i = 1; i <= softirq_values_to_parse; i++) {
      /* Per-CPU value */
      uint64_t v;
      int status = parse_uinteger(fields[i], &v);
      if (status != 0)
        break;
      metric_label_set(&m, "cpu", cpu_fields[i - 1]);
      m.value.counter.uint64 = v;
      metric_family_metric_append(fam, m);
    } 
  }
  metric_reset(&m);

  fclose(fh);

  return 0;
}

static int softirq_read(void)
{
  metric_family_t fam = {
    .name = "host_softirq_total",
    .type = METRIC_TYPE_COUNTER,
  };

  int ret = softirq_read_data(&fam);

  if (fam.metric.num > 0) {
    int status = plugin_dispatch_metric_family(&fam);
    if (status != 0) {
      ERROR("softirq plugin: plugin_dispatch_metric_family failed: %s", STRERROR(status));
      ret = -1;
    }
    metric_family_metric_reset(&fam);
  }

  return ret;
}

void module_register(void)
{
  plugin_register_config("softirq", softirq_config, config_keys, config_keys_num);
  plugin_register_read("softirq", softirq_read);
}
