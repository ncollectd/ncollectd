// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "libmetric/notification.h"

int notification_proto(strbuf_t *buf, notification_t const *n)
{
    if ((buf == NULL) || (n == NULL))
        return EINVAL;


    return 0;
}
