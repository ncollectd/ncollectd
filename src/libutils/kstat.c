// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2005-2014 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Niki W. Waibel <niki.waibel at gmx.net>
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>
// SPDX-FileContributor: Michał Mirosław <mirq-linux at rere.qmqm.pl>
// SPDX-FileContributor: Manoj Srivastava <srivasta at google.com>

#include "ncollectd.h"
#include "log.h"
#include "libutils/common.h"

#include <kstat.h>

int get_kstat(kstat_ctl_t *kc, kstat_t **ksp_ptr, char *module, int instance, char *name)
{
    char ident[128];

    *ksp_ptr = NULL;

    if (kc == NULL)
        return -1;

    snprintf(ident, sizeof(ident), "%s,%i,%s", module, instance, name);

    *ksp_ptr = kstat_lookup(kc, module, instance, name);
    if (*ksp_ptr == NULL) {
        ERROR("get_kstat: Cound not find kstat %s", ident);
        return -1;
    }

    if ((*ksp_ptr)->ks_type != KSTAT_TYPE_NAMED) {
        ERROR("get_kstat: kstat %s has wrong type", ident);
        *ksp_ptr = NULL;
        return -1;
    }

#ifdef assert
    assert(*ksp_ptr != NULL);
    assert((*ksp_ptr)->ks_type == KSTAT_TYPE_NAMED);
#endif

    if (kstat_read(kc, *ksp_ptr, NULL) == -1) {
        ERROR("get_kstat: kstat %s could not be read", ident);
        return -1;
    }

    if ((*ksp_ptr)->ks_type != KSTAT_TYPE_NAMED) {
        ERROR("get_kstat: kstat %s has wrong type", ident);
        return -1;
    }

    return 0;
}

long long get_kstat_value(kstat_t *ksp, const char *name)
{
    kstat_named_t *kn;
    long long retval = -1LL;

    if (ksp == NULL) {
        ERROR("get_kstat_value (\"%s\"): ksp is NULL.", name);
        return -1LL;
    } else if (ksp->ks_type != KSTAT_TYPE_NAMED) {
        ERROR("get_kstat_value (\"%s\"): ksp->ks_type (%#x) "
              "is not KSTAT_TYPE_NAMED (%#x).",
              name, (unsigned int)ksp->ks_type, (unsigned int)KSTAT_TYPE_NAMED);
        return -1LL;
    }

    if ((kn = (kstat_named_t *)kstat_data_lookup(ksp, discard_const(name))) == NULL)
        return -1LL;

    if (kn->data_type == KSTAT_DATA_INT32)
        retval = (long long)kn->value.i32;
    else if (kn->data_type == KSTAT_DATA_UINT32)
        retval = (long long)kn->value.ui32;
    else if (kn->data_type == KSTAT_DATA_INT64)
        retval = (long long)kn->value.i64; /* According to ANSI C99 `long long' must
                                              hold at least 64 bits */
    else if (kn->data_type == KSTAT_DATA_UINT64)
        retval = (long long)kn->value.ui64; /* XXX: Might overflow! */
    else
        WARNING("get_kstat_value: Not a numeric value: %s", name);

    return retval;
}
