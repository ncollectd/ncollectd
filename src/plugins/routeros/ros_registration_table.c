// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2009 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at verplant.org>

// from librouteros - src/registration_table.c

#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "ros_api.h"
#include "ros_parse.h"

/*
Status: re
  Param 0: .id = *C
  Param 1: interface = wlan2
  Param 2: radio-name = 000C423AECCF
  Param 3: mac-address = 00:0C:42:3A:EC:CF
  Param 4: ap = true
  Param 5: wds = false
  Param 6: rx-rate = 58.5Mbps-HT
  Param 7: tx-rate = 52.0Mbps-HT
  Param 8: packets = 6962070,10208268
  Param 9: bytes = 1090924066,2872515632
  Param 10: frames = 6662813,9698341
  Param 11: frame-bytes = 1111022872,2846535122
  Param 12: hw-frames = 8338229,9718084
  Param 13: hw-frame-bytes = 1941643579,3255546365
  Param 14: tx-frames-timed-out = 0
  Param 15: uptime = 2w6d13:21:53
  Param 16: last-activity = 00:00:00.060
  Param 17: signal-strength = -74dBm@6Mbps
  Param 18: signal-to-noise = 42
  Param 19: strength-at-rates = -74dBm@6Mbps 10ms,-78dBm@9Mbps 2w2d16h56m20s890ms,-78dBm@12Mbps 2w2d16h55m51s670ms,-77dBm@18Mbps 2w2d16h55m35s880ms,-78dBm@24Mbps 2w2d16h55m5s590ms,-75dBm@36Mbps 1d10h58m48s840ms,-75dBm@48Mbps 1d10h48m34s130ms,-77dBm@54Mbps 1d10h47m34s680ms,-73dBm@HT20-4 38m45s600ms,-73dBm@HT20-5 4m700ms,-74dBm@HT20-6 60ms,-78dBm@HT20-7 1d10h30m34s410ms
  Param 20: tx-signal-strength = -76
  Param 21: tx-ccq = 51
  Param 22: rx-ccq = 77
  Param 23: p-throughput = 36877
  Param 24: ack-timeout = 30
  Param 25: nstreme = false
  Param 26: framing-mode = none
  Param 27: routeros-version = 4.2
  Param 28: last-ip = 62.128.1.53
  Param 29: 802.1x-port-enabled = true
  Param 30: authentication-type = wpa2-psk
  Param 31: encryption = aes-ccm
  Param 32: group-encryption = aes-ccm
  Param 33: compression = false
  Param 34: wmm-enabled = true
===
Status: done
===
 */

typedef struct {
    ros_registration_table_handler_t handler;
    void *user_data;
} rt_internal_data_t;

static ros_registration_table_t *rt_reply_to_regtable (const ros_reply_t *r)
{
    ros_registration_table_t *ret;

    if (r == NULL)
        return (NULL);

    if (strcmp ("re", ros_reply_status (r)) != 0)
        return (rt_reply_to_regtable (ros_reply_next (r)));

    ret = malloc (sizeof (*ret));
    if (ret == NULL)
        return (NULL);
    memset (ret, 0, sizeof (*ret));

    ret->interface = ros_reply_param_val_by_key (r, "interface");
    ret->radio_name = ros_reply_param_val_by_key (r, "radio-name");
    ret->mac_address = ros_reply_param_val_by_key (r, "mac-address");

    ret->ap = ros_sstrtob (ros_reply_param_val_by_key (r, "ap"));
    ret->wds = ros_sstrtob (ros_reply_param_val_by_key (r, "wds"));

    ret->rx_rate = ros_sstrtod (ros_reply_param_val_by_key (r, "rx-rate"));
    ret->tx_rate = ros_sstrtod (ros_reply_param_val_by_key (r, "tx-rate"));

    ros_sstrto_rx_tx_counters (ros_reply_param_val_by_key (r, "packets"),
            &ret->rx_packets, &ret->tx_packets);
    ros_sstrto_rx_tx_counters (ros_reply_param_val_by_key (r, "bytes"),
            &ret->rx_bytes, &ret->tx_bytes);
    ros_sstrto_rx_tx_counters (ros_reply_param_val_by_key (r, "frames"),
            &ret->rx_frames, &ret->tx_frames);
    ros_sstrto_rx_tx_counters (ros_reply_param_val_by_key (r, "frame-bytes"),
            &ret->rx_frame_bytes, &ret->tx_frame_bytes);
    ros_sstrto_rx_tx_counters (ros_reply_param_val_by_key (r, "hw-frames"),
            &ret->rx_hw_frames, &ret->tx_hw_frames);
    ros_sstrto_rx_tx_counters (ros_reply_param_val_by_key (r, "hw-frame-bytes"),
            &ret->rx_hw_frame_bytes, &ret->tx_hw_frame_bytes);

    ret->rx_signal_strength = ros_sstrtod (ros_reply_param_val_by_key (r, "signal-strength"));
    ret->tx_signal_strength = ros_sstrtod (ros_reply_param_val_by_key (r, "tx-signal-strength"));
    ret->signal_to_noise = ros_sstrtod (ros_reply_param_val_by_key (r, "signal-to-noise"));

    ret->rx_ccq = ros_sstrtod (ros_reply_param_val_by_key (r, "rx-ccq"));
    ret->tx_ccq = ros_sstrtod (ros_reply_param_val_by_key (r, "tx-ccq"));

    ret->next = rt_reply_to_regtable (ros_reply_next (r));

    return (ret);
}

static void rt_regtable_free (ros_registration_table_t *r)
{
    while (r != NULL) {
        ros_registration_table_t *next = r->next;
        free (r);
        r = next;
    }
}

static int rt_internal_handler (ros_connection_t *c, const ros_reply_t *r, void *user_data)
{
    ros_registration_table_t *rt_data;
    rt_internal_data_t *internal_data;
    int status;

    rt_data = rt_reply_to_regtable (r);
    if (rt_data == NULL)
        return (errno);

    internal_data = user_data;

    status = internal_data->handler (c, rt_data, internal_data->user_data);

    rt_regtable_free (rt_data);

    return (status);
}

int ros_registration_table (ros_connection_t *c,
                            ros_registration_table_handler_t handler, void *user_data)
{
    rt_internal_data_t data;

    if ((c == NULL) || (handler == NULL))
        return (EINVAL);

    data.handler = handler;
    data.user_data = user_data;

    return ros_query (c, "/interface/wireless/registration-table/print",
                      /* args_num = */ 0, /* args = */ NULL, rt_internal_handler, &data);
}
