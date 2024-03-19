// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2005,2006  Scott Garrett
// SPDX-FileContributor: Scott Garrett <sgarrett at technomancer.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#include <kstat.h>

#if defined(HAVE_KSTAT_IO_T_WRITES) && defined(HAVE_KSTAT_IO_T_NWRITES) && defined(HAVE_KSTAT_IO_T_WTIME)
#    define KIO_ROCTETS reads
#    define KIO_WOCTETS writes
#    define KIO_ROPS nreads
#    define KIO_WOPS nwrites
#    define KIO_RTIME rtime
#    define KIO_WTIME wtime
#elif defined(HAVE_KSTAT_IO_T_NWRITTEN) && defined(HAVE_KSTAT_IO_T_WRITES) && defined(HAVE_KSTAT_IO_T_WTIME)
#    define KIO_ROCTETS nread
#    define KIO_WOCTETS nwritten
#    define KIO_ROPS reads
#    define KIO_WOPS writes
#    define KIO_RTIME rtime
#    define KIO_WTIME wtime
#else
#    error "kstat_io_t does not have the required members"
#endif

#include "tape.h"

extern exclist_t excl_tape;
extern metric_family_t *fams;

#define MAX_NUMTAPE 256

static kstat_ctl_t *kc;
static kstat_t *ksp[MAX_NUMTAPE];
static int numtape;

int tape_read(void)
{
    static kstat_io_t kio;

    if (kc == NULL)
        return -1;

    if (kstat_chain_update(kc) < 0) {
        PLUGIN_ERROR("kstat_chain_update failed.");
        return -1;
    }

    if (numtape <= 0)
        return -1;

    for (int i = 0; i < numtape; i++) {
        if (kstat_read(kc, ksp[i], &kio) == -1)
            continue;

        if (strncmp(ksp[i]->ks_class, "tape", 4) == 0) {
            char *tape_name = ksp[i]->ks_name;

            if (!exclist_match(&excl_tape, tape_name))
                continue;

            metric_family_append(&fams[FAM_TAPE_READ_BYTES], VALUE_COUNTER(kio.KIO_ROCTETS), NULL,
                                 &(label_pair_const_t){.name="device", .value=tape_name}, NULL);
            metric_family_append(&fams[FAM_TAPE_READ_OPS], VALUE_COUNTER(kio.KIO_ROPS), NULL,
                                 &(label_pair_const_t){.name="device", .value=tape_name}, NULL);
            metric_family_append(&fams[FAM_TAPE_READ_TIME], VALUE_COUNTER(kio.KIO_RTIME), NULL,
                                 &(label_pair_const_t){.name="device", .value=tape_name}, NULL);
            metric_family_append(&fams[FAM_TAPE_WRITE_BYTES], VALUE_COUNTER(kio.KIO_WOCTETS), NULL,
                                 &(label_pair_const_t){.name="device", .value=tape_name}, NULL);
            metric_family_append(&fams[FAM_TAPE_WRITE_OPS], VALUE_COUNTER(kio.KIO_WOPS), NULL,
                                 &(label_pair_const_t){.name="device", .value=tape_name}, NULL);
            metric_family_append(&fams[FAM_TAPE_WRITE_TIME], VALUE_COUNTER(kio.KIO_WTIME), NULL,
                                 &(label_pair_const_t){.name="device", .value=tape_name}, NULL);
      }
    }

    plugin_dispatch_metric_family_array(fams, FAM_TAPE_MAX, 0);

    return 0;
}

int tape_init(void)
{
    kstat_t *ksp_chain;

    numtape = 0;

    if (kc == NULL)
        kc = kstat_open();

    if (kc == NULL) {
        PLUGIN_ERROR("kstat_open failed.");
        return -1;
    }

    for (numtape = 0, ksp_chain = kc->kc_chain; (numtape < MAX_NUMTAPE) && (ksp_chain != NULL);
         ksp_chain = ksp_chain->ks_next) {
        if (strncmp(ksp_chain->ks_class, "tape", 4))
            continue;
        if (ksp_chain->ks_type != KSTAT_TYPE_IO)
            continue;
        ksp[numtape++] = ksp_chain;
    }

    return 0;
}

int tape_shutdown(void)
{
    exclist_reset(&excl_tape);

    return 0;
}
