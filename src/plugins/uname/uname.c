// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <sys/utsname.h>

static metric_family_t fam_uname = {
    .name = "system_uname",
    .type = METRIC_TYPE_INFO,
    .help = "Labeled system information as provided by the uname system call.",
};

static int uname_read(void)
{
    struct utsname name;

    int status = uname(&name);
    if (unlikely(status < 0)) {
        PLUGIN_ERROR("uname failed: %s", STRERRNO);
        return -1;
    }

    metric_t m = {
        .value.info = (label_set_t){
            .num = 5,
            .ptr = (label_pair_t[]){
                {.name = "machine",  .value = name.machine },
                {.name = "nodename", .value = name.nodename},
                {.name = "release",  .value = name.release },
                {.name = "sysname",  .value = name.sysname },
                {.name = "version",  .value = name.version }
    }}};
    metric_family_metric_append(&fam_uname, m);

    plugin_dispatch_metric_family(&fam_uname, 0);

    return 0;
}

void module_register(void)
{
    plugin_register_read("uname", uname_read);
}
