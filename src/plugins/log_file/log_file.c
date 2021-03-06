// SPDX-License-Identifier: GPL-2.0-only OR MIT
// Copyright (C) 2007       Sebastian Harl
// Copyright (C) 2007,2008  Florian Forster
// Authors:
//   Sebastian Harl <sh at tokkee.org>
//   Florian Forster <octo at collectd.org>

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

#if COLLECT_DEBUG
static int log_level = LOG_DEBUG;
#else
static int log_level = LOG_INFO;
#endif /* COLLECT_DEBUG */

static pthread_mutex_t file_lock = PTHREAD_MUTEX_INITIALIZER;

static char *log_file;
static int print_timestamp = 1;
static int print_severity;

static const char *config_keys[] = {"LogLevel",
                                    "File",
                                    "Timestamp",
                                    "PrintSeverity"};
static int config_keys_num = STATIC_ARRAY_SIZE(config_keys);

static int logfile_config(const char *key, const char *value)
{
  if (0 == strcasecmp(key, "LogLevel")) {
    log_level = parse_log_severity(value);
    if (log_level < 0) {
      log_level = LOG_INFO;
      WARNING("logfile: invalid loglevel [%s] defaulting to 'info'", value);
      return 0;
    }
  } else if (0 == strcasecmp(key, "File")) {
    sfree(log_file);
    log_file = strdup(value);
  } else if (0 == strcasecmp(key, "Timestamp")) {
    if (IS_FALSE(value))
      print_timestamp = 0;
    else
      print_timestamp = 1;
  } else if (0 == strcasecmp(key, "PrintSeverity")) {
    if (IS_FALSE(value))
      print_severity = 0;
    else
      print_severity = 1;
  } else {
    return -1;
  }
  return 0;
}

static void logfile_print(const char *msg, int severity,
                          cdtime_t timestamp_time)
{
  FILE *fh;
  bool do_close = false;
  char timestamp_str[64];
  char level_str[16] = "";

  if (print_severity) {
    switch (severity) {
    case LOG_ERR:
      snprintf(level_str, sizeof(level_str), "[error] ");
      break;
    case LOG_WARNING:
      snprintf(level_str, sizeof(level_str), "[warning] ");
      break;
    case LOG_NOTICE:
      snprintf(level_str, sizeof(level_str), "[notice] ");
      break;
    case LOG_INFO:
      snprintf(level_str, sizeof(level_str), "[info] ");
      break;
    case LOG_DEBUG:
      snprintf(level_str, sizeof(level_str), "[debug] ");
      break;
    default:
      break;
    }
  }

  if (print_timestamp) {
    struct tm timestamp_tm;
    localtime_r(&CDTIME_T_TO_TIME_T(timestamp_time), &timestamp_tm);

    strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%d %H:%M:%S", &timestamp_tm);
    timestamp_str[sizeof(timestamp_str) - 1] = '\0';
  }

  pthread_mutex_lock(&file_lock);

  if (log_file == NULL) {
    fh = stderr;
  } else if (strcasecmp(log_file, "stderr") == 0)
    fh = stderr;
  else if (strcasecmp(log_file, "stdout") == 0)
    fh = stdout;
  else {
    fh = fopen(log_file, "a");
    do_close = true;
  }

  if (fh == NULL) {
    fprintf(stderr, "logfile plugin: fopen (%s) failed: %s\n", log_file, STRERRNO);
  } else {
    if (print_timestamp)
      fprintf(fh, "[%s] %s%s\n", timestamp_str, level_str, msg);
    else
      fprintf(fh, "%s%s\n", level_str, msg);

    if (do_close) {
      fclose(fh);
    } else {
      fflush(fh);
    }
  }

  pthread_mutex_unlock(&file_lock);

  return;
}

static void logfile_log(int severity, const char *msg,
                        user_data_t __attribute__((unused)) * user_data)
{
  if (severity > log_level)
    return;

  logfile_print(msg, severity, cdtime());
}

static int logfile_notification(const notification_t *n,
                                user_data_t __attribute__((unused)) * user_data)
{
  strbuf_t buf = STRBUF_CREATE;

  strbuf_print(&buf, "Notification: ");

  notification_marshal(&buf, n);

  logfile_print(buf.ptr, LOG_INFO, (n->time != 0) ? n->time : cdtime());

  strbuf_destroy(&buf);
  return 0;
}

void module_register(void)
{
  plugin_register_config("log_file", logfile_config, config_keys, config_keys_num);
  plugin_register_log("log_file", logfile_log, /* user_data = */ NULL);
  plugin_register_notification("log_file", logfile_notification, /* user_data = */ NULL);
}
