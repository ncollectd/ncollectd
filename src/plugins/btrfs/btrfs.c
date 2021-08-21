// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2018       Martin Langlotz
// Authors:
//   Martin Langlotz < stackshadow at evilbrain . de >

#include "collectd.h"
#include "plugin.h"
#include "utils/common/common.h"
#include "utils/ignorelist/ignorelist.h"
#include "utils_llist.h"
#include "utils/mount/mount.h"

#include <btrfs/ioctl.h>

#define PLUGIN_NAME "btrfs"
static const char *config_keys[] = {"RefreshMounts"};
static int config_keys_num = STATIC_ARRAY_SIZE(config_keys);
static char btrfs_is_init = 0;
static char btrfs_conf_refresh_always = 0;
static llist_t *llist_btrfs_paths = NULL;

enum {
  FAM_BTRFS_WRITE_ERRORS,
  FAM_BTRFS_READ_ERRORS,
  FAM_BTRFS_FLUSH_ERRORS,
  FAM_BTRFS_CORRUPTION_ERRORS,
  FAM_BTRFS_GENERATION_ERRORS,
  FAM_BTRFS_MAX,
};

static metric_family_t fams[FAM_BTRFS_MAX] = {
  [FAM_BTRFS_WRITE_ERRORS] = {
    .name = "host_brtfs_write_errors",
    .type = METRIC_TYPE_COUNTER,
    .help = "EIO or EREMOTEIO write errors from lower layers.",
  },
  [FAM_BTRFS_READ_ERRORS] = {
    .name = "host_brtfs_read_errors",
    .type = METRIC_TYPE_COUNTER,
    .help = "EIO or EREMOTEIO read errors from lower layers.",
  },
  [FAM_BTRFS_FLUSH_ERRORS] = {
    .name = "host_brtfs_flush_errors",
    .type = METRIC_TYPE_COUNTER,
    .help = "EIO or EREMOTEIO flush errors from lower layers.",
  },
  [FAM_BTRFS_CORRUPTION_ERRORS] = {
    .name = "host_brtfs_corruption_errors",
    .type = METRIC_TYPE_COUNTER,
    .help = "Count of checksums errors, bytenr error or contents is illegal: this is an indication that the block was damaged during read or write, or written to wrong location or read from wrong location.",
  },
  [FAM_BTRFS_GENERATION_ERRORS] = {
    .name = "host_brtfs_generation_errors",
    .type = METRIC_TYPE_COUNTER,
    .help = "An indication that blocks have not been writte.",
  },
};

static int btrfs_mountlist_create()
{
  if (llist_btrfs_paths != NULL) {
    llentry_t *e = llist_head(llist_btrfs_paths);
    while (e != NULL) {
      sfree(e->key);
      e = e->next;
    }
    llist_destroy(llist_btrfs_paths);
  }

  llist_btrfs_paths = llist_create();
  if (llist_btrfs_paths == NULL) {
    return -1;
  }
  return 0;
}

static int btrfs_mountlist_read()
{
  FILE *file_mounts = setmntent("/proc/mounts", "r");
  if (file_mounts == NULL) {
    return -1;
  }

  struct mntent *mount_entry = getmntent(file_mounts);
  while (mount_entry != NULL) {
    if (strncmp(mount_entry->mnt_type, "btrfs", 5) == 0) {
      char *mnt_dir = strdup(mount_entry->mnt_dir);
      llist_append(llist_btrfs_paths, llentry_create(mnt_dir, NULL));
    }
    mount_entry = getmntent(file_mounts);
  }

  endmntent(file_mounts);
  return 0;
}

static int btrfs_init()
{
  if (btrfs_is_init == 1)
    return 0;

  int ret = btrfs_mountlist_create();
  if (ret < 0) {
    return -1;
  }

  ret = btrfs_mountlist_read();
  if (ret < 0) {
    return -1;
  }

  btrfs_is_init = 1;
  return 0;
}

static int btrfs_config(const char *key, const char *value)
{
  int ret = -1;

  ret = btrfs_init();
  if (ret < 0) {
    return -1;
  }

  if (strcasecmp("RefreshMounts", key) == 0) {
    if (strcasecmp("on", value) == 0) {
      btrfs_conf_refresh_always = 1;
      DEBUG("plugin btrfs:Enable refresh on every read \n");
    } else {
      btrfs_conf_refresh_always = 0;
    }
  }

  return 0;
}

