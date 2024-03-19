/* SPDX-License-Identifier: GPL-2.0-only OR MIT                         */
/* SPDX-FileCopyrightText: Copyright (C) 2005-2014 Florian octo Forster */
/* SPDX-FileContributor: Florian octo Forster <octo at collectd.org>    */
/* SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>              */
/* SPDX-FileContributor: Manoj Srivastava <srivasta at google.com>      */

#pragma once

#include "ncollectd.h"
#include "libutils/config.h"
#include "ncollectd/plugin_match.h"
#include "libmetric/metric.h"
#include "libmetric/notification.h"
#include "libutils/time.h"

#include <inttypes.h>
#include <pthread.h>

#ifndef LOG_ERR
#define LOG_ERR 3
#endif
#ifndef LOG_WARNING
#define LOG_WARNING 4
#endif
#ifndef LOG_NOTICE
#define LOG_NOTICE 5
#endif
#ifndef LOG_INFO
#define LOG_INFO 6
#endif
#ifndef LOG_DEBUG
#define LOG_DEBUG 7
#endif

typedef struct {
    void *data;
    void (*free_func)(void *);
} user_data_t;

typedef struct {
    char *name;
    cdtime_t interval;
    bool normalize_interval;
} plugin_ctx_t;

typedef struct {
    int severity;
    cdtime_t time;
    char const *plugin;
    char const *file;
    int line;
    char const *func;
    char const *msg;
} log_msg_t;

/*
 * Callback types
 */
typedef int (*plugin_init_cb)(void);
typedef int (*plugin_read_cb)(user_data_t *);
typedef int (*plugin_write_cb)(metric_family_t const *, user_data_t *);
typedef int (*plugin_flush_cb)(cdtime_t timeout, user_data_t *);
typedef void (*plugin_log_cb)(const log_msg_t *msg, user_data_t *);
typedef int (*plugin_shutdown_cb)(void);
typedef int (*plugin_notification_cb)(const notification_t *, user_data_t *);

/*
 * The `plugin_register_*' functions are used to make `config', `init',
 * `read', `write' and `shutdown' functions known to the plugin
 * infrastructure. Also, the data-formats are made public like this.
 */
int plugin_register_config(const char *type,int (*callback)(config_item_t *));

int plugin_register_init(const char *name, plugin_init_cb callback);

int plugin_register_read(const char *name, int (*callback)(void));
/* "user_data" will be freed automatically, unless
 * "plugin_register_complex_read" returns an error (non-zero). */
int plugin_register_complex_read(const char *group, const char *name,
                                 plugin_read_cb callback, cdtime_t interval,
                                 user_data_t const *user_data);

int plugin_register_write(const char *group, const char *name, plugin_write_cb write_cb,
                          plugin_flush_cb flush_cb, cdtime_t flush_interval, cdtime_t flush_timeout,
                          user_data_t const *user_data);

int plugin_register_shutdown(const char *name, plugin_shutdown_cb callback);

int plugin_register_log(const char *group, const char *name,
                        plugin_log_cb callback, user_data_t const *user_data);

int plugin_register_notification(const char *group, const char *name,
                                 plugin_notification_cb callback,
                                 user_data_t const *user_data);

int plugin_unregister_config(const char *name);

int plugin_unregister_init(const char *name);

int plugin_unregister_read(const char *name);
#if 0
int plugin_unregister_read_group(const char *group);
#endif
int plugin_unregister_write(const char *name);

int plugin_unregister_shutdown(const char *name);

int plugin_unregister_log(const char *name);

int plugin_unregister_notification(const char *name);

int plugin_dispatch_metric_family_array(metric_family_t *fams, size_t size, cdtime_t time);

static inline int plugin_dispatch_metric_family(metric_family_t *fam, cdtime_t time)
{
    return plugin_dispatch_metric_family_array(fam, 1, time);
}

int plugin_dispatch_notification(const notification_t *notif);

void plugin_log(int level, const char *file, int line, const char *func, const char *format, ...)
    __attribute__((format(printf, 5, 6)));

#define PLUGIN_ERROR(...) plugin_log(LOG_ERR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define PLUGIN_WARNING(...) plugin_log(LOG_WARNING, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define PLUGIN_NOTICE(...) plugin_log(LOG_NOTICE, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define PLUGIN_INFO(...) plugin_log(LOG_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#ifdef NCOLLECTD_DEBUG
#define PLUGIN_DEBUG(...) plugin_log(LOG_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#else
#define PLUGIN_DEBUG(...) /* noop */
#endif

#define ERROR(...) plugin_log(LOG_ERR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define WARNING(...) plugin_log(LOG_WARNING, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define NOTICE(...) plugin_log(LOG_NOTICE, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define INFO(...) plugin_log(LOG_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#ifdef NCOLLECTD_DEBUG
#define DEBUG(...) plugin_log(LOG_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#else
#define DEBUG(...) /* noop */
#endif

plugin_ctx_t plugin_get_ctx(void);

plugin_ctx_t plugin_set_ctx(plugin_ctx_t ctx);

cdtime_t plugin_get_interval(void);

int plugin_thread_create(pthread_t *thread, void *(*start_routine)(void *),
                                            void *arg, char const *name);

char *plugin_procpath(const char *path);

char *plugin_syspath(const char *path);

void plugin_set_hostname(char const *hostname);

const char *plugin_get_hostname(void);

/** Check if the current process benefits from the capability passed in
 * argument. Returns zero if it does, less than zero if it doesn't or on error.
 * See capabilities(7) for the list of possible capabilities.
 * */
int plugin_check_capability(int arg);

void module_register(void);
