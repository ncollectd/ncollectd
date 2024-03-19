// SPDX-License-Identifier: GPL-2.0-only OR ISC
// SPDX-FileCopyrightText: Copyright (C) 2009 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at verplant.org>

// from librouteros - src/interface.c

#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "ros_api.h"
#include "ros_parse.h"

typedef struct {
    ros_interface_handler_t handler;
    void *user_data;
} rt_internal_data_t;

static ros_interface_t *rt_reply_to_interface (const ros_reply_t *r)
{
    ros_interface_t *ret;

    if (r == NULL)
        return (NULL);

    if (strcmp ("re", ros_reply_status (r)) != 0)
        return (rt_reply_to_interface (ros_reply_next (r)));

    ret = malloc (sizeof (*ret));
    if (ret == NULL)
        return (NULL);
    memset (ret, 0, sizeof (*ret));

    ret->name = ros_reply_param_val_by_key (r, "name");
    ret->type = ros_reply_param_val_by_key (r, "type");
    ret->comment = ros_reply_param_val_by_key (r, "comment");

    if (ros_reply_param_val_by_key (r, "packets") == NULL) {
        ret->rx_packets = ros_sstrtoui64 (ros_reply_param_val_by_key (r, "rx-packet"));
        ret->tx_packets = ros_sstrtoui64 (ros_reply_param_val_by_key (r, "tx-packet"));
        ret->rx_bytes = ros_sstrtoui64 (ros_reply_param_val_by_key (r, "rx-byte"));
        ret->tx_bytes = ros_sstrtoui64 (ros_reply_param_val_by_key (r, "tx-byte"));
        ret->rx_errors = ros_sstrtoui64 (ros_reply_param_val_by_key (r, "rx-error"));
        ret->tx_errors = ros_sstrtoui64 (ros_reply_param_val_by_key (r, "tx-error"));
        ret->rx_drops = ros_sstrtoui64 (ros_reply_param_val_by_key (r, "rx-drop"));
        ret->tx_drops = ros_sstrtoui64 (ros_reply_param_val_by_key (r, "tx-drop"));
    } else {
        ros_sstrto_rx_tx_counters (ros_reply_param_val_by_key (r, "packets"),
                &ret->rx_packets, &ret->tx_packets);
        ros_sstrto_rx_tx_counters (ros_reply_param_val_by_key (r, "bytes"),
                &ret->rx_bytes, &ret->tx_bytes);
        ros_sstrto_rx_tx_counters (ros_reply_param_val_by_key (r, "errors"),
                &ret->rx_errors, &ret->tx_errors);
        ros_sstrto_rx_tx_counters (ros_reply_param_val_by_key (r, "drops"),
                &ret->rx_drops, &ret->tx_drops);
    }

    ret->mtu = ros_sstrtoui (ros_reply_param_val_by_key (r, "mtu"));
    ret->l2mtu = ros_sstrtoui (ros_reply_param_val_by_key (r, "l2mtu"));

    ret->dynamic = ros_sstrtob (ros_reply_param_val_by_key (r, "dynamic"));
    ret->running = ros_sstrtob (ros_reply_param_val_by_key (r, "running"));
    ret->enabled = !ros_sstrtob (ros_reply_param_val_by_key (r, "disabled"));

    ret->next = rt_reply_to_interface (ros_reply_next (r));

    return ret;
}

static void if_interface_free (ros_interface_t *r)
{
    while (r != NULL) {
        ros_interface_t *next = r->next;
        free (r);
        r = next;
    }
}

static int if_internal_handler (ros_connection_t *c, const ros_reply_t *r, void *user_data)
{
    ros_interface_t *if_data;
    rt_internal_data_t *internal_data;
    int status;

    if_data = rt_reply_to_interface (r);
    if (if_data == NULL)
        return (errno);

    internal_data = user_data;

    status = internal_data->handler (c, if_data, internal_data->user_data);

    if_interface_free (if_data);

    return status;
}

int ros_interface (ros_connection_t *c, ros_interface_handler_t handler, void *user_data)
{
    rt_internal_data_t data;

    if ((c == NULL) || (handler == NULL))
        return (EINVAL);

    data.handler = handler;
    data.user_data = user_data;

    return ros_query (c, "/interface/print",
                      /* args_num = */ 0, /* args = */ NULL, if_internal_handler, &data);
}
