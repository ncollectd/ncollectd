// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2013-2015 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

#include "plugin.h"

#include "libutils/common.h"
#include "libmetric/metric.h"
#include "libmetric/label_set.h"

static int (*g_config_cb)(config_item_t *);
static plugin_init_cb g_init_cb;
static int (*g_simple_read_cb)(void);
static plugin_write_cb g_write_cb;
static plugin_flush_cb g_flush_cb;
static cdtime_t g_flush_interval;
static cdtime_t g_flush_timeout;
static user_data_t g_write_ud;
static int (*g_complex_read_cb)(user_data_t *);
static user_data_t g_complex_read_ud;
static int (*g_shutdown_cb)(void);
static plugin_notification_cb g_notification_cb;
static user_data_t g_notification_ud;
static plugin_log_cb g_log_cb;
static user_data_t g_log_ud;

static const char *g_procpath;
static const char *g_syspath;

char *hostname_g = "localhost.localdomain";

void plugin_test_metrics_reset(void);
int plugin_test_metrics_cmp(const char *filename);

void plugin_set_dir(__attribute__((unused)) const char *dir)
{
    /* nop */
}

int plugin_load(__attribute__((unused)) const char *name, __attribute__((unused)) bool global)
{
    return 0;
}

bool plugin_is_loaded(__attribute__((unused)) const char *name)
{
    return false;
}

int plugin_register_config(__attribute__((unused)) const char *name,
                           int (*callback)(config_item_t *))
{
    g_config_cb = callback;
    return 0;
}

int plugin_unregister_config(__attribute__((unused)) char const *name)
{
    g_config_cb = NULL;
    return 0;
}

int plugin_register_init(__attribute__((unused)) const char *name,
                         plugin_init_cb callback)
{
    g_init_cb = callback;
    return 0;
}

int plugin_unregister_init(__attribute__((unused)) char const *name)
{
    g_init_cb = NULL;
    return 0;
}

int plugin_register_read(__attribute__((unused)) const char *name,
                         int (*callback)(void))
{
    g_simple_read_cb = callback;
    return 0;
}

int plugin_unregister_read(__attribute__((unused)) char const *name)
{
    g_simple_read_cb = NULL;
    return 0;
}

int plugin_register_write(__attribute__((unused)) const char *group,
                          __attribute__((unused)) const char *name,
                          plugin_write_cb write_cb,
                          plugin_flush_cb flush_cb, cdtime_t flush_interval, cdtime_t flush_timeout,
                          user_data_t const *ud)
{
    g_write_cb = write_cb;
    g_flush_cb = flush_cb;
    g_flush_interval = flush_interval;
    g_flush_timeout = flush_timeout;
    if (ud == NULL)
        memset(&g_write_ud, 0, sizeof(g_write_ud));
    else
        memcpy(&g_write_ud, ud, sizeof(g_write_ud));
    return 0;
}

int plugin_unregister_write(__attribute__((unused)) char const *name)
{
    g_write_cb = NULL;
    g_flush_cb = NULL;
    g_flush_interval = 0;
    g_flush_timeout = 0;
    if (g_write_ud.free_func != NULL)
        g_write_ud.free_func(g_write_ud.data);
    memset(&g_write_ud, 0, sizeof(g_write_ud));
    return 0;
}

int plugin_register_complex_read(__attribute__((unused)) const char *group,
                                 __attribute__((unused)) const char *name,
                                 int (*callback)(user_data_t *),
                                 __attribute__((unused)) cdtime_t interval,
                                 user_data_t const *user_data)
{
    g_complex_read_cb = callback;
    if (user_data == NULL)
        memset(&g_complex_read_ud, 0, sizeof(g_complex_read_ud));
    else
        memcpy(&g_complex_read_ud, user_data, sizeof(g_complex_read_ud));
    return 0;
}

int plugin_unregister_read_group(__attribute__((unused)) char const *name)
{
    g_complex_read_cb = NULL;
    if (g_complex_read_ud.free_func != NULL)
        g_complex_read_ud.free_func(g_complex_read_ud.data);
    memset(&g_complex_read_ud, 0, sizeof(g_complex_read_ud));
    return 0;
}

