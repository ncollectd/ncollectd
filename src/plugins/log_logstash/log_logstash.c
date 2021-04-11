/**
 * collectd - src/log_logstash.c
 * Copyright (C) 2013       Pierre-Yves Ritschard
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
 *   Pierre-Yves Ritschard <pyr at spootnik.org>
 * Acknowledgements:
 *   This file is largely inspired by logfile.c
 **/

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

#include <sys/types.h>
#include <yajl/yajl_common.h>
#include <yajl/yajl_gen.h>
#if HAVE_YAJL_YAJL_VERSION_H
#include <yajl/yajl_version.h>
#endif
#if defined(YAJL_MAJOR) && (YAJL_MAJOR > 1)
#define HAVE_YAJL_V2 1
#endif

#if COLLECT_DEBUG
static int log_level = LOG_DEBUG;
#else
static int log_level = LOG_INFO;
#endif /* COLLECT_DEBUG */

static pthread_mutex_t file_lock = PTHREAD_MUTEX_INITIALIZER;

static char *log_file;

static const char *config_keys[] = {"LogLevel", "File"};
static int config_keys_num = STATIC_ARRAY_SIZE(config_keys);

static int log_logstash_config(const char *key, const char *value)
{
  if (0 == strcasecmp(key, "LogLevel")) {
    log_level = parse_log_severity(value);
    if (log_level < 0) {
      log_level = LOG_INFO;
      ERROR("log_logstash: invalid loglevel [%s] defaulting to 'info'", value);
      return 1;
    }
  } else if (0 == strcasecmp(key, "File")) {
    sfree(log_file);
    log_file = strdup(value);
  } else {
    return -1;
  }
  return 0;
}

static void log_logstash_print(yajl_gen g, int severity,
                               cdtime_t timestamp_time)
{ 
  FILE *fh;
  bool do_close = false;
  struct tm timestamp_tm;
  char timestamp_str[64];
  const unsigned char *buf;
#if HAVE_YAJL_V2
  size_t len;
#else
  unsigned int len;
#endif

  if (yajl_gen_string(g, (u_char *)"level", strlen("level")) !=
      yajl_gen_status_ok)
    goto err;

  switch (severity) {
  case LOG_ERR:
    if (yajl_gen_string(g, (u_char *)"error", strlen("error")) !=
        yajl_gen_status_ok)
      goto err;
    break;
  case LOG_WARNING:
    if (yajl_gen_string(g, (u_char *)"warning", strlen("warning")) !=
        yajl_gen_status_ok)
      goto err;
    break;
  case LOG_NOTICE:
    if (yajl_gen_string(g, (u_char *)"notice", strlen("notice")) !=
        yajl_gen_status_ok)
      goto err;
    break;
  case LOG_INFO:
    if (yajl_gen_string(g, (u_char *)"info", strlen("info")) !=
        yajl_gen_status_ok)
      goto err;
    break;
  case LOG_DEBUG:
    if (yajl_gen_string(g, (u_char *)"debug", strlen("debug")) !=
        yajl_gen_status_ok)
      goto err;
    break;
  default:
    if (yajl_gen_string(g, (u_char *)"unknown", strlen("unknown")) !=
        yajl_gen_status_ok)
      goto err;
    break;
  }

  if (yajl_gen_string(g, (u_char *)"@timestamp", strlen("@timestamp")) !=
      yajl_gen_status_ok)
    goto err;

  gmtime_r(&CDTIME_T_TO_TIME_T(timestamp_time), &timestamp_tm);

  /*
   * format time as a UTC ISO 8601 compliant string
   */
  strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ",
           &timestamp_tm);
  timestamp_str[sizeof(timestamp_str) - 1] = '\0';

  if (yajl_gen_string(g, (u_char *)timestamp_str, strlen(timestamp_str)) !=
      yajl_gen_status_ok)
    goto err;

  if (yajl_gen_map_close(g) != yajl_gen_status_ok)
    goto err;

  if (yajl_gen_get_buf(g, &buf, &len) != yajl_gen_status_ok)
    goto err;
  pthread_mutex_lock(&file_lock);

  if (log_file == NULL) {
    fh = stderr;
  } else if (strcasecmp(log_file, "stdout") == 0) {
    fh = stdout;
    do_close = false;
  } else if (strcasecmp(log_file, "stderr") == 0) {
    fh = stderr;
    do_close = false;
  } else {
    fh = fopen(log_file, "a");
    do_close = true;
  }

  if (fh == NULL) {
    fprintf(stderr, "log_logstash plugin: fopen (%s) failed: %s\n", log_file,
            STRERRNO);
  } else {
    fprintf(fh, "%s\n", buf);
    if (do_close) {
      fclose(fh);
    } else {
      fflush(fh);
    }
  }
  pthread_mutex_unlock(&file_lock);
  yajl_gen_free(g);
  return;

err:
  yajl_gen_free(g);
  fprintf(stderr, "Could not correctly generate JSON message\n");
  return;
}

