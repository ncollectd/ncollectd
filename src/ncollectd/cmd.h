/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
/* Copyright (C) 2018  Florian octo Forster     */

#ifndef CMD_H
#define CMD_H

#include <stdbool.h>

struct cmdline_config {
  bool test_config;
  bool test_readall;
  bool create_basedir;
  const char *configfile;
  bool daemonize;
};

void stop_collectd(void);
struct cmdline_config init_config(int argc, char **argv);
int run_loop(bool test_readall);

#endif /* CMD_H */
