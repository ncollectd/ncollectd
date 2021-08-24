// SPDX-License-Identifier: GPL-2.0-only

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

#if !KERNEL_LINUX
#error "No applicable input method."
#endif

static const char *proc_locks = "/proc/locks";

enum {
  FAM_LOCK,
  FAM_LOCK_PENDING,
  FAM_LOCK_MAX,
};

enum {
  LOCK_CLASS_POSIX,
  LOCK_CLASS_FLOCK,
  LOCK_CLASS_LEASE,
  LOCK_CLASS_MAX,
};

char *lock_class_name[LOCK_CLASS_MAX] = {
  [LOCK_CLASS_POSIX] = "POSIX",
  [LOCK_CLASS_FLOCK] = "FLOCK",
  [LOCK_CLASS_LEASE] = "LEASE",
};

enum {
  LOCK_TYPE_READ,
  LOCK_TYPE_WRITE,
  LOCK_TYPE_MAX,
};

char *lock_type_name[LOCK_TYPE_MAX] = {
  [LOCK_TYPE_READ]  = "READ",
  [LOCK_TYPE_WRITE] = "WRITE",
};

static metric_family_t fams[FAM_LOCK_MAX] = {
  [FAM_LOCK] = {
    .name = "host_locks",
    .type = METRIC_TYPE_COUNTER,
    .help = "Files currently locked by the kernel.",
  },
  [FAM_LOCK_PENDING] = {
    .name = "host_locks_pending",
    .type = METRIC_TYPE_COUNTER,
    .help = "File locks waiting.",
  },
};

static int locks_read(void)
{
  FILE *fh = fopen(proc_locks, "r");
  if (fh == NULL ) {
    WARNING("plugin locks: fopen(%s) failed: %s", proc_locks, STRERRNO);
    return -1;
  }

  uint64_t counter[FAM_LOCK_MAX][LOCK_CLASS_MAX][LOCK_TYPE_MAX] = {0};

  char buffer[1024];
  while (fgets(buffer, sizeof(buffer), fh) != NULL) {
    char *fields[16];
    int numfields = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
    if (numfields < 5)
      continue;

    /*
     * 47: FLOCK  ADVISORY  WRITE 714044 00:25:9880 0 EOF
     * 47: -> FLOCK  ADVISORY  WRITE 714256 00:25:9880 0 EOF
     */

    size_t kind = FAM_LOCK;
    char *lock_class = fields[1];
    char *lock_type = fields[3];

    if ((fields[1][0] == '-') && (fields[1][1] == '>') && (fields[1][2] == '\0')) {
      lock_class = fields[2];
      lock_type = fields[4];
      kind = FAM_LOCK_PENDING;
    }

    size_t type;
    if (!strcmp(lock_class, "POSIX") ||
        !strcmp(lock_class, "ACCESS") ||
        !strcmp(lock_class, "OFDLCK")) {
      type = LOCK_CLASS_POSIX;
    } else if (!strcmp(lock_class, "FLOCK")) {
      type = LOCK_CLASS_FLOCK;
    } else if (!strcmp(lock_class, "DELEG") ||
               !strcmp(lock_class, "LEASE")) {
      type = LOCK_CLASS_LEASE;
    } else {
      continue;
    }

    if (!strcmp(lock_type, "READ")) {
       counter[kind][type][LOCK_TYPE_READ]++;
    } else if (!strcmp(lock_type, "WRITE")) {
       counter[kind][type][LOCK_TYPE_WRITE]++;
    } else if (!strcmp(lock_type, "RW")) {
       counter[kind][type][LOCK_TYPE_READ]++;
       counter[kind][type][LOCK_TYPE_WRITE]++;
    }
  }

  metric_t tmpl = {0};

  for (size_t j = 0; j < LOCK_CLASS_MAX; j++) {
    metric_label_set(&tmpl, "class", lock_class_name[j]);
    for (size_t i = 0; i < FAM_LOCK_MAX; i++) {
      for (size_t k = 0; k < LOCK_TYPE_MAX; k++) {
        metric_family_append(&fams[i], "type", lock_type_name[k],
                             (value_t){.counter.uint64 = counter[i][j][k]},
                             &tmpl);
      }
    }
  }

  metric_reset(&tmpl);

  for (size_t i = 0; i < FAM_LOCK_MAX; i++) {
    if (fams[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&fams[i]);
      if (status != 0) {
        ERROR("plugin locks: plugin_dispatch_metric_family failed: %s",
              STRERROR(status));
      }
      metric_family_metric_reset(&fams[i]);
    }
  }

  fclose(fh);
  return 0;
}

void module_register(void)
{
  plugin_register_read("locks", locks_read);
}
