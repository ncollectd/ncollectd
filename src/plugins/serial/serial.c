/**
 * collectd - src/serial.c
 * Copyright (C) 2005,2006  David Bacher
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Authors:
 *   David Bacher <drbacher at gmail.com>
 *   Florian octo Forster <octo at collectd.org>
 **/

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

#if !KERNEL_LINUX
#error "No applicable input method."
#endif


static const char *proc_serial = "/proc/tty/driver/serial";
enum {
  FAM_SERIAL_READ = 0,
  FAM_SERIAL_WRITE,
  FAM_SERIAL_FRAMING_ERRORS,
  FAM_SERIAL_PARITY_ERRORS,
  FAM_SERIAL_BREAK_CONDITIONS,
  FAM_SERIAL_OVERRUN_ERRORS,
  FAM_SERIAL_MAX,
}
;
static int serial_read(void)
{
  metric_family_t fams[FAM_SERIAL_MAX] = {
    [FAM_SERIAL_READ] = {
      .name = "host_serial_read_bytes_total",
      .type = METRIC_TYPE_COUNTER,
      .help = "Total bytes read in serial port",
    },
    [FAM_SERIAL_WRITE] = {
      .name = "host_serial_write_bytes_total",
      .type = METRIC_TYPE_COUNTER,
      .help = "Total bytes written in serial port",
    },
    [FAM_SERIAL_FRAMING_ERRORS] = {
      .name = "host_serial_framing_errors_total",
      .type = METRIC_TYPE_COUNTER,
      .help = "Total framing errors (stop bit not found) in serial port",
    },
    [FAM_SERIAL_PARITY_ERRORS] = {
      .name = "host_serial_parity_errrors_total",
      .type = METRIC_TYPE_COUNTER,
      .help = "Total parity errors in serial port",
    },
    [FAM_SERIAL_BREAK_CONDITIONS] = {
      .name = "host_serial_break_conditions_total",
      .type = METRIC_TYPE_COUNTER,
      .help = "Total break conditions in serial port",
    },
    [FAM_SERIAL_OVERRUN_ERRORS] = {
      .name = "host_serial_overrun_errors_total",
      .type = METRIC_TYPE_COUNTER,
      .help = "Total receiver overrun errors in serial port",
    },
  };

  FILE *fh = fopen(proc_serial, "r");
  if (fh == NULL ) {
    WARNING("serial: fopeni(%s): %s", proc_serial, STRERRNO);
    return -1;
  }

  char buffer[1024];
  while (fgets(buffer, sizeof(buffer), fh) != NULL) {
    char *fields[16];
    int numfields = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
    if (numfields < 6)
      continue;

    /*
     * 0: uart:16550A port:000003F8 irq:4 tx:0 rx:0
     * 1: uart:16550A port:000002F8 irq:3 tx:0 rx:0
     */
    size_t len = strlen(fields[0]);
    if (len < 2)
      continue;
    if (fields[0][len - 1] != ':')
      continue;
    fields[0][len - 1] = '\0';

    metric_t m = {0};
    metric_label_set(&m, "line", fields[0]);

    counter_t value = 0;

    for (int i = 1; i < numfields; i++) {
      len = strlen(fields[i]);
      if (len < 4)
        continue;

      switch (fields[i][0]) {
        case 'i':
          if (strncmp(fields[i], "irq:", 4) == 0)
            metric_label_set(&m, "irq", fields[i]+4);
          break;
        case 't':
          if (strncmp(fields[i], "tx:", 3) == 0) {
            if (strtocounter(fields[i] + 3, &value) == 0) {
              m.value.counter = value;
              metric_family_metric_append(&fams[FAM_SERIAL_READ], m);
            }
          }
          break;
        case 'r':
          if (strncmp(fields[i], "rx:", 3) == 0) {
            if (strtocounter(fields[i] + 3, &value) == 0) {
               m.value.counter = value;
               metric_family_metric_append(&fams[FAM_SERIAL_WRITE], m);
            }
          }
          break;
        case 'f':
          if (strncmp(fields[i], "fe:", 3) == 0) {
            if (strtocounter(fields[i] + 3, &value) == 0) {
              m.value.counter = value;
              metric_family_metric_append(&fams[FAM_SERIAL_FRAMING_ERRORS], m);
            }
          }
          break;
        case 'p':
          if (strncmp(fields[i], "pe:", 3) == 0) {
            if (strtocounter(fields[i] + 3, &value) == 0) {
              m.value.counter = value;
              metric_family_metric_append(&fams[FAM_SERIAL_PARITY_ERRORS], m);
            }
          }
          break;
        case 'b':
          if (strncmp(fields[i], "brk:", 4) == 0) {
            if (strtocounter(fields[i] + 4, &value) == 0) {
              m.value.counter = value;
              metric_family_metric_append(&fams[FAM_SERIAL_BREAK_CONDITIONS], m);
            }
          }
          break;
        case 'o':
          if (strncmp(fields[i], "oe:", 3) == 0) {
            if (strtocounter(fields[i] + 3, &value) == 0) {
              m.value.counter = value;
              metric_family_metric_append(&fams[FAM_SERIAL_OVERRUN_ERRORS], m); 
            }
          }
          break;
      }
    }

    metric_reset(&m);
  }

  for (size_t i = 0; i < FAM_SERIAL_MAX; i++) {
    if (fams[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&fams[i]);
      if (status != 0) {
        ERROR("serial plugin: plugin_dispatch_metric_family failed: %s",
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
  plugin_register_read("serial", serial_read);
}
