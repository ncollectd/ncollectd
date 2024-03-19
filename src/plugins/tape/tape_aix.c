// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

# include <sys/protosw.h>
# include <libperfstat.h>
/* XINTFRAC was defined in libperfstat.h somewhere between AIX 5.3 and 6.1 */
# ifndef XINTFRAC
#  include <sys/systemcfg.h>
#  define XINTFRAC ((double)(_system_configuration.Xint) / \
                   (double)(_system_configuration.Xfrac))
# endif
# ifndef HTIC2NANOSEC
#  define HTIC2NANOSEC(x)  (((double)(x) * XINTFRAC))
# endif

#include "tape.h"

extern exclist_t excl_tape;
extern metric_family_t *fams;

static perfstat_tape_t *stat_tape;
static int pnumtape;

typedef struct tape_stats {
    char *name;
    unsigned int poll_count;
    uint64_t read_ops;
    uint64_t write_ops;
    uint64_t read_time;
    uint64_t write_time;
    uint64_t avg_read_time;
    uint64_t avg_write_time;
    struct tape_stats *next;
} tape_stats_t;

static tape_stats_t *tape_list;

static uint64_t tape_calc_time_incr (uint64_t delta_time, uint64_t delta_ops)
{
    double interval = CDTIME_T_TO_DOUBLE (plugin_get_interval ());
    double avg_time = ((double) delta_time) / ((double) delta_ops);
    double avg_time_incr = interval * avg_time;

    return (uint64_t) (avg_time_incr + .5);
}

int tape_read(void)
{
    /* check how many perfstat_tape_t structures are available */
    int numtape =  perfstat_tape(NULL, NULL, sizeof(perfstat_tape_t), 0);
    if (numtape < 0) {
        char errbuf[1024];
        PLUGIN_WARNING ("perfstat_tape: %s", sstrerror (errno, errbuf, sizeof (errbuf)));
        return -1;
    }

    if (numtape == 0)
        return 0;

    if ((numtape != pnumtape) || (stat_tape == NULL)) {
        free(stat_tape);
        stat_tape = (perfstat_tape_t *)calloc(numtape, sizeof(perfstat_tape_t));
        if (stat_tape == NULL) {
            PLUGIN_ERROR ("malloc failed.");
            return -1;
        }
    }
    pnumtape = numtape;

    perfstat_id_t first = {0};
    first.name[0]='\0';
    int rnumtape = perfstat_tape(&first, stat_tape, sizeof(perfstat_tape_t), numtape);
    if (rnumtape < 0) {
        PLUGIN_WARNING ("tape plugin: perfstat_tape: %s", STRERRNO);
        return -1;
    }

    for (int i=0; i < rnumtape ; i++) {
        char *tape_name = stat_tape[i].name;

        if (!exclist_match(&excl_tape, tape_name))
            continue;

        tape_stats_t *ts, *pre_ts;
        for (ts = tape_list, pre_ts = tape_list; ts != NULL; pre_ts = ts, ts = ts->next) {
            if (strcmp (tape_name, ts->name) == 0)
                break;
        }

        if (ts == NULL) {
           ts = (tape_stats_t *) calloc(1, sizeof (tape_stats_t));
           if (ts == NULL)
               continue;

           ts->name = strdup(tape_name);
           if (ts->name == NULL) {
               free (ts);
               continue;
           }

           if (pre_ts == NULL)
               tape_list = ts;
           else
               pre_ts->next = ts;
        }

        uint64_t read_bytes = stat_tape[i].rblks * stat_tape[i].bsize;
        uint64_t write_bytes = stat_tape[i].wblks * stat_tape[i].bsize;

        uint64_t read_ops = stat_tape[i].rxfers;
        uint64_t write_ops = stat_tape[i].xfers - stat_tape[i].rxfers;

        uint64_t read_time = HTIC2NANOSEC(stat_tape[i].rserv) / 1000000.0;
        uint64_t write_time = HTIC2NANOSEC(stat_tape[i].wserv) / 1000000.0;

        uint64_t diff_read_ops = read_ops - ts->read_ops;
        uint64_t diff_write_ops = write_ops - ts->write_ops;

        uint64_t diff_read_time = read_time - ts->read_time;
        uint64_t diff_write_time = write_time - ts->write_time;

        if (diff_read_ops != 0)
            ts->avg_read_time += tape_calc_time_incr (diff_read_time, diff_read_ops);
        if (diff_write_ops != 0)
            ts->avg_write_time += tape_calc_time_incr (diff_write_time, diff_write_ops);

        ts->read_ops = read_ops;
        ts->read_time = read_time;

        ts->write_ops = write_ops;
        ts->write_time = write_time;

        ts->poll_count++;
        if (ts->poll_count <= 2)
            continue;

        if ((read_ops == 0) && (write_ops == 0)) {
            PLUGIN_DEBUG ("((read_ops == 0) && " "(write_ops == 0)); => Not writing.");
            continue;
        }

        metric_family_append(&fams[FAM_TAPE_READ_BYTES], VALUE_COUNTER(read_bytes), NULL,
                             &(label_pair_const_t){.name="device", .value=tape_name}, NULL);
        metric_family_append(&fams[FAM_TAPE_READ_OPS], VALUE_COUNTER(read_ops), NULL,
                             &(label_pair_const_t){.name="device", .value=tape_name}, NULL);
        metric_family_append(&fams[FAM_TAPE_READ_TIME], VALUE_COUNTER(ts->avg_read_time), NULL,
                             &(label_pair_const_t){.name="device", .value=tape_name}, NULL);
        metric_family_append(&fams[FAM_TAPE_WRITE_BYTES], VALUE_COUNTER(write_bytes), NULL,
                             &(label_pair_const_t){.name="device", .value=tape_name}, NULL);
        metric_family_append(&fams[FAM_TAPE_WRITE_OPS], VALUE_COUNTER(write_ops), NULL,
                             &(label_pair_const_t){.name="device", .value=tape_name}, NULL);
        metric_family_append(&fams[FAM_TAPE_WRITE_TIME], VALUE_COUNTER(ts->avg_write_time), NULL,
                             &(label_pair_const_t){.name="device", .value=tape_name}, NULL);
    }

    plugin_dispatch_metric_family_array(fams, FAM_TAPE_MAX, 0);

    return 0;
}

int tape_shutdown(void)
{
    exclist_reset(&excl_tape);

    free(stat_tape);

    while(tape_list != NULL) {
        tape_stats_t *next = tape_list->next;
        free(tape_list->name);
        free(tape_list);
        tape_list = next;
    }

    return 0;
}
