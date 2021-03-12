/**
 * collectd - src/utils_cmd_putnotif.c
 * Copyright (C) 2008       Florian octo Forster
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Florian octo Forster <octo at collectd.org>
 **/

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

#include "utils/cmds/parse_option.h"
#include "utils/cmds/putnotif.h"

#define print_to_socket(fh, ...)                                               \
  do {                                                                         \
    if (fprintf(fh, __VA_ARGS__) < 0) {                                        \
      WARNING("handle_putnotif: failed to write to socket #%i: %s",            \
              fileno(fh), STRERRNO);                                           \
      return -1;                                                               \
    }                                                                          \
    fflush(fh);                                                                \
  } while (0)

#if 0 // FIXME
static int set_option_time(notification_t *n, const char *value) {
  char *endptr = NULL;
  double tmp;

  errno = 0;
  tmp = strtod(value, &endptr);
  if ((errno != 0)         /* Overflow */
      || (endptr == value) /* Invalid string */
      || (endptr == NULL)  /* This should not happen */
      || (*endptr != 0))   /* Trailing chars */
    return -1;

  n->time = DOUBLE_TO_CDTIME_T(tmp);

  return 0;
} /* int set_option_time */
#endif

int handle_putnotif(FILE *fh, char *buffer) {
  char *command;
  int status;

  if ((fh == NULL) || (buffer == NULL))
    return -1;

  DEBUG("utils_cmd_putnotif: handle_putnotif (fh = %p, buffer = %s);",
        (void *)fh, buffer);

  command = NULL;
  status = parse_string(&buffer, &command);
  if (status != 0) {
    print_to_socket(fh, "-1 Cannot parse command.\n");
    return -1;
  }
  assert(command != NULL);

  if (strcasecmp("PUTNOTIF", command) != 0) {
    print_to_socket(fh, "-1 Unexpected command: `%s'.\n", command);
    return -1;
  }
#if 0 // FIXME
  notification_t n = {0};
  /* Check for required fields and complain if anything is missing. */
  if ((status == 0) && (n.severity == 0)) {
    print_to_socket(fh, "-1 Option `severity' missing.\n");
    status = -1;
  }
  if ((status == 0) && (n.time == 0)) {
    print_to_socket(fh, "-1 Option `time' missing.\n");
    status = -1;
  }
  if ((status == 0) && (strlen(n.message) == 0)) {
    print_to_socket(fh, "-1 No message or message of length 0 given.\n");
    status = -1;
  }

  /* If status is still zero the notification is fine and we can finally
   * dispatch it. */
  if (status == 0) {
    plugin_dispatch_notification(&n);
    print_to_socket(fh, "0 Success\n");
  }
#endif 
  return 0;
} /* int handle_putnotif */
