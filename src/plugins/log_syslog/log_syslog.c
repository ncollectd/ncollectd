/**
 * collectd - src/syslog.c
 * Copyright (C) 2007       Florian Forster
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
 *   Florian Forster <octo at collectd.org>
 **/

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

#if HAVE_SYSLOG_H
#include <syslog.h>
#endif

#if COLLECT_DEBUG
static int log_level = LOG_DEBUG;
#else
static int log_level = LOG_INFO;
#endif /* COLLECT_DEBUG */
static int notif_severity;

static const char *config_keys[] = {
    "LogLevel",
    "NotifyLevel",
};
static int config_keys_num = STATIC_ARRAY_SIZE(config_keys);

static int sl_config(const char *key, const char *value)
{
  if (strcasecmp(key, "LogLevel") == 0) {
    log_level = parse_log_severity(value);
    if (log_level < 0) {
      log_level = LOG_INFO;
      WARNING("syslog: invalid loglevel [%s] defaulting to 'info'", value);
    }
  } else if (strcasecmp(key, "NotifyLevel") == 0) {
    notif_severity = parse_notif_severity(value);
    if (notif_severity < 0) {
      ERROR("syslog: invalid notification severity [%s]", value);
      return 1;
    }
  }

  return 0;
}

static void sl_log(int severity, const char *msg,
                   user_data_t __attribute__((unused)) * user_data)
{
  if (severity > log_level)
    return;

  syslog(severity, "%s", msg);
}

static int sl_shutdown(void)
{
  closelog();

  return 0;
}

static int sl_notification(const notification_t *n,
                           user_data_t __attribute__((unused)) * user_data)
{
  if (n->severity > notif_severity)
    return 0;

  int log_severity;
  switch (n->severity) {
    case NOTIF_FAILURE:
      log_severity = LOG_ERR;
      break;
    case NOTIF_WARNING:
      log_severity = LOG_WARNING;
      break;
    case NOTIF_OKAY:
      log_severity = LOG_NOTICE;
      break;
    default:
      log_severity = LOG_ERR;
  }

  strbuf_t buf = STRBUF_CREATE;
  strbuf_print(&buf, "Notification: ");
  notification_marshal(&buf, n);

  sl_log(log_severity, buf.ptr, NULL);

  STRBUF_DESTROY(buf);
  return 0;
}

void module_register(void)
{
  openlog("collectd", LOG_CONS | LOG_PID, LOG_DAEMON);

  plugin_register_config("log_syslog", sl_config, config_keys, config_keys_num);
  plugin_register_log("log_syslog", sl_log, /* user_data = */ NULL);
  plugin_register_notification("log_syslog", sl_notification, NULL);
  plugin_register_shutdown("log_syslog", sl_shutdown);
}
