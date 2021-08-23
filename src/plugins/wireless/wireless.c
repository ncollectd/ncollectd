// SPDX-License-Identifier: GPL-2.0-only OR MIT
// Copyright (C) 2006-2018  Florian octo Forster
// Authors:
//   Florian octo Forster <octo at collectd.org>

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

#include <linux/if.h>
#include <linux/wireless.h>
#include <sys/ioctl.h>

#define WIRELESS_PROC_FILE "/proc/net/wireless"

enum {
  FAM_WIRELESS_SIGNAL_QUALITY,
  FAM_WIRELESS_SIGNAL_POWER_DBM,
  FAM_WIRELESS_SIGNAL_NOISE_DBM,
  FAM_WIRELESS_BITRATE,
  FAM_WIRELESS_MAX,
};

static metric_family_t fams[FAM_WIRELESS_MAX] = {
  [FAM_WIRELESS_SIGNAL_QUALITY] = {
    .name = "host_wireless_signal_quality",
    .type = METRIC_TYPE_GAUGE,
  },
  [FAM_WIRELESS_SIGNAL_POWER_DBM] = {
    .name = "host_wireless_signal_power_dbm",
    .type = METRIC_TYPE_GAUGE,
  },
  [FAM_WIRELESS_SIGNAL_NOISE_DBM] = {
    .name = "host_wireless_signal_noise_dbm",
    .type = METRIC_TYPE_GAUGE,
  },
  [FAM_WIRELESS_BITRATE] = {
    .name = "host_wireless_bitrate",
    .type = METRIC_TYPE_GAUGE,
  },
};

#if 0
static double wireless_dbm_to_watt (double dbm)
{
	double watt;

	/*
	 * dbm = 10 * log_{10} (1000 * power / W)
	 * power = 10^(dbm/10) * W/1000
	 */

	watt = pow (10.0, (dbm / 10.0)) / 1000.0;

	return watt;
}
#endif

#define POWER_MIN -90.0
#define POWER_MAX -50.0
static double wireless_percent_to_power(double quality)
{
  assert((quality >= 0.0) && (quality <= 100.0));

  return (quality * (POWER_MAX - POWER_MIN)) + POWER_MIN;
} 

static int wireless_read(void)
{
  FILE *fh;
  char buffer[1024];

  char *device;
  double quality;
  double power;
  double noise;

  char *fields[8];
  int numfields;

  int devices_found;
  size_t len;

  /* there are a variety of names for the wireless device */
  if ((fh = fopen(WIRELESS_PROC_FILE, "r")) == NULL) {
    ERROR("wireless plugin: fopen: %s", STRERRNO);
    return -1;
  }

  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock == -1) {
    ERROR("wireless plugin: socket: %s", STRERRNO);
    fclose(fh);
    return -1;
  }

  devices_found = 0;
  while (fgets(buffer, sizeof(buffer), fh) != NULL) {
    char *endptr;

    numfields = strsplit(buffer, fields, 8);

    if (numfields < 5)
      continue;

    len = strlen(fields[0]) - 1;
    if (len < 1)
      continue;
    if (fields[0][len] != ':')
      continue;
    fields[0][len] = '\0';

    device = fields[0];

    quality = strtod(fields[2], &endptr);
    if (fields[2] == endptr)
      quality = -1.0; /* invalid */

    metric_family_append(&fams[FAM_WIRELESS_SIGNAL_QUALITY], "device", device, 
                         (value_t){.gauge.float64 = quality}, NULL);

    /* power [dBm] < 0.0 */
    power = strtod(fields[3], &endptr);
    if (fields[3] == endptr)
      power = 1.0; /* invalid */
    else if ((power >= 0.0) && (power <= 100.0))
      power = wireless_percent_to_power(power);
    else if ((power > 100.0) && (power <= 256.0))
      power = power - 256.0;
    else if (power > 0.0)
      power = 1.0; /* invalid */

    metric_family_append(&fams[FAM_WIRELESS_SIGNAL_POWER_DBM], "device", device,
                         (value_t){.gauge.float64 = power}, NULL);

    /* noise [dBm] < 0.0 */
    noise = strtod(fields[4], &endptr);
    if (fields[4] == endptr)
      noise = 1.0; /* invalid */
    else if ((noise >= 0.0) && (noise <= 100.0))
      noise = wireless_percent_to_power(noise);
    else if ((noise > 100.0) && (noise <= 256.0))
      noise = noise - 256.0;
    else if (noise > 0.0)
      noise = 1.0; /* invalid */

    metric_family_append(&fams[FAM_WIRELESS_SIGNAL_NOISE_DBM], "device", device,
                         (value_t){.gauge.float64 = noise}, NULL);

    struct iwreq req = {
        .ifr_ifrn.ifrn_name = {0},
    };
    sstrncpy(req.ifr_ifrn.ifrn_name, device, sizeof(req.ifr_ifrn.ifrn_name));
    if (ioctl(sock, SIOCGIWRATE, &req) == -1) {
      WARNING("wireless plugin: ioctl(SIOCGIWRATE): %s", STRERRNO);
    } else {
      metric_family_append(&fams[FAM_WIRELESS_BITRATE], "device", device,
                           (value_t){.gauge.float64 = (double)req.u.bitrate.value}, NULL);
    }

    devices_found++;
  }

  close(sock);
  fclose(fh);

  /* If no wireless devices are present return an error, so the plugin
   * code delays our read function. */
  if (devices_found == 0)
    return -1;

  for (size_t i = 0; i < FAM_WIRELESS_MAX; i++) {
    if (fams[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&fams[i]);
      if (status != 0) {
        ERROR("wireless plugin: plugin_dispatch_metric_family failed: %s",
              STRERROR(status));
      }
      metric_family_metric_reset(&fams[i]);
    }
  }

  return 0;
}

void module_register(void)
{
  plugin_register_read("wireless", wireless_read);
}
