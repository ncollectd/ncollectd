// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#include "tape.h"

extern metric_family_t tape_fams[FAM_TAPE_MAX];
extern exclist_t excl_tape;
extern plugin_filter_t *tape_filter;

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
static char *path_sys_tape;

static bool is_tape(const char *filename)
{
    if (filename == NULL)
        return false;
    if (strncmp(filename, "st", 2) != 0)
        return false;

    const char *digits = filename + 2;
    while(*digits != '\0') {
        if(!isdigit((unsigned char)*digits))
            return false;
        digits++;
    }

    return true;
}

static int64_t tape_calc_time_incr(int64_t delta_time, int64_t delta_ops)
{
    double interval = CDTIME_T_TO_DOUBLE(plugin_get_interval());
    double avg_time = ((double)delta_time) / ((double)delta_ops);
    double avg_time_incr = interval * avg_time;

    return (int64_t)(avg_time_incr + .5);
}

static int tape_read_device(int dir_fd,  __attribute__((unused)) const char *dirname,
                            const char *tape,  __attribute__((unused)) void *user_data)
{
    if(!is_tape(tape))
        return 0;

    if (!exclist_match(&excl_tape, tape))
        return 0;

    int tape_fd = openat(dir_fd, tape, O_RDONLY | O_DIRECTORY);
    if (tape_fd < 0)
        return 0;

    tape_stats_t *ts, *pre_ts;
    for (ts = tape_list, pre_ts = tape_list; ts != NULL; pre_ts = ts, ts = ts->next) {
        if (strcmp (tape, ts->name) == 0)
            break;
    }

    if (ts == NULL) {
        ts = (tape_stats_t *)calloc(1, sizeof (tape_stats_t));
        if (ts == NULL) {
            close(tape_fd);
            return 0;
        }

        if ((ts->name = strdup (tape)) == NULL) {
            close(tape_fd);
            free(ts);
            return 0;
        }

        if (pre_ts == NULL)
            tape_list = ts;
        else
            pre_ts->next = ts;
    }

    uint64_t in_flight = 0;
    if (filetouint_at(tape_fd, "stats/in_flight", &in_flight) == 0)
        metric_family_append(&tape_fams[FAM_TAPE_IN_FLIGHT_REQUESTS], VALUE_GAUGE(in_flight), NULL,
                             &LABEL_PAIR_CONST("device", tape), NULL);

    uint64_t other_cnt = 0;
    if (filetouint_at(tape_fd, "stats/other_cnt", &other_cnt) == 0)
        metric_family_append(&tape_fams[FAM_TAPE_OTHER_OPS], VALUE_COUNTER(other_cnt), NULL,
                             &LABEL_PAIR_CONST("device", tape), NULL);

    uint64_t read_byte_cnt = 0;
    if (filetouint_at(tape_fd, "stats/read_byte_cnt", &read_byte_cnt) == 0)
        metric_family_append(&tape_fams[FAM_TAPE_READ_BYTES], VALUE_COUNTER(read_byte_cnt), NULL,
                             &LABEL_PAIR_CONST("device", tape), NULL);

    uint64_t read_cnt = 0;
    bool read_cnt_found = false;
    if (filetouint_at(tape_fd, "stats/read_cnt", &read_cnt) == 0) {
        metric_family_append(&tape_fams[FAM_TAPE_READ_OPS], VALUE_COUNTER(read_cnt), NULL,
                             &LABEL_PAIR_CONST("device", tape), NULL);
        read_cnt_found = true;
    }

    uint64_t write_byte_cnt = 0;
    if (filetouint_at(tape_fd, "stats/write_byte_cnt", &write_byte_cnt) == 0)
        metric_family_append(&tape_fams[FAM_TAPE_WRITE_BYTES], VALUE_COUNTER(write_byte_cnt), NULL,
                             &LABEL_PAIR_CONST("device", tape), NULL);
    uint64_t write_cnt = 0;
    bool write_cnt_found = false;
    if (filetouint_at(tape_fd, "stats/write_cnt", &write_cnt) == 0) {
        metric_family_append(&tape_fams[FAM_TAPE_WRITE_OPS], VALUE_COUNTER(write_cnt), NULL,
                             &LABEL_PAIR_CONST("device", tape), NULL);
        write_cnt_found = true;
    }

    uint64_t resid_cnt = 0;
    if (filetouint_at(tape_fd, "stats/resid_cnt", &resid_cnt) == 0)
        metric_family_append(&tape_fams[FAM_TAPE_RESIDUAL], VALUE_COUNTER(resid_cnt), NULL,
                             &LABEL_PAIR_CONST("device", tape), NULL);

    if (!read_cnt_found || !write_cnt_found) {
        close(tape_fd);
        return 0;
    }

    uint64_t diff_read_ops = read_cnt - ts->read_ops;
    uint64_t diff_write_ops = write_cnt - ts->write_ops;

    uint64_t read_ns = 0;
    bool read_ns_found = false;
    if (filetouint_at(tape_fd, "stats/read_ns", &read_ns) == 0)
        read_ns_found = true;

    uint64_t write_ns = 0;
    bool write_ns_found = false;
    if (filetouint_at(tape_fd, "stats/write_ns", &write_ns) == 0)
        write_ns_found = true;

    if (!read_ns_found || !write_ns_found) {
        close(tape_fd);
        return 0;
    }

    metric_family_append(&tape_fams[FAM_TAPE_READ_TIME],
                         VALUE_COUNTER_FLOAT64((double)read_ns/(double)1e9), NULL,
                         &LABEL_PAIR_CONST("device", tape), NULL);
    metric_family_append(&tape_fams[FAM_TAPE_WRITE_TIME],
                         VALUE_COUNTER_FLOAT64((double)write_ns/(double)1e9), NULL,
                         &LABEL_PAIR_CONST("device", tape), NULL);

    uint64_t diff_read_time = read_ns - ts->read_time;
    uint64_t diff_write_time = write_ns - ts->write_time;

    ts->read_ops = read_cnt;
    ts->write_ops = write_cnt;
    ts->read_time = read_ns;
    ts->write_time = write_ns;

    if (diff_read_ops != 0)
        ts->avg_read_time += tape_calc_time_incr(diff_read_time, diff_read_ops);
    if (diff_write_ops != 0)
        ts->avg_write_time += tape_calc_time_incr(diff_write_time, diff_write_ops);

    uint64_t io_ns = 0;
    if (filetouint_at(tape_fd, "stats/io_ns", &io_ns) == 0)
        metric_family_append(&tape_fams[FAM_TAPE_IO_TIME],
                             VALUE_COUNTER_FLOAT64((double)io_ns/(double)1e9), NULL,
                             &LABEL_PAIR_CONST("device", tape), NULL);

    ts->poll_count++;
    if (ts->poll_count <= 2) {
        close(tape_fd);
        return 0;
    }

    metric_family_append(&tape_fams[FAM_TAPE_READ_WEIGHTED_TIME],
                         VALUE_COUNTER_FLOAT64((double)ts->avg_read_time/(double)1e9), NULL,
                         &LABEL_PAIR_CONST("device", tape), NULL);
    metric_family_append(&tape_fams[FAM_TAPE_WRITE_WEIGHTED_TIME],
                         VALUE_COUNTER_FLOAT64((double)ts->avg_write_time/(double)1e9), NULL,
                         &LABEL_PAIR_CONST("device", tape), NULL);

    close(tape_fd);

    return 0;
}

int tape_read(void)
{
    walk_directory(path_sys_tape, tape_read_device, NULL, 0);

    plugin_dispatch_metric_family_array_filtered(tape_fams, FAM_TAPE_MAX, tape_filter, 0);

    return 0;
}

int tape_init(void)
{
    path_sys_tape = plugin_syspath("class/scsi_tape");
    if (path_sys_tape == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    return 0;
}

int tape_shutdown(void)
{
    exclist_reset(&excl_tape);
    plugin_filter_free(tape_filter);
    free(path_sys_tape);

    while(tape_list != NULL) {
        tape_stats_t *next = tape_list->next;
        free(tape_list->name);
        free(tape_list);
        tape_list = next;
    }

    return 0;
}
