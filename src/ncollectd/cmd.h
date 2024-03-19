/* SPDX-License-Identifier: GPL-2.0-only OR MIT                      */
/* SPDX-FileCopyrightText: Copyright (C) 2018  Florian octo Forster  */
/* SPDX-FileContributor: Florian octo Forster <octo at collectd.org> */

#pragma once

#include <stdbool.h>

struct cmdline_config {
    bool test_config;
    bool test_readall;
    bool create_basedir;
    const char *configfile;
    bool daemonize;
    bool dump_config;
};

void stop_ncollectd(void);

struct cmdline_config init_config(int argc, char **argv);

int run_loop(bool test_readall, void (*notify_func)(void));