int plugin_register_shutdown(__attribute__((unused)) const char *name, int (*callback)(void))
{
    g_shutdown_cb = callback;
    return 0;
}

int plugin_unregister_shutdown(__attribute__((unused)) char const *name)
{
    g_shutdown_cb = NULL;
    return 0;
}

int plugin_register_notification(__attribute__((unused)) const char *group,
                                 __attribute__((unused)) const char *name,
                                 plugin_notification_cb callback, user_data_t const *user_data)
{
    g_notification_cb = callback;
    if (user_data == NULL)
        memset(&g_notification_ud, 0, sizeof(g_notification_ud));
    else
        memcpy(&g_notification_ud, user_data, sizeof(g_notification_ud));
    return 0;
}

int plugin_unregister_notification(__attribute__((unused)) char const *name)
{
    g_notification_cb = NULL;
    if (g_notification_ud.free_func != NULL)
        g_notification_ud.free_func(g_notification_ud.data);
    memset(&g_notification_ud, 0, sizeof(g_notification_ud));
    return 0;
}

int plugin_register_log(__attribute__((unused)) const char *name,
                        __attribute__((unused)) const char *group,
                        plugin_log_cb callback, user_data_t const *user_data)
{
    g_log_cb = callback;
    if (user_data == NULL)
        memset(&g_log_ud, 0, sizeof(g_log_ud));
    else
        memcpy(&g_log_ud, user_data, sizeof(g_log_ud));
    return 0;
}

int plugin_unregister_log(__attribute__((unused)) char const *name)
{
    g_log_cb = NULL;
    if (g_log_ud.free_func != NULL)
        g_log_ud.free_func(g_log_ud.data);
    memset(&g_log_ud, 0, sizeof(g_log_ud));
    return 0;
}

int plugin_dispatch_notification(__attribute__((unused)) const notification_t *notif)
{
    return 0;
}

char *plugin_procpath(const char *path)
{
    if (g_procpath == NULL)
        g_procpath = "/proc";

    if (path == NULL)
        return strdup(g_procpath);

    size_t len = strlen(g_procpath) + strlen(path) + 2;
    char *rpath = malloc (len);
    if (rpath == NULL)
        return NULL;

    ssnprintf(rpath, len, "%s/%s", g_procpath, path);
    return rpath;
}

char *plugin_syspath(const char *path)
{
    if (g_syspath == NULL)
        g_syspath = "/sys";

    if (path == NULL)
        return strdup(g_syspath);

    size_t len = strlen(g_syspath) + strlen(path) + 2;
    char *rpath = malloc (len);
    if (rpath == NULL)
        return NULL;

    ssnprintf(rpath, len, "%s/%s", g_syspath, path);
    return rpath;
}

int plugin_test_set_procpath(const char *path)
{
    g_procpath = path;
    return 0;
}

int plugin_test_set_syspath(const char *path)
{
    g_syspath = path;
    return 0;
}

int plugin_test_config(config_item_t *ci)
{
    if (g_config_cb == NULL)
        return 0;
    return g_config_cb(ci);
}

int plugin_test_init(void)
{
    if (g_init_cb == NULL)
        return 0;
    return g_init_cb();
}

int plugin_test_read(void)
{
    if (g_simple_read_cb != NULL)
        return g_simple_read_cb();
    if (g_complex_read_cb != 0)
        return g_complex_read_cb(&g_complex_read_ud);
    return -1;
}

int plugin_test_write(metric_family_t const *fam)
{
    if (g_write_cb == NULL)
        return 0;
    return g_write_cb(fam, &g_write_ud);
}

int plugin_test_notification(const notification_t *n)
{
    if (g_notification_cb == NULL)
        return 0;
    return g_notification_cb(n, &g_notification_ud);
}

int plugin_test_shutdown(void)
{
    if (g_shutdown_cb == NULL)
        return 0;
    return g_shutdown_cb();
}

