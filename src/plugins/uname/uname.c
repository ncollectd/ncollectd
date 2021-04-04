// SPDX-License-Identifier: GPL-2.0
#include "collectd.h"
#include "plugin.h"
#include "utils/common/common.h"

#include <sys/utsname.h>

static metric_family_t fam_uname = {
  .name = "host_uname_info",
  .type = METRIC_TYPE_INFO, 
  .help = "Labeled system information as provided by the uname system call.",
};

static int uname_read(void)
{
  if (fam_uname.metric.num == 0)
    return 0;

  int status = plugin_dispatch_metric_family(&fam_uname);
  if (status != 0)
    ERROR("uname plugin: plugin_dispatch_metric_family failed: %s", STRERROR(status));

  return 0;
}

static int uname_shutdown(void)
{
  return metric_family_metric_reset(&fam_uname);
}

static int uname_init(void)
{
  struct utsname name;

  int status = uname(&name);
  if (status < 0) {
    ERROR("uname plugin: uname failed: %s", STRERRNO);
    return -1;
  }

  metric_t m = {0};
  m.value.gauge = 1;

  metric_label_set(&m, "sysname", name.sysname);
  metric_label_set(&m, "release", name.release);
  metric_label_set(&m, "machine", name.machine);
  metric_label_set(&m, "nodename", name.nodename);
  metric_label_set(&m, "version", name.version);
  
  metric_family_metric_append(&fam_uname, m);
  metric_reset(&m);
  return 0;
}

void module_register(void)
{
  plugin_register_init("uname", uname_init);
  plugin_register_read("uname", uname_read);
  plugin_register_shutdown("uname", uname_shutdown);
}
