/* SPDX-License-Identifier: GPL-2.0-only OR MIT                         */
/* SPDX-FileCopyrightText: Copyright (C) 2006-2013 Florian octo Forster */
/* SPDX-FileCopyrightText: Copyright (C) 2008 Sebastian tokkee Harl     */
/* SPDX-FileContributor: Florian octo Forster <octo at collectd.org>    */
/* SPDX-FileContributor: Sebastian tokkee Harl <sh at tokkee.org>       */

#pragma once

#include "libutils/time.h"

typedef struct {
  /* time of the last report */
  cdtime_t last;

  /* How long to wait until reporting again.
   * 0 indicates that the complaint is no longer valid. */
  cdtime_t interval;

  bool complained_once;
} c_complain_t;

#define C_COMPLAIN_INIT_STATIC                                                 \
  { 0, 0, 0 }
#define C_COMPLAIN_INIT(c)                                                     \
  do {                                                                         \
    (c)->last = 0;                                                             \
    (c)->interval = 0;                                                         \
    (c)->complained_once = false;                                              \
  } while (0)

/*
 * NAME
 *   c_complain
 *
 * DESCRIPTION
 *   Complain about something. This function will report a message (usually
 *   indicating some error condition) using the ncollectd logging mechanism.
 *   When this function is called again, reporting the message again will be
 *   deferred by an increasing interval (up to one day) to prevent flooding
 *   the logs. A call to `c_release' resets the counter.
 *
 * PARAMETERS
 *   `level'  The log level passed to `plugin_log'.
 *   `c'      Identifier for the complaint.
 *   `format' Message format - see the documentation of printf(3).
 */
__attribute__((format(printf, 3, 4))) void
c_complain(int level, c_complain_t *c, const char *format, ...);

/*
 * NAME
 *   c_complain_once
 *
 * DESCRIPTION
 *   Complain about something once. This function will not report anything
 *   again, unless `c_release' has been called in between. If used after some
 *   calls to `c_complain', it will report again on the next interval and stop
 *   after that.
 *
 *   See `c_complain' for further details and a description of the parameters.
 */
__attribute__((format(printf, 3, 4))) void
c_complain_once(int level, c_complain_t *c, const char *format, ...);

/*
 * NAME
 *   c_would_release
 *
 * DESCRIPTION
 *   Returns true if the specified complaint would be released, false else.
 */
#define c_would_release(c) ((c)->interval != 0)

/*
 * NAME
 *   c_release
 *
 * DESCRIPTION
 *   Release a complaint. This will report a message once, marking the
 *   complaint as released.
 *
 *   See `c_complain' for a description of the parameters.
 */
__attribute__((format(printf, 3, 4))) void
c_do_release(int level, c_complain_t *c, const char *format, ...);
#define c_release(level, c, ...)                                               \
  do {                                                                         \
    if (c_would_release(c))                                                    \
      c_do_release(level, c, __VA_ARGS__);                                     \
  } while (0)