void plugin_test_reset(void)
{
    g_config_cb = NULL;
    g_init_cb = NULL;
    g_simple_read_cb = NULL;

    g_write_cb = NULL;
    g_flush_cb = NULL;
    g_flush_interval = 0;
    g_flush_timeout = 0;
    if (g_write_ud.free_func != NULL)
        g_write_ud.free_func(g_write_ud.data);
    memset(&g_write_ud, 0, sizeof(g_write_ud));

    g_complex_read_cb = NULL;
    if (g_complex_read_ud.free_func != NULL)
        g_complex_read_ud.free_func(g_complex_read_ud.data);
    memset(&g_complex_read_ud, 0, sizeof(g_complex_read_ud));

    g_shutdown_cb = NULL;

    g_notification_cb = NULL;
    if (g_notification_ud.free_func != NULL)
        g_notification_ud.free_func(g_notification_ud.data);
    memset(&g_notification_ud, 0, sizeof(g_notification_ud));

    g_log_cb = NULL;
    if (g_log_ud.free_func != NULL)
        g_log_ud.free_func(g_log_ud.data);
    memset(&g_log_ud, 0, sizeof(g_log_ud));

    g_procpath = NULL;
    g_syspath = NULL;

    plugin_test_metrics_reset();
}

void plugin_log(int level, const char *file, int line, const char *func, const char *format, ...)
{
    char buffer[1024];
    va_list ap;

    va_start(ap, format);
    vsnprintf(buffer, sizeof(buffer), format, ap);
    va_end(ap);

    printf("plugin_log (%i, %s(%s:%d), \"%s\");\n", level, func, file, line, buffer);
}

void daemon_log(int level, const char *file, int line, const char *func, const char *format, ...)
{
    char buffer[1024];
    va_list ap;

    va_start(ap, format);
    vsnprintf(buffer, sizeof(buffer), format, ap);
    va_end(ap);

    printf("daemon_log (%i, %s(%s:%d), \"%s\");\n", level, func, file, line, buffer);
}

void plugin_init_ctx(void)
{
    /* nop */
}

plugin_ctx_t mock_context = {
    .interval = TIME_T_TO_CDTIME_T_STATIC(10),
};

plugin_ctx_t plugin_get_ctx(void)
{
    return mock_context;
}

plugin_ctx_t plugin_set_ctx(plugin_ctx_t ctx)
{
    plugin_ctx_t prev = mock_context;
    mock_context = ctx;
    return prev;
}

cdtime_t plugin_get_interval(void)
{
    return mock_context.interval;
}

int plugin_thread_create(pthread_t *thread, void *(*start_routine)(void *), void *arg,
                         __attribute__((unused)) char const *name)
{
    return pthread_create(thread, NULL, start_routine, arg);
}

int plugin_test_do_read(char *proc_path, char *sys_path, config_item_t *ci, char *expect)
{
    int status = 0;

    if (proc_path != NULL) {
        status = plugin_test_set_procpath(proc_path);
        if (status != 0) {
            fprintf(stderr, "plugin_test_set_procpath failed.\n");
            plugin_test_metrics_reset();
            return -1;
        }
    }

    if (sys_path != NULL) {
        status = plugin_test_set_syspath(sys_path);
        if (status != 0) {
            fprintf(stderr, "plugin_test_set_syspath failed.\n");
            plugin_test_metrics_reset();
            return -1;
        }
    }

    if (ci != NULL) {
        status = plugin_test_config(ci);
        if (status != 0) {
            fprintf(stderr, "plugin_test_config  failed.\n");
            plugin_test_metrics_reset();
            return -1;
        }
    }

    status = plugin_test_init();
    if (status != 0) {
        fprintf(stderr, "plugin_test_init failed.\n");
        plugin_test_metrics_reset();
        return -1;
    }

    status = plugin_test_read();
    if (status != 0) {
        fprintf(stderr, "plugin_test_read failed.\n");
        plugin_test_metrics_reset();
        return -1;
    }

    status = plugin_test_shutdown();
    if (status != 0) {
        fprintf(stderr, "plugin_test_shutdown failed.\n");
        plugin_test_metrics_reset();
        return -1;
    }

    if (expect != NULL) {
        status = plugin_test_metrics_cmp(expect);
        if (status != 0) {
            fprintf(stderr, "plugin_test_metrics_cmp failed.\n");
            plugin_test_metrics_reset();
            return -1;
        }
    }

    plugin_test_metrics_reset();

    return 0;
}
