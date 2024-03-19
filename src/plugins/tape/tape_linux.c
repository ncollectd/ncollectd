// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#include "tape.h"

extern metric_family_t fams[FAM_TAPE_MAX];
extern exclist_t excl_tape;

typedef struct tape_stats {
    char *name;
    unsigned int poll_count;
    uint64_t read_ops;
    uint64_t write_ops;
    uint64_t other_ops;
    uint64_t read_time;
    uint64_t write_time;
    uint64_t other_time;
    uint64_t avg_read_time;
    uint64_t avg_write_time;
    uint64_t avg_other_time;
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
        if(!isdigit(*digits))
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
        if (ts == NULL)
            return 0;

        if ((ts->name = strdup (tape)) == NULL) {
            free(ts);
            return 0;
        }

        if (pre_ts == NULL)
            tape_list = ts;
        else
            pre_ts->next = ts;
    }

    uint64_t in_flight = 0;
    ssize_t in_flight_size = filetouint_at(tape_fd, "stats/in_flight", &in_flight);
    if (in_flight_size > 0)
        metric_family_append(&fams[FAM_TAPE_IN_FLIGHT_OPS], VALUE_COUNTER(in_flight), NULL,
                             &(label_pair_const_t){.name="device", .value=tape}, NULL);

    uint64_t other_cnt = 0;
    ssize_t other_cnt_size = filetouint_at(tape_fd, "stats/other_cnt", &other_cnt);
    if (other_cnt_size > 0)
        metric_family_append(&fams[FAM_TAPE_OTHER_OPS], VALUE_COUNTER(other_cnt), NULL,
                             &(label_pair_const_t){.name="device", .value=tape}, NULL);

    uint64_t read_byte_cnt = 0;
    ssize_t read_byte_cnt_size = filetouint_at(tape_fd, "stats/read_byte_cnt", &read_byte_cnt);
    if(read_byte_cnt_size > 0)
        metric_family_append(&fams[FAM_TAPE_READ_BYTES], VALUE_COUNTER(read_byte_cnt), NULL,
                             &(label_pair_const_t){.name="device", .value=tape}, NULL);

    uint64_t read_cnt = 0;
    ssize_t read_cnt_size = filetouint_at(tape_fd, "stats/read_cnt", &read_cnt);
    if(read_cnt_size > 0)
        metric_family_append(&fams[FAM_TAPE_READ_OPS], VALUE_COUNTER(read_cnt), NULL,
                             &(label_pair_const_t){.name="device", .value=tape}, NULL);

    uint64_t write_byte_cnt = 0;
    ssize_t write_byte_cnt_size = filetouint_at(tape_fd, "stats/write_byte_cnt", &write_byte_cnt);
    if(write_byte_cnt_size > 0)
        metric_family_append(&fams[FAM_TAPE_WRITE_BYTES], VALUE_COUNTER(write_byte_cnt), NULL,
                             &(label_pair_const_t){.name="device", .value=tape}, NULL);
    uint64_t write_cnt = 0;
    ssize_t write_cnt_size = filetouint_at(tape_fd, "stats/write_cnt", &write_cnt);
    if(write_cnt_size > 0)
        metric_family_append(&fams[FAM_TAPE_WRITE_OPS], VALUE_COUNTER(write_cnt), NULL,
                             &(label_pair_const_t){.name="device", .value=tape}, NULL);

    uint64_t resid_cnt = 0;
    ssize_t resid_cnt_size = filetouint_at(tape_fd, "stats/resid_cnt", &resid_cnt);
    if (resid_cnt_size > 0)
        metric_family_append(&fams[FAM_TAPE_RESIDUAL], VALUE_COUNTER(resid_cnt), NULL,
                             &(label_pair_const_t){.name="device", .value=tape}, NULL);

    if ((read_cnt_size <= 0) || (write_cnt_size <= 0))
        return 0;

    uint64_t diff_read_ops = read_cnt - ts->read_ops;
    uint64_t diff_write_ops = write_cnt - ts->write_ops;


    uint64_t read_ns = 0;
    ssize_t read_ns_size = filetouint_at(tape_fd, "stats/read_ns", &read_ns);

    uint64_t write_ns = 0;
    ssize_t write_ns_size = filetouint_at(tape_fd, "stats/write_ns", &write_ns);

    if ((read_ns_size <= 0) || (write_ns_size <= 0))
        return 0;

    uint64_t diff_read_time = read_ns - ts->read_time;
    uint64_t diff_write_time = write_ns - ts->write_time;

    ts->poll_count++;

    uint64_t io_ns = 0;
    ssize_t io_ns_size = filetouint_at(tape_fd, "stats/io_ns", &io_ns);

    uint64_t diff_other_ops = 0;
    if (other_cnt_size > 0)  {
        diff_other_ops = other_cnt - ts->other_ops;
        ts->other_ops = other_cnt;
    }
    uint64_t diff_other_time = 0;
    if (io_ns_size) {
        diff_other_time = io_ns - ts->other_time; // FIXME
        ts->other_time = io_ns;
    }

    if (diff_read_ops != 0)
        ts->avg_read_time += tape_calc_time_incr(diff_read_time, diff_read_ops);
    if (diff_write_ops != 0)
        ts->avg_write_time += tape_calc_time_incr(diff_write_time, diff_write_ops);
    if (diff_other_ops != 0)
        ts->avg_other_time += tape_calc_time_incr(diff_other_time, diff_other_ops);

    ts->read_ops = read_cnt;
    ts->write_ops = write_cnt;
    ts->other_ops = other_cnt;
    ts->read_time = read_ns;
    ts->write_time = write_ns;
    ts->other_time = io_ns;

    if (ts->poll_count <= 2)
        return 0;

    metric_family_append(&fams[FAM_TAPE_READ_TIME], VALUE_COUNTER(ts->avg_read_time), NULL,
                         &(label_pair_const_t){.name="device", .value=tape}, NULL);
    metric_family_append(&fams[FAM_TAPE_WRITE_TIME], VALUE_COUNTER(ts->avg_write_time), NULL,
                         &(label_pair_const_t){.name="device", .value=tape}, NULL);
    metric_family_append(&fams[FAM_TAPE_OTHER_TIME], VALUE_COUNTER(ts->avg_other_time), NULL,
                         &(label_pair_const_t){.name="device", .value=tape}, NULL);

    close(tape_fd);

    return 0;
}

int tape_read(void)
{
    walk_directory(path_sys_tape, tape_read_device, NULL, 0);

    plugin_dispatch_metric_family_array(fams, FAM_TAPE_MAX, 0);

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
    free(path_sys_tape);

    while(tape_list != NULL) {
        tape_stats_t *next = tape_list->next;
        free(tape_list->name);
        free(tape_list);
        tape_list = next;
    }

    return 0;
}
