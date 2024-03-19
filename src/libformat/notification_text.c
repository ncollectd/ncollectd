// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "libmetric/notification.h"
#include "libmetric/marshal.h"

int notification_identity(strbuf_t *buf, notification_t const *n)
{
    if ((buf == NULL) || (n == NULL))
        return EINVAL;

    int status = strbuf_print(buf, n->name);
    if (n->label.num == 0)
        return status;

    status = status || label_set_marshal(buf, n->label);
    return status;
}

int notification_text(strbuf_t *buf, notification_t const *n)
{
    if ((buf == NULL) || (n == NULL))
        return EINVAL;

    int status = strbuf_print(buf, n->name);

    status = status || label_set_marshal(buf, n->label);
    status = status || label_set_marshal(buf, n->annotation);

    const char *severity = " FAILURE ";
    if (n->severity == NOTIF_WARNING)
        severity = " WARNING ";
    else if (n->severity == NOTIF_OKAY)
        severity = " OKAY ";

    status = status || strbuf_print(buf, severity);
    status = status || strbuf_printf(buf, "%.3f\n", CDTIME_T_TO_DOUBLE(n->time));

    return status;
}


