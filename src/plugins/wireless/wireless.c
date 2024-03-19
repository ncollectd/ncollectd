// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2006-2018 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

#include <linux/if.h>
#include <linux/wireless.h>
#include <sys/ioctl.h>

static char *path_proc_wireless;

enum {
    FAM_WIRELESS_SIGNAL_QUALITY,
    FAM_WIRELESS_SIGNAL_POWER_DBM,
    FAM_WIRELESS_SIGNAL_NOISE_DBM,
    FAM_WIRELESS_BITRATE,
    FAM_WIRELESS_MAX,
};

static metric_family_t fams[FAM_WIRELESS_MAX] = {
    [FAM_WIRELESS_SIGNAL_QUALITY] = {
        .name = "system_wireless_signal_quality",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_WIRELESS_SIGNAL_POWER_DBM] = {
        .name = "system_wireless_signal_power_dbm",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_WIRELESS_SIGNAL_NOISE_DBM] = {
        .name = "system_wireless_signal_noise_dbm",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_WIRELESS_BITRATE] = {
        .name = "system_wireless_bitrate",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
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
    /* there are a variety of names for the wireless device */
    FILE *fh = fopen(path_proc_wireless, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Cannot open '%s': %s", path_proc_wireless, STRERRNO);
        return -1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        PLUGIN_ERROR("socket: %s", STRERRNO);
        fclose(fh);
        return -1;
    }

    int devices_found = 0;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *endptr;
        char *fields[8];
        int numfields = strsplit(buffer, fields, 8);

        if (numfields < 5)
            continue;

        size_t len = strlen(fields[0]) - 1;
        if (len < 1)
            continue;
        if (fields[0][len] != ':')
            continue;
        fields[0][len] = '\0';

        char *device = fields[0];

        double quality = strtod(fields[2], &endptr);
        if (fields[2] == endptr)
            quality = -1.0; /* invalid */

        metric_family_append(&fams[FAM_WIRELESS_SIGNAL_QUALITY], VALUE_GAUGE(quality), NULL,
                             &(label_pair_const_t){.name="device", .value=device}, NULL);

        /* power [dBm] < 0.0 */
        double power = strtod(fields[3], &endptr);
        if (fields[3] == endptr)
            power = 1.0; /* invalid */
        else if ((power >= 0.0) && (power <= 100.0))
            power = wireless_percent_to_power(power);
        else if ((power > 100.0) && (power <= 256.0))
            power = power - 256.0;
        else if (power > 0.0)
            power = 1.0; /* invalid */

        metric_family_append(&fams[FAM_WIRELESS_SIGNAL_POWER_DBM], VALUE_GAUGE(power), NULL,
                             &(label_pair_const_t){.name="device", .value=device}, NULL);

        /* noise [dBm] < 0.0 */
        double noise = strtod(fields[4], &endptr);
        if (fields[4] == endptr)
            noise = 1.0; /* invalid */
        else if ((noise >= 0.0) && (noise <= 100.0))
            noise = wireless_percent_to_power(noise);
        else if ((noise > 100.0) && (noise <= 256.0))
            noise = noise - 256.0;
        else if (noise > 0.0)
            noise = 1.0; /* invalid */

        metric_family_append(&fams[FAM_WIRELESS_SIGNAL_NOISE_DBM], VALUE_GAUGE(noise), NULL,
                             &(label_pair_const_t){.name="device", .value=device}, NULL);

        struct iwreq req = {
                .ifr_ifrn.ifrn_name = {0},
        };
        sstrncpy(req.ifr_ifrn.ifrn_name, device, sizeof(req.ifr_ifrn.ifrn_name));
        if (ioctl(sock, SIOCGIWRATE, &req) == -1) {
            PLUGIN_WARNING("ioctl(SIOCGIWRATE): %s", STRERRNO);
        } else {
            metric_family_append(&fams[FAM_WIRELESS_BITRATE],
                                 VALUE_GAUGE((double)req.u.bitrate.value), NULL,
                                 &(label_pair_const_t){.name="device", .value=device}, NULL);
        }

        devices_found++;
    }

    close(sock);
    fclose(fh);

    /* If no wireless devices are present return an error, so the plugin
     * code delays our read function. */
    if (devices_found == 0)
        return -1;

    plugin_dispatch_metric_family_array(fams, FAM_WIRELESS_MAX, 0);
    return 0;
}

static int wireless_init(void)
{
    path_proc_wireless = plugin_procpath("net/wireless");
    if (path_proc_wireless == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int wireless_shutdown(void)
{
    free(path_proc_wireless);
    return 0;
}

void module_register(void)
{
    plugin_register_init("wireless", wireless_init);
    plugin_register_read("wireless", wireless_read);
    plugin_register_shutdown("wireless", wireless_shutdown);
}
