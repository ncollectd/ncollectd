// SPDX-License-Identifier: GPL-2.0-only OR ISC
// SPDX-FileCopyrightText: Copyright (C) 2009 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at verplant.org>

// from librouteros - src/system_resource.c

#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "ros_api.h"
#include "ros_parse.h"

typedef struct {
    ros_system_resource_handler_t handler;
    void *user_data;
} rt_internal_data_t;

static int rt_reply_to_system_resource (const ros_reply_t *r, ros_system_resource_t *ret)
{
    if (r == NULL)
        return (EINVAL);

    if (strcmp ("re", ros_reply_status (r)) != 0)
        return (rt_reply_to_system_resource (ros_reply_next (r), ret));

    ret->uptime = ros_sstrtodate (ros_reply_param_val_by_key (r, "uptime"));

    ret->version = ros_reply_param_val_by_key (r, "version");
    ret->architecture_name = ros_reply_param_val_by_key (r, "architecture-name");
    ret->board_name = ros_reply_param_val_by_key (r, "board-name");

    ret->cpu_model = ros_reply_param_val_by_key (r, "cpu");
    ret->cpu_count = ros_sstrtoui (ros_reply_param_val_by_key (r, "cpu-count"));
    ret->cpu_load = ros_sstrtoui (ros_reply_param_val_by_key (r, "cpu-load"));
    ret->cpu_frequency = ros_sstrtoui64 (ros_reply_param_val_by_key (r, "cpu-frequency")) * 1000000;

    ret->free_memory = ros_sstrtoui64 (ros_reply_param_val_by_key (r, "free-memory"));
    ret->total_memory = ros_sstrtoui64 (ros_reply_param_val_by_key (r, "total-memory"));

    ret->free_hdd_space = ros_sstrtoui64 (ros_reply_param_val_by_key (r, "free-hdd-space"));
    ret->total_hdd_space = ros_sstrtoui64 (ros_reply_param_val_by_key (r, "total-hdd-space"));

    ret->write_sect_since_reboot = ros_sstrtoui64 (ros_reply_param_val_by_key (r, "write-sect-since-reboot"));
    ret->write_sect_total = ros_sstrtoui64 (ros_reply_param_val_by_key (r, "write-sect-total"));
    ret->bad_blocks = ros_sstrtoui64 (ros_reply_param_val_by_key (r, "bad-blocks"));

    return (0);
}

static int sr_internal_handler (ros_connection_t *c, const ros_reply_t *r, void *user_data)
{
    ros_system_resource_t sys_res;
    rt_internal_data_t *internal_data;
    int status;

    memset (&sys_res, 0, sizeof (sys_res));
    status = rt_reply_to_system_resource (r, &sys_res);
    if (status != 0)
        return (status);

    internal_data = user_data;

    status = internal_data->handler (c, &sys_res, internal_data->user_data);

    return (status);
}

int ros_system_resource (ros_connection_t *c, ros_system_resource_handler_t handler,
                                              void *user_data)
{
    rt_internal_data_t data;

    if ((c == NULL) || (handler == NULL))
        return (EINVAL);

    data.handler = handler;
    data.user_data = user_data;

    return ros_query (c, "/system/resource/print",
                         /* args_num = */ 0, /* args = */ NULL, sr_internal_handler, &data);
}
