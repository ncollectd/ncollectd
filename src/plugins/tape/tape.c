// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#include "tape.h"

metric_family_t fams[FAM_TAPE_MAX] = {
    [FAM_TAPE_IN_FLIGHT_OPS] = {
        .name = "system_tape_in_flight_ops",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of I/Os currently outstanding to this device.",
    },
    [FAM_TAPE_OTHER_OPS] = {
        .name = "system_tape_other_ops",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of I/Os issued to the tape drive other than read or write commands.",
    },
    [FAM_TAPE_OTHER_TIME] = {
        .name = "system_tape_other_time",
        .type = METRIC_TYPE_COUNTER,
        .help = "The amount of time (in nanoseconds) spent waiting for I/Ps "
                "other than read or write commands.", // FIXME
    },
    [FAM_TAPE_READ_BYTES] = {
        .name = "system_tape_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of bytes read from the tape drive.",
    },
    [FAM_TAPE_READ_OPS] = {
        .name = "system_tape_read_ops",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of read requests issued to the tape drive.",
    },
    [FAM_TAPE_READ_TIME] = {
        .name = "system_tape_read_time",
        .type = METRIC_TYPE_COUNTER,
        .help = "The amount of time (in nanoseconds) spent waiting "
                "for read requests to complete.", // FIXME
    },
    [FAM_TAPE_WRITE_BYTES] = {
        .name = "system_tape_write_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of bytes written to the tape drive.",
    },
    [FAM_TAPE_WRITE_OPS] = {
        .name = "system_tape_write_ops",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of write requests issued to the tape drive.",
    },
    [FAM_TAPE_WRITE_TIME] = {
        .name = "system_tape_write_time",
        .type = METRIC_TYPE_COUNTER,
        .help = "The amount of time (in nanoseconds) spent waiting "
                "for write requests to complete.", // FIXME
    },
    [FAM_TAPE_RESIDUAL] = {
        .name = "system_tape_residual",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times during a read or write we found "
                "the residual amount to be non-zero.",
    },
};

exclist_t excl_tape = {0};

static int tape_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "tape") == 0) {
            status = cf_util_exclist(child, &excl_tape);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

int tape_read(void);

__attribute__(( weak ))
int tape_shutdown(void)
{
    return 0;
}

__attribute__(( weak ))
int tape_init(void)
{
    return 0;
}

void module_register(void)
{
    plugin_register_config("tape", tape_config);
    plugin_register_init("tape", tape_init);
    plugin_register_shutdown("tape", tape_shutdown);
    plugin_register_read("tape", tape_read);
}