static int btrfs_submit_read_stats(char *mount_path)
{
  int ret = 0;

  DIR *dirstream = opendir(mount_path);
  if (dirstream == NULL) {
    ERROR("plugin btrfs: ERROR: open on %s failed %s\n", mount_path, strerror(errno));
    return -1;
  }

  int fd = dirfd(dirstream);
  if (fd < 0) {
    ERROR("plugin btrfs: ERROR: open on %s failed: %s\n", mount_path,
          strerror(errno));
    ret = -1;
    goto onerr;
  }

  struct btrfs_ioctl_fs_info_args fs_args = {0};
  ret = ioctl(fd, BTRFS_IOC_FS_INFO, &fs_args);
  if (ret < 0) {
    ERROR("plugin btrfs: ERROR: ioctl(BTRFS_IOC_FS_INFO) on %s failed: %s\n",
          mount_path, strerror(errno));
    goto onerr;
  }

  struct btrfs_ioctl_get_dev_stats dev_stats_args = {0};
  dev_stats_args.devid = fs_args.max_id;
  dev_stats_args.nr_items = BTRFS_DEV_STAT_VALUES_MAX;
  dev_stats_args.flags = 0;

  ret = ioctl(fd, BTRFS_IOC_GET_DEV_STATS, &dev_stats_args);
  if (ret < 0) {
    ERROR("plugin btrfs: ERROR: ioctl(BTRFS_IOC_GET_DEV_STATS) on %s failed: %s\n",
          mount_path, strerror(errno));
    goto onerr;
  }

  value_t value = {0};

  value.counter.uint64 = dev_stats_args.values[BTRFS_DEV_STAT_WRITE_ERRS];
  metric_family_append(&fams[FAM_BTRFS_WRITE_ERRORS], "path", mount_path, value, NULL);
  value.counter.uint64 = dev_stats_args.values[BTRFS_DEV_STAT_READ_ERRS];
  metric_family_append(&fams[FAM_BTRFS_READ_ERRORS], "path", mount_path, value, NULL);
  value.counter.uint64 = dev_stats_args.values[BTRFS_DEV_STAT_FLUSH_ERRS];
  metric_family_append( &fams[FAM_BTRFS_FLUSH_ERRORS], "path", mount_path, value, NULL);
  value.counter.uint64 = dev_stats_args.values[BTRFS_DEV_STAT_CORRUPTION_ERRS];
  metric_family_append(&fams[FAM_BTRFS_CORRUPTION_ERRORS], "path", mount_path, value, NULL);
  value.counter.uint64 = dev_stats_args.values[BTRFS_DEV_STAT_GENERATION_ERRS];
  metric_family_append(&fams[FAM_BTRFS_GENERATION_ERRORS], "path", mount_path, value, NULL);

  for(size_t i = 0; i < FAM_BTRFS_MAX; i++) {
    if (fams[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&fams[i]);
      if (status != 0) {
        ERROR("plugin btrfs: plugin_dispatch_metric_family failed: %s", STRERROR(status));
      }
      metric_family_metric_reset(&fams[i]);
    }
  }

onerr:
  closedir(dirstream);
  close(fd);
  return ret;
}

static int btrfs_read(void)
{
  int ret = btrfs_init();
  if (ret < 0) {
    return -1;
  }

  if (btrfs_conf_refresh_always == 1) {
    DEBUG("plugin btrfs: Refresh mounts..\n");

    ret = btrfs_mountlist_create();
    if (ret < 0) {
      return -1;
    }

    btrfs_mountlist_read();
  }

  for (llentry_t *e = llist_head(llist_btrfs_paths); e != NULL; e = e->next) {
    btrfs_submit_read_stats(e->key);
  }

  return 0;
}

static int btrfs_shutdown(void)
{

  if (llist_btrfs_paths != NULL) {
    llentry_t *e = llist_head(llist_btrfs_paths);
    while (e != NULL) {
      sfree(e->key);
      e = e->next;
    }
    llist_destroy(llist_btrfs_paths);
  }

  return 0;
}

void module_register(void)
{
  plugin_register_config(PLUGIN_NAME, btrfs_config, config_keys, config_keys_num);
  plugin_register_read(PLUGIN_NAME, btrfs_read);
  plugin_register_shutdown(PLUGIN_NAME, btrfs_shutdown);
}
