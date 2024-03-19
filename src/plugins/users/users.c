// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2007 Sebastian Harl
// SPDX-FileCopyrightText: Copyright (C) 2005 Niki W. Waibel
// SPDX-FileCopyrightText: Copyright (C) 2005-2007 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2008 Oleg King
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>
// SPDX-FileContributor: Niki W. Waibel <niki.waibel at newlogic.com>
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Oleg King <king2 at kaluga.ru>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"

#ifdef HAVE_UTMPX_H
#include <utmpx.h>
#elif defined(HAVE_UTMP_H)
#include <utmp.h>
#endif

static metric_family_t fam = {
    .name = "system_users",
    .type = METRIC_TYPE_GAUGE,
    .help = "Number of users currently logged into the system"
};

static int users_read(void)
{
    unsigned int users = 0;
#ifdef HAVE_GETUTXENT
    struct utmpx *entry = NULL;

    /* according to the *utent(3) man page none of the functions sets errno
         in case of an error, so we cannot do any error-checking here */
    setutxent();
    while (NULL != (entry = getutxent())) {
        if (USER_PROCESS == entry->ut_type)
            ++users;
    }
    endutxent();

#elif defines(HAVE_GETUTENT)
    struct utmp *entry = NULL;

    /* according to the *utent(3) man page none of the functions sets errno
         in case of an error, so we cannot do any error-checking here */
    setutent();
    while (NULL != (entry = getutent())) {
        if (USER_PROCESS == entry->ut_type)
            ++users;
    }
    endutent();

#else
#error "No applicable input method."
#endif

    metric_family_append(&fam, VALUE_GAUGE(users), NULL, NULL);

    plugin_dispatch_metric_family(&fam, 0);

    return 0;
}

void module_register(void)
{
    plugin_register_read("users", users_read);
}
