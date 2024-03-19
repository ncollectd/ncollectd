/* SPDX-License-Identifier: GPL-2.0-only OR MIT                         */
/* SPDX-FileCopyrightText: Copyright (C) 2005-2014 Florian octo Forster */
/* SPDX-FileContributor: Florian octo Forster <octo at collectd.org>    */
/* SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>              */
/* SPDX-FileContributor: Manoj Srivastava <srivasta at google.com>      */

#pragma once

#include "ncollectd.h"
#include "configfile.h"
#include "libmetric/metric.h"
#include "libmetric/notification.h"
#include "libutils/time.h"
#include "libutils/heap.h"
#include "libutils/llist.h"
#include "libutils/strlist.h"

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

enum {
    FAM_NCOLLECTD_UPTIME,
    FAM_NCOLLECTD_METRICS_DISPACHED,
    FAM_NCOLLECTD_WRITE_QUEUE_LENGTH,
    FAM_NCOLLECTD_WRITE_QUEUE_DROPPED,
    FAM_NCOLLECTD_PLUGIN_WRITE_TIME_SECONDS,
    FAM_NCOLLECTD_PLUGIN_WRITE_CALLS,
    FAM_NCOLLECTD_PLUGIN_WRITE_FAILURES,
    FAM_NCOLLECTD_NOTIFICATIONS_DISPACHED,
    FAM_NCOLLECTD_NOTIFY_QUEUE_LENGTH,
    FAM_NCOLLECTD_NOTIFY_QUEUE_DROPPED,
    FAM_NCOLLECTD_PLUGIN_NOTIFY_TIME_SECONDS,
    FAM_NCOLLECTD_PLUGIN_NOTIFY_CALLS,
    FAM_NCOLLECTD_PLUGIN_NOTIFY_FAILURES,
    FAM_NCOLLECTD_PLUGIN_READ_TIME_SECONDS,
    FAM_NCOLLECTD_PLUGIN_READ_CALLS,
    FAM_NCOLLECTD_PLUGIN_READ_FAILURES,
    FAM_NCOLLECTD_CACHE_SIZE,
    FAM_NCOLLECTD_MAX,
};

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

typedef int (*plugin_init_cb)(void);
typedef int (*plugin_read_cb)(user_data_t *);
typedef int (*plugin_write_cb)(metric_family_t const *, user_data_t *);
typedef int (*plugin_flush_cb)(cdtime_t timeout, user_data_t *);
typedef void (*plugin_log_cb)(const log_msg_t *msg, user_data_t *);
typedef int (*plugin_shutdown_cb)(void);
typedef int (*plugin_notification_cb)(const notification_t *, user_data_t *);

typedef struct {
    union {
        plugin_init_cb cf_init;
        plugin_read_cb cf_read;
        struct {
            plugin_write_cb cf_write;
            plugin_flush_cb cf_flush;
            cdtime_t flush_interval;
        };
        plugin_log_cb cf_log;
        plugin_shutdown_cb cf_shutdown;
        plugin_notification_cb cf_notification;
    };
    user_data_t cf_udata;
    plugin_ctx_t cf_ctx;
} callback_func_t;

void free_userdata(const user_data_t *ud);

#ifdef PTHREAD_MAX_NAMELEN_NP
#define THREAD_NAME_MAX PTHREAD_MAX_NAMELEN_NP
#else
#define THREAD_NAME_MAX 16
#endif

void set_thread_name(pthread_t tid, char const *name);

strlist_t *list_callbacks(llist_t **list);

int create_register_callback(llist_t **list, const char *name, callback_func_t *icf);

int plugin_unregister(llist_t *list, const char *name);

int plugin_register_read(const char *name, int (*callback)(void));
int plugin_register_complex_read(const char *group, const char *name,
                                 plugin_read_cb callback, cdtime_t interval,
                                 user_data_t const *user_data);
int plugin_unregister_read(const char *name);
void plugin_read_stats(metric_family_t *fams);

void stop_read_threads(void);
int plugin_init_read(void);

int plugin_init_write(void);
void plugin_shutdown_write(void);
void plugin_write_stats(metric_family_t *fams);

int plugin_init_notify(void);
void plugin_shutdown_notify(void);
void plugin_notify_stats(metric_family_t *fams);

void plugin_set_dir(const char *dir);
int plugin_load(const char *name, bool global);
bool plugin_is_loaded(char const *name);
int plugin_init_all(void);
int plugin_read_all_once(void);
int plugin_shutdown_all(void);

int plugin_write(const char *plugin, metric_family_t *fam, bool clone);

strlist_t *plugin_get_writers(void);
strlist_t *plugin_get_readers(void);
strlist_t *plugin_get_loggers(void);
strlist_t *plugin_get_notificators(void);

void plugin_init_ctx(void);
plugin_ctx_t plugin_get_ctx(void);
plugin_ctx_t plugin_set_ctx(plugin_ctx_t ctx);

int plugin_dispatch_metric_family_array(metric_family_t *fams, size_t size, cdtime_t time);
static inline int plugin_dispatch_metric_family(metric_family_t *fam, cdtime_t time)
{
  return plugin_dispatch_metric_family_array(fam, 1, time);
}

int plugin_dispatch_notification(const notification_t *notif);

cdtime_t plugin_get_interval(void);

int plugin_thread_create(pthread_t *thread, void *(*start_routine)(void *),
                         void *arg, char const *name);

char *plugin_full_name(const char *group, const char *name);

void daemon_log(int level, const char *file, int line, const char *func, const char *format, ...)
    __attribute__((format(printf, 5, 6)));

#define ERROR(...) daemon_log(LOG_ERR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define WARNING(...) daemon_log(LOG_WARNING, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define NOTICE(...) daemon_log(LOG_NOTICE, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define INFO(...) daemon_log(LOG_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#ifdef NCOLLECTD_DEBUG
#define DEBUG(...) daemon_log(LOG_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#else
#define DEBUG(...) /* noop */
#endif


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
