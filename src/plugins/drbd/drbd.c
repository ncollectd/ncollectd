/**
 * collectd - src/drbd.c
 * Copyright (C) 2014  Tim Laszlo
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Tim Laszlo <tim.laszlo at gmail.com>
 **/

/*
 See: http://www.drbd.org/users-guide/ch-admin.html#s-performance-indicators

 version: 8.3.11 (api:88/proto:86-96)
 srcversion: 71955441799F513ACA6DA60
  0: cs:Connected ro:Primary/Secondary ds:UpToDate/UpToDate B r-----
         ns:64363752 nr:0 dw:357799284 dr:846902273 al:34987022 bm:18062 lo:0 \
                                                pe:0 ua:0 ap:0 ep:1 wo:f oos:0
 */

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

static const char *drbd_stats = "/proc/drbd";

enum {
  FAM_DRBD_CONNECTED = 0,
  FAM_DRDB_NODE_ROLE_IS_PRIMARY,
  FAM_DRDB_DISK_STATE_IS_UP_TO_DATE,
  FAM_DRDB_NETWORK_SENT_BYTES,
  FAM_DRDB_NETWORK_RECEIVED_BYTES,
  FAM_DRDB_DISK_WRITTEN_BYTES,
  FAM_DRDB_DISK_READ_BYTES,
  FAM_DRDB_ACTIVITYLOG_WRITES,
  FAM_DRDB_BITMAP_WRITES,
  FAM_DRDB_LOCAL_PENDING,
  FAM_DRDB_REMOTE_PENDING,
  FAM_DRDB_REMOTE_UNACKNOWLEDGED,
  FAM_DRDB_APPLICATION_PENDING,
  FAM_DRDB_EPOCHS,
  FAM_DRDB_OUT_OF_SYNC_BYTES,
  FAM_DRDB_MAX,
};

typedef struct {
  char *field;
  int field_size;
  int fam_num;
} drbd_fam_t;

static drbd_fam_t drbd_fams[] = {
  { "ns", 2, FAM_DRDB_NETWORK_SENT_BYTES },
  { "nr", 2, FAM_DRDB_NETWORK_RECEIVED_BYTES },
  { "dw", 2, FAM_DRDB_DISK_WRITTEN_BYTES },
  { "dr", 2, FAM_DRDB_DISK_READ_BYTES },
  { "al", 2, FAM_DRDB_ACTIVITYLOG_WRITES },
  { "bm", 2, FAM_DRDB_BITMAP_WRITES },
  { "lo", 2, FAM_DRDB_LOCAL_PENDING },
  { "pe", 2, FAM_DRDB_REMOTE_PENDING },
  { "ua", 2, FAM_DRDB_REMOTE_UNACKNOWLEDGED },
  { "ap", 2, FAM_DRDB_APPLICATION_PENDING },
  { "ep", 2, FAM_DRDB_EPOCHS },
  { NULL /* "wo" */ , -1, -1 },
  { "oos", 3, FAM_DRDB_OUT_OF_SYNC_BYTES },
};
static size_t drbd_fams_num = STATIC_ARRAY_SIZE(drbd_fams);

static int drbd_metrics(metric_family_t *fams, long int resource,
                        char **fields, size_t fields_num)
{
  if (resource < 0) {
    WARNING("drbd plugin: Unable to parse resource");
    return EINVAL;
  }

  if (fields_num != drbd_fams_num) {
    WARNING("drbd plugin: Wrong number of fields for "
            "r%ld statistics. Expected %" PRIsz ", got %" PRIsz ".",
            resource, drbd_fams_num, fields_num);
    return EINVAL;
  }

  metric_t m = {0};

  char buffer[64];
  ssnprintf(buffer, sizeof(buffer), "r%ld", resource);
  metric_label_set(&m, "device", buffer);

  for (size_t i = 0; i < drbd_fams_num; i++) {
    if (drbd_fams[i].field == NULL)
      continue;
    if (strncmp(drbd_fams[i].field, fields[i], drbd_fams[i].field_size) == 0)
      return EINVAL;
    char *data = strchr(fields[i], ':');
    if (data == NULL)
      return EINVAL;

    value_t value;
    (void)parse_value(++data, &value, DS_TYPE_COUNTER);

    metric_family_t *fam = &fams[drbd_fams[i].fam_num];
    if (fam->type == METRIC_TYPE_COUNTER) {
      m.value.counter = value.counter;
    } else {
      m.value.gauge = value.gauge;
    }

    metric_family_metric_append(fam, m);
  }

  metric_reset(&m);

  return 0;
} 

static int drbd_status(metric_family_t *fams, long int resource,
                       char **fields, size_t fields_num)
{
  if (resource < 0) {
    WARNING("drbd plugin: Unable to parse resource");
    return EINVAL;
  }

  if (fields_num  < 4) {
    WARNING("drbd plugin: Wrong number of fields");
    return EINVAL;
  }

  metric_t m = {0};

  char buffer[64];
  ssnprintf(buffer, sizeof(buffer), "r%ld", resource);
  metric_label_set(&m, "device", buffer);

  if (strncmp("cs", fields[1], 2) == 0) {
    char *data = strchr(fields[1], ':');
    if (data != NULL) {
      m.value.gauge = 0;
      if (strncmp(data, "Connected", strlen("Connected")) == 0)
        m.value.gauge = 1;
      metric_family_metric_append(&fams[FAM_DRBD_CONNECTED], m);
      
    }
  }

  if (strncmp("ro", fields[2], 2) == 0) {
    char *data = strchr(fields[2], ':');
    if (data != NULL) {
      m.value.gauge = 0;
      if (strncmp(data, "Primary", strlen("Primary")) == 0)
        m.value.gauge = 1;
      metric_family_metric_append(&fams[FAM_DRDB_NODE_ROLE_IS_PRIMARY], m);
    }
  }

  if (strncmp("ds", fields[3], 2) == 0) {
    char *data = strchr(fields[3], ':');
    if (data != NULL) {
      m.value.gauge = 0;
      if (strncmp(data, "UpToDate", strlen("UpToDate")) == 0)
        m.value.gauge = 1;
      metric_family_metric_append(&fams[FAM_DRDB_DISK_STATE_IS_UP_TO_DATE], m);
    }
  }

  metric_reset(&m);

  return 0;
} 

