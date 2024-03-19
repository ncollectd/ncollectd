// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2009 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at verplant.org>
// SPDX-FileContributor: Mariusz Bialonczyk <manio at skyboo.net>

// from librouteros - src/system_health.c

#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "ros_api.h"
#include "ros_parse.h"

typedef struct
{
    ros_system_health_handler_t handler;
    void *user_data;
} rt_internal_data_t;

static int rt_reply_to_system_health (const ros_reply_t *r, ros_system_health_t *ret)
{
    if (r == NULL)
        return (EINVAL);

    if (strcmp ("re", ros_reply_status (r)) != 0)
        return (rt_reply_to_system_health (ros_reply_next (r), ret));

    ret->voltage = ros_sstrtod (ros_reply_param_val_by_key (r, "voltage"));
    ret->temperature = ros_sstrtod (ros_reply_param_val_by_key (r, "temperature"));

    return (0);
}

static int sh_internal_handler (ros_connection_t *c, const ros_reply_t *r, void *user_data)
{
    ros_system_health_t sys_health;
    rt_internal_data_t *internal_data;
    int status;

    memset (&sys_health, 0, sizeof (sys_health));
    status = rt_reply_to_system_health (r, &sys_health);
    if (status != 0)
        return (status);

    internal_data = user_data;

    status = internal_data->handler (c, &sys_health, internal_data->user_data);

    return (status);
}

int ros_system_health (ros_connection_t *c, ros_system_health_handler_t handler, void *user_data)
{
    rt_internal_data_t data;

    if ((c == NULL) || (handler == NULL))
        return (EINVAL);

    data.handler = handler;
    data.user_data = user_data;

    return ros_query (c, "/system/health/print",
                         /* args_num = */ 0, /* args = */ NULL, sh_internal_handler, &data);
}