static void log_logstash_log(int severity, const char *msg,
                             user_data_t __attribute__((unused)) * user_data)
{
  if (severity > log_level)
    return;

#if HAVE_YAJL_V2
  yajl_gen g = yajl_gen_alloc(NULL);
#else
  yajl_gen g = yajl_gen_alloc(&(yajl_gen_config){0}, NULL);
#endif
  if (g == NULL) {
    fprintf(stderr, "Could not allocate JSON generator.\n");
    return;
  }

  if (yajl_gen_map_open(g) != yajl_gen_status_ok)
    goto err;
  if (yajl_gen_string(g, (u_char *)"message", strlen("message")) !=
      yajl_gen_status_ok)
    goto err;
  if (yajl_gen_string(g, (u_char *)msg, strlen(msg)) != yajl_gen_status_ok)
    goto err;

  log_logstash_print(g, severity, cdtime());
  return;
err:
  yajl_gen_free(g);
  fprintf(stderr, "Could not generate JSON message preamble\n");
  return;

}

static int log_logstash_notification(const notification_t *n,
                                     user_data_t __attribute__((unused)) *
                                         user_data)
{
  yajl_gen g;
#if HAVE_YAJL_V2
  g = yajl_gen_alloc(NULL);
#else
  yajl_gen_config conf = {};

  conf.beautify = 0;
  g = yajl_gen_alloc(&conf, NULL);
#endif

  if (g == NULL) {
    fprintf(stderr, "Could not allocate JSON generator.\n");
    return 0;
  }

  if (yajl_gen_map_open(g) != yajl_gen_status_ok)
    goto err;

  if (yajl_gen_string(g, (u_char *)"name", strlen("name")) !=
      yajl_gen_status_ok)
    goto err;

  if (strlen(n->name) > 0) {
    if (yajl_gen_string(g, (u_char *)n->name, strlen(n->name)) !=
        yajl_gen_status_ok)
      goto err;
  } else {
    if (yajl_gen_string(g, (u_char *)"notification without a message",
                        strlen("notification without a message")) !=
        yajl_gen_status_ok)
      goto err;
  }

  if (yajl_gen_string(g, (u_char *)"severity", strlen("severity")) !=
      yajl_gen_status_ok)
    goto err;

  switch (n->severity) {
    case NOTIF_FAILURE:
      if (yajl_gen_string(g, (u_char *)"failure", strlen("failure")) !=
          yajl_gen_status_ok)
        goto err;
      break;
    case NOTIF_WARNING:
      if (yajl_gen_string(g, (u_char *)"warning", strlen("warning")) !=
          yajl_gen_status_ok)
        goto err;
      break;
    case NOTIF_OKAY:
      if (yajl_gen_string(g, (u_char *)"ok", strlen("ok")) != yajl_gen_status_ok)
        goto err;
      break;
    default:
      if (yajl_gen_string(g, (u_char *)"unknown", strlen("unknown")) !=
          yajl_gen_status_ok)
        goto err;
      break;
  }

 
  if (yajl_gen_string(g, (u_char *)"labels", strlen("labels")) !=
      yajl_gen_status_ok)
    goto err;

  if (yajl_gen_map_open(g) != yajl_gen_status_ok)
    goto err;

  for (size_t i = 0; i < n->label.num; i++) {
     label_pair_t *l = n->label.ptr + i;

     if (yajl_gen_string(g, (u_char *)l->name, strlen(l->name)) !=
         yajl_gen_status_ok)
       goto err;

     if (yajl_gen_string(g, (u_char *)l->value, strlen(l->value)) !=
         yajl_gen_status_ok)
       goto err;
  }

  if (yajl_gen_map_close(g) != yajl_gen_status_ok)
    goto err;



  if (yajl_gen_string(g, (u_char *)"annotations", strlen("annotations")) !=
      yajl_gen_status_ok)
    goto err;

  if (yajl_gen_map_open(g) != yajl_gen_status_ok)
    goto err;

  for (size_t i = 0; i < n->annotation.num; i++) {
     label_pair_t *l = n->annotation.ptr + i;

     if (yajl_gen_string(g, (u_char *)l->name, strlen(l->name)) !=
         yajl_gen_status_ok)
       goto err;

     if (yajl_gen_string(g, (u_char *)l->value, strlen(l->value)) !=
         yajl_gen_status_ok)
       goto err;
  }

  if (yajl_gen_map_close(g) != yajl_gen_status_ok)
    goto err;


  log_logstash_print(g, LOG_INFO, (n->time != 0) ? n->time : cdtime());
  return 0;

err:
  yajl_gen_free(g);
  fprintf(stderr, "Could not correctly generate JSON notification\n");
  return 0;
}

void module_register(void)
{
  plugin_register_config("log_logstash", log_logstash_config, config_keys,
                         config_keys_num);
  plugin_register_log("log_logstash", log_logstash_log,
                      /* user_data = */ NULL);
  plugin_register_notification("log_logstash", log_logstash_notification,
                               /* user_data = */ NULL);
}