static int drbd_read(void)
{
  metric_family_t fams[FAM_DRDB_MAX] = {
    [FAM_DRBD_CONNECTED] = {
      .name = "drbd_connected",
      .help = "Whether DRBD is connected to the peer.",
      .type = METRIC_TYPE_GAUGE,
    },
    [FAM_DRDB_NODE_ROLE_IS_PRIMARY] = {
      .name = "drdb_node_role_is_primary",
      .help = "Whether the role of the node is in the primary state.",
      .type = METRIC_TYPE_GAUGE,
    },
    [FAM_DRDB_DISK_STATE_IS_UP_TO_DATE] = {
      .name = "drdb_disk_state_is_up_to_date",
      .help = "Whether the disk of the node is up to date.",
      .type = METRIC_TYPE_GAUGE,
    },
    [FAM_DRDB_NETWORK_SENT_BYTES] = {
      .name = "drdb_network_sent_bytes_total",
      .help = "Total number of bytes sent via the network.",
      .type = METRIC_TYPE_COUNTER,
    },
    [FAM_DRDB_NETWORK_RECEIVED_BYTES] = {
      .name = "drdb_network_received_bytes_total",
      .help = "Total number of bytes received via the network.",
      .type = METRIC_TYPE_COUNTER,
    },
    [FAM_DRDB_DISK_WRITTEN_BYTES] = {
      .name = "drdb_disk_written_bytes_total",
      .help = "Net data written on local hard disk; in bytes.",
      .type = METRIC_TYPE_COUNTER,
    },
    [FAM_DRDB_DISK_READ_BYTES] = {
      .name = "drdb_disk_read_bytes_total",
      .help = "Net data read from local hard disk; in bytes.",
      .type = METRIC_TYPE_COUNTER,
    },
    [FAM_DRDB_ACTIVITYLOG_WRITES] = {
      .name = "drdb_activitylog_writes_total",
      .help = "Number of updates of the activity log area of the meta data.",
      .type = METRIC_TYPE_COUNTER,
    },
    [FAM_DRDB_BITMAP_WRITES] = {
      .name = "drdb_bitmap_writes_total",
      .help = "Number of updates of the bitmap area of the meta data.",
      .type = METRIC_TYPE_COUNTER,
    },
    [FAM_DRDB_LOCAL_PENDING] = {
      .name = "drdb_local_pending",
      .help = "Number of open requests to the local I/O sub-system.",
      .type = METRIC_TYPE_GAUGE,
    },
    [FAM_DRDB_REMOTE_PENDING] = {
      .name = "drdb_remote_pending",
      .help = "Number of requests sent to the peer, but that have not yet been answered by the latter.",
      .type = METRIC_TYPE_GAUGE,
    },
    [FAM_DRDB_REMOTE_UNACKNOWLEDGED] = {
      .name = "drdb_remote_unacknowledged",
      .help = "Number of requests received by the peer via the network connection, but that have not yet been answered.",
      .type = METRIC_TYPE_GAUGE,
    },
    [FAM_DRDB_APPLICATION_PENDING] = {
      .name = "drdb_application_pending",
      .help = "Number of block I/O requests forwarded to DRBD, but not yet answered by DRBD.",
      .type = METRIC_TYPE_GAUGE,
    },
    [FAM_DRDB_EPOCHS] = {
      .name = "drdb_epochs",
      .help = "Number of Epochs currently on the fly.",
      .type = METRIC_TYPE_GAUGE,
    },
    [FAM_DRDB_OUT_OF_SYNC_BYTES] = {
      .name = "drdb_out_of_sync_bytes",
      .help = "Amount of data known to be out of sync; in bytes.",
      .type = METRIC_TYPE_GAUGE,
    },
  };


  FILE *fh = fopen(drbd_stats, "r");
  if (fh == NULL) {
    WARNING("drbd plugin: Unable to open %s", drbd_stats);
    return EINVAL;
  }

  char buffer[256];
  long int resource = -1;
  char *fields[16];

  while (fgets(buffer, sizeof(buffer), fh) != NULL) {
    int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));

    /* ignore headers (first two iterations) */
    if ((strcmp(fields[0], "version:") == 0) ||
        (strcmp(fields[0], "srcversion:") == 0) ||
        (strcmp(fields[0], "GIT-hash:") == 0)) {
      continue;
    }

    if (isdigit(fields[0][0])) {
      /* parse the resource line, next loop iteration
         will submit values for this resource */
      resource = strtol(fields[0], NULL, 10);
      drbd_status(fams, resource, fields, fields_num);
    } else {
      /* handle stats data for the resource defined in the
         previous iteration */
      drbd_metrics(fams, resource, fields, fields_num);
    }
  }

  for (size_t i = 0; i < FAM_DRDB_MAX; i++) {
    if (fams[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&fams[i]);
      if (status != 0) {
        ERROR("drdb plugin: plugin_dispatch_metric_family failed: %s", STRERROR(status));
      }
      metric_family_metric_reset(&fams[i]);
    }
  }

  fclose(fh);
  return 0;
}

void module_register(void)
{
  plugin_register_read("drbd", drbd_read);
}
