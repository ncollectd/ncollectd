#include "collectd.h"
#include "plugin.h"
#include "utils/common/common.h"
#include "utils/avltree/avltree.h"

static const char *proc_arp = "/proc/net/arp";

typedef struct {
  value_t num;
  char device[];
} arp_device_t;

static int arp_read(void)
{
  metric_family_t fam_arp = {
    .name = "host_arp_entries",
    .help = "ARP entries by device",
    .type = METRIC_TYPE_GAUGE,
  };

  FILE *fh = fopen(proc_arp, "r");
  if (fh == NULL) {
    WARNING("arp plugin: Unable to open %s", proc_arp);
    return EINVAL;
  }

  c_avl_tree_t *tree = c_avl_create((int (*)(const void *, const void *))strcmp);
  if (tree == NULL) {
    fclose(fh);
    return -1;
  }

  char buffer[256];
  char *fields[8];
  while (fgets(buffer, sizeof(buffer), fh) != NULL) {
    int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));

    if (fields_num < 6)
      continue;
    if (strcmp(fields[0], "IP") == 0)
      continue;

    char *device = fields[5];
    if (device == NULL)
      continue;

    size_t len = strlen(device);
    if (len == 0)
       continue;

    arp_device_t *arp_device = NULL;
    int status = c_avl_get(tree, device, (void *)&arp_device);
    if (status == 0) {
      assert(arp_device != NULL);
      arp_device->num.gauge++;
      continue;
    }

    arp_device = calloc(1, sizeof(*arp_device)+len+1);
    if (arp_device == NULL) {
      return -1;
      // FIXME break
    }

    arp_device->num.gauge = 1;
    memcpy(arp_device->device, device, len+1);
    status = c_avl_insert(tree, arp_device->device, arp_device);
    if (status != 0) { 
      sfree(arp_device);
      continue;
    }
  }

  while (42) {
    arp_device_t *arp_device = NULL;
    char *key = NULL;
    int status = c_avl_pick(tree, (void *)&key, (void *)&arp_device);
    if (status != 0)
      break;

    metric_family_append(&fam_arp, "device", key, arp_device->num, NULL);
    sfree(arp_device);
  }

  c_avl_destroy(tree);

  int status = plugin_dispatch_metric_family(&fam_arp);
  if (status != 0) {
    ERROR("arp plugin: plugin_dispatch_metric_family failed: %s", STRERROR(status));
  }
  metric_family_metric_reset(&fam_arp);

  fclose(fh);
  return 0;
}

void module_register(void)
{
  plugin_register_read("arp", arp_read);
}
