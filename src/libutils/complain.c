// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2006-2013 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2008  Sebastian tokkee Harl
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Sebastian tokkee Harl <sh at tokkee.org>

#include "ncollectd.h"
#include "log.h"
#include "libutils/complain.h"

/* vcomplain returns 0 if it did not report, 1 else */
__attribute__((format(printf, 3, 0))) static int
vcomplain(int level, c_complain_t *c, const char *format, va_list ap)
{
    cdtime_t now;
    char message[512];

    now = cdtime();

    if (c->last + c->interval > now)
        return 0;

    c->last = now;
#if 0
    if (c->interval < plugin_get_interval())
        c->interval = plugin_get_interval();
    else
        c->interval *= 2;
#endif
    c->interval *= 2;

    if (c->interval > TIME_T_TO_CDTIME_T(86400))
        c->interval = TIME_T_TO_CDTIME_T(86400);

    vsnprintf(message, sizeof(message), format, ap);
    message[sizeof(message) - 1] = '\0';

    plugin_log(level, NULL, 0, NULL, "%s", message);
    return 1;
}

void c_complain(int level, c_complain_t *c, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    if (vcomplain(level, c, format, ap))
        c->complained_once = true;
    va_end(ap);
}

void c_complain_once(int level, c_complain_t *c, const char *format, ...)
{
    va_list ap;

    if (c->complained_once)
        return;

    va_start(ap, format);
    if (vcomplain(level, c, format, ap))
        c->complained_once = true;
    va_end(ap);
}

void c_do_release(int level, c_complain_t *c, const char *format, ...)
{
    char message[512];
    va_list ap;

    if (c->interval == 0)
        return;

    c->interval = 0;
    c->complained_once = false;

    va_start(ap, format);
    vsnprintf(message, sizeof(message), format, ap);
    message[sizeof(message) - 1] = '\0';
    va_end(ap);

    plugin_log(level, NULL, 0, NULL, "%s", message);
}
