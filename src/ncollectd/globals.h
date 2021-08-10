/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
/* Copyright (C) 2017  Google LLC               */

#ifndef GLOBALS_H
#define GLOBALS_H

#include <inttypes.h>

#ifndef DATA_MAX_NAME_LEN
#define DATA_MAX_NAME_LEN 128
#endif

#ifndef PRIsz
#define PRIsz "zu"
#endif /* !PRIsz */

/* Type for time as used by "utils_time.h" */
typedef uint64_t cdtime_t;

/* hostname_set updates hostname_g */
void hostname_set(char const *hostname);

extern char *hostname_g;
extern cdtime_t interval_g;
extern int pidfile_from_cli;
extern int timeout_g;
#endif /* GLOBALS_H */
