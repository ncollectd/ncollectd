// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "libutils/buf.h"
#include "libmetric/notification.h"
#include "libformat/notification_proto.h"

int notification_proto(buf_t *buf, notification_t const *n)
{
    if ((buf == NULL) || (n == NULL))
        return EINVAL;


    return 0;
}
