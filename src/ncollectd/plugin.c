// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2005-2014 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>
// SPDX-FileContributor: Manoj Srivastava <srivasta at google.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

/* _GNU_SOURCE is needed in Linux to use pthread_setname_np */
#define _GNU_SOURCE

#include "ncollectd.h"
#include "globals.h"
#include "configfile.h"
#include "plugin_internal.h"
#include "plugin_match.h"
#include "libutils/avltree.h"
#include "libutils/common.h"
#include "libutils/heap.h"
#include "libutils/complain.h"
#include "libutils/llist.h"
#include "libutils/random.h"
#include "libutils/time.h"
#include "libmdb/mdb.h"

#include <time.h>

#ifdef HAVE_PTHREAD_NP_H
#include <pthread_np.h> /* for pthread_set_name_np(3) */
#endif

#ifdef HAVE_CAPABILITY
#include <sys/capability.h>
#endif

#include <dlfcn.h>

static c_avl_tree_t *plugins_loaded;

static llist_t *list_init;
static llist_t *list_shutdown;
static llist_t *list_log;

static char *plugindir;

static pthread_key_t plugin_ctx_key;
static bool plugin_ctx_key_initialized;

mdb_t *mdb = NULL;

static metric_family_t internal_fams[FAM_NCOLLECTD_MAX] = {
    [FAM_NCOLLECTD_UPTIME] = {
        .name = "ncollectd_uptime_seconds",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_NCOLLECTD_METRICS_DISPACHED] = {
        .name = "ncollectd_metrics_dispached",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_NCOLLECTD_WRITE_QUEUE_LENGTH] = {
        .name = "ncollectd_write_queue_length",
        .type = METRIC_TYPE_GAUGE,
    },
    [FAM_NCOLLECTD_WRITE_QUEUE_DROPPED] = {
        .name = "ncollectd_write_queue_dropped",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_NCOLLECTD_PLUGIN_WRITE_TIME_SECONDS] = {
        .name = "ncollectd_plugin_write_time_seconds",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_NCOLLECTD_PLUGIN_WRITE_CALLS] = {
        .name = "ncollectd_plugin_write_calls",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_NCOLLECTD_PLUGIN_WRITE_FAILURES] = {
        .name = "ncollectd_plugin_write_failures",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_NCOLLECTD_NOTIFICATIONS_DISPACHED] = {
        .name = "ncollectd_notifications_dispached",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_NCOLLECTD_NOTIFY_QUEUE_LENGTH] = {
        .name = "ncollectd_notify_queue_length",
        .type = METRIC_TYPE_GAUGE,
    },
    [FAM_NCOLLECTD_NOTIFY_QUEUE_DROPPED] = {
        .name = "ncollectd_notify_queue_dropped",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_NCOLLECTD_PLUGIN_NOTIFY_TIME_SECONDS] = {
        .name = "ncollectd_plugin_notify_time_seconds",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_NCOLLECTD_PLUGIN_NOTIFY_CALLS] = {
        .name = "ncollectd_plugin_notify_calls",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_NCOLLECTD_PLUGIN_NOTIFY_FAILURES] = {
        .name = "ncollectd_plugin_notify_failures",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_NCOLLECTD_PLUGIN_READ_TIME_SECONDS] = {
        .name = "ncollectd_plugin_read_time_seconds",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_NCOLLECTD_PLUGIN_READ_CALLS] = {
        .name = "ncollectd_plugin_read_calls",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_NCOLLECTD_PLUGIN_READ_FAILURES] = {
        .name = "ncollectd_plugin_read_failures",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_NCOLLECTD_CACHE_SIZE] = {
        .name = "ncollectd_cache_size",
        .type = METRIC_TYPE_GAUGE,
    },
};

static const char *plugin_get_dir(void)
{
    if (plugindir == NULL)
        return PLUGINDIR;
    else
        return plugindir;
}

static int plugin_update_internal_statistics(void)
{
    static time_t ncollectd_uptime = 0;

    if (ncollectd_uptime == 0)
        ncollectd_uptime = time(NULL);

    metric_family_append(&internal_fams[FAM_NCOLLECTD_UPTIME],
                         VALUE_COUNTER(time(NULL) - ncollectd_uptime), NULL, NULL);

    plugin_write_stats(internal_fams);
    plugin_notify_stats(internal_fams);
    plugin_read_stats(internal_fams);

//  TODO cache metrics

    plugin_dispatch_metric_family_array(internal_fams, FAM_NCOLLECTD_MAX, 0);

    return 0;
}

void free_userdata(const user_data_t *ud)
{
    if (ud == NULL)
        return;

    if ((ud->data != NULL) && (ud->free_func != NULL)) {
        ud->free_func(ud->data);
    }
}

static void destroy_callback(callback_func_t *cf)
{
    if (cf == NULL)
        return;

    free_userdata(&cf->cf_udata);
    free(cf);
}

static void destroy_all_callbacks(llist_t **list)
{
    if (*list == NULL)
        return;

    llentry_t *le = llist_head(*list);
    while (le != NULL) {
        llentry_t *le_next;

        le_next = le->next;

        free(le->key);
        destroy_callback(le->value);
        le->value = NULL;

        le = le_next;
    }

    llist_destroy(*list);
    *list = NULL;
}

static int register_callback(llist_t **list, const char *name, callback_func_t *cf)
{
    if (*list == NULL) {
        *list = llist_create();
        if (*list == NULL) {
            ERROR("llist_create failed.");
            destroy_callback(cf);
            return -1;
        }
    }

    char *key = strdup(name);
    if (key == NULL) {
        ERROR("strdup failed.");
        destroy_callback(cf);
        return -1;
    }

    llentry_t *le = llist_search(*list, name);
    if (le == NULL) {
        le = llentry_create(key, cf);
        if (le == NULL) {
            ERROR("llentry_create failed.");
            free(key);
            destroy_callback(cf);
            return -1;
        }

        llist_append(*list, le);
    } else {
        callback_func_t *old_cf = le->value;
        le->value = cf;

        PLUGIN_WARNING("A callback named '%s' already exists - overwriting the old entry!", name);

        destroy_callback(old_cf);
        free(key);
    }

    return 0;
}

strlist_t *list_callbacks(llist_t **list)
{
    int n = llist_size(*list);

    strlist_t *sl = strlist_alloc(n);
    if (sl == NULL)
        return NULL;

    if (n == 0)
        return sl;

    for (llentry_t *le = llist_head(*list); le != NULL; le = le->next) {
        if (le->key!= NULL)
            strlist_append(sl, le->key);
    }

    return sl;
}

int create_register_callback(llist_t **list, const char *name, callback_func_t *icf)
{
    if (name == NULL || icf == NULL)
        return EINVAL;

    callback_func_t *cf = calloc(1, sizeof(*cf));
    if (cf == NULL) {
        free_userdata(&icf->cf_udata);
        ERROR("Calloc failed.");
        return ENOMEM;
    }

    memcpy(cf, icf, sizeof(*cf));

    cf->cf_ctx = plugin_get_ctx();

    return register_callback(list, name, cf);
}

int plugin_unregister(llist_t *list, const char *name)
{
    if (list == NULL)
        return -1;

    llentry_t *e = llist_search(list, name);
    if (e == NULL)
        return -1;

    llist_remove(list, e);

    free(e->key);
    destroy_callback(e->value);

    llentry_destroy(e);

    return 0;
}

/* plugin_load_file loads the shared object "file" and calls its
 * "module_register" function. Returns zero on success, non-zero otherwise. */
static int plugin_load_file(char const *file, bool global, void **dlh)
{
    int flags = RTLD_NOW;
    if (global)
        flags |= RTLD_GLOBAL;

    *dlh = dlopen(file, flags);
    if (*dlh == NULL) {
        char errbuf[1024] = "";

        snprintf(errbuf, sizeof(errbuf),
                         "dlopen(\"%s\") failed: %s. "
                         "The most common cause for this problem is missing dependencies. "
                         "Use ldd(1) to check the dependencies of the plugin / shared "
                         "object.",
                         file, dlerror());

        /* This error is printed to STDERR unconditionally. If list_log is NULL,
         * plugin_log() will also print to STDERR. We avoid duplicate output by
         * checking that the list of log handlers, list_log, is not NULL. */
        fprintf(stderr, "ERROR: %s\n", errbuf);
        if (list_log != NULL) {
            ERROR("%s", errbuf);
        }

        return ENOENT;
    }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    void (*reg_handle)(void) = (void (*)(void))dlsym(*dlh, "module_register");
#pragma GCC diagnostic pop
    if (reg_handle == NULL) {
        ERROR("Couldn't find symbol \"module_register\" in \"%s\": %s\n", file, dlerror());
        dlclose(*dlh);
        *dlh = NULL;
        return ENOENT;
    }

    (*reg_handle)();
    return 0;
}

void set_thread_name(pthread_t tid, char const *name)
{
#if defined(HAVE_PTHREAD_SETNAME_NP) || defined(HAVE_PTHREAD_SET_NAME_NP)

    /* glibc limits the length of the name and fails if the passed string
     * is too long, so we truncate it here. */
    char tname[THREAD_NAME_MAX];
    if (strlen(name) >= THREAD_NAME_MAX)
        WARNING("set_thread_name(\"%s\"): name too long", name);
    sstrncpy(tname, name, sizeof(tname));

#if defined(HAVE_PTHREAD_SETNAME_NP)
    #if defined(HAVE_PTHREAD_SETNAME_NP_THREE_ARGS)
        int status = pthread_setname_np(tid, tname, NULL);
    #else
        int status = pthread_setname_np(tid, tname);
    #endif
    if (status != 0)
        ERROR("set_thread_name(\"%s\"): %s", tname, STRERROR(status));
#else /* if defined(HAVE_PTHREAD_SET_NAME_NP) */
    pthread_set_name_np(tid, tname);
#endif
#else
    (void)tid;
    (void)name;
#endif
}

void set_thread_setaffinity(pthread_attr_t *attr, char const *name)
{
#ifdef HAVE_PTHREAD_ATTR_SETAFFINITY_NP
    int ncpu = global_option_get_cpumap(name);
    if (ncpu < 0)
      return;

    if (ncpu >= CPU_SETSIZE) {
        ERROR("cpu number '%d' is greater than CPU_SETSIZE(%d).", ncpu, CPU_SETSIZE);
        return;
    }

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(ncpu, &cpuset);
    int status = pthread_attr_setaffinity_np(attr, sizeof(cpu_set_t), &cpuset);
    if (status != 0) {
        ERROR("pthread_attr_setaffinity_np(\"%s\"): %s", name, STRERROR(status));
    }
#else
    (void)attr;
    (void)name;
#endif
}

void plugin_set_dir(const char *dir)
{
    free(plugindir);

    if (dir == NULL) {
        plugindir = NULL;
        return;
    }

    plugindir = strdup(dir);
    if (plugindir == NULL)
        ERROR("strdup(\"%s\") failed", dir);
}

bool plugin_is_loaded(char const *name)
{
    if (plugins_loaded == NULL)
        plugins_loaded = c_avl_create((int (*)(const void *, const void *))strcasecmp);
    assert(plugins_loaded != NULL);

    int status = c_avl_get(plugins_loaded, name, /* ret_value = */ NULL);
    return status == 0;
}

static int plugin_mark_loaded(char const *name, void *dlh)
{
    char *name_copy = strdup(name);
    if (name_copy == NULL)
        return ENOMEM;

    return c_avl_insert(plugins_loaded, name_copy, dlh);
}

static void plugin_free_loaded(void)
{
    if (plugins_loaded == NULL)
        return;

    void *name, *dlh;
    while (c_avl_pick(plugins_loaded, &name, &dlh) == 0) {
        free(name);
        if (dlh != NULL)
            dlclose(dlh);
    }

    c_avl_destroy(plugins_loaded);
    plugins_loaded = NULL;
}

#define BUFSIZE 512
#define SHLIB_SUFFIX ".so"

int plugin_load(char const *plugin_name, bool global)
{
    if (plugin_name == NULL)
        return EINVAL;

    /* Check if plugin is already loaded and don't do anything in this
     * case. */
    if (plugin_is_loaded(plugin_name))
        return 0;

    const char *dir = plugin_get_dir();
    int ret = 1;

    /*
     * XXX: Magic at work:
     *
     * Some of the language bindings, for example the Python and Perl
     * plugins, need to be able to export symbols to the scripts they run.
     * For this to happen, the "Globals" flag needs to be set.
     * Unfortunately, this technical detail is hard to explain to the
     * average user and she shouldn't have to worry about this, ideally.
     * So in order to save everyone's sanity use a different default for a
     * handful of special plugins. --octo
     */
    if ((strcasecmp("perl", plugin_name) == 0) || (strcasecmp("python", plugin_name) == 0))
        global = true;

    /* 'cpu' should not match 'cpufreq'. To solve this we add SHLIB_SUFFIX to the
     * type when matching the filename */
    char typename[BUFSIZE];
    int status = snprintf(typename, sizeof(typename), "%s" SHLIB_SUFFIX, plugin_name);
    if ((status < 0) || ((size_t)status >= sizeof(typename))) {
        WARNING("Filename too long: \"%s" SHLIB_SUFFIX "\"", plugin_name);
        return -1;
    }

    DIR *dh = opendir(dir);
    if (dh == NULL) {
        ERROR("opendir (%s) failed: %s", dir, STRERRNO);
        return -1;
    }

    char filename[BUFSIZE] = "";
    struct dirent *de;
    while ((de = readdir(dh)) != NULL) {
        if (strcasecmp(de->d_name, typename))
            continue;

        status = snprintf(filename, sizeof(filename), "%s/%s", dir, de->d_name);
        if ((status < 0) || ((size_t)status >= sizeof(filename))) {
            WARNING("Filename too long: \"%s/%s\"", dir, de->d_name);
            continue;
        }

        struct stat statbuf;
        if (lstat(filename, &statbuf) == -1) {
            WARNING("stat (\"%s\") failed: %s", filename, STRERRNO);
            continue;
        } else if (!S_ISREG(statbuf.st_mode)) {
            /* don't follow symlinks */
            WARNING("%s is not a regular file.", filename);
            continue;
        }

        void *dlh;
        status = plugin_load_file(filename, global, &dlh);
        if (status == 0) {
            /* success */
            plugin_mark_loaded(plugin_name, dlh);
            ret = 0;
            INFO("plugin \"%s\" successfully loaded.", plugin_name);
            break;
        } else {
            ERROR("Load plugin \"%s\" failed with status %i.", plugin_name, status);
        }
    }

    closedir(dh);

    if (filename[0] == 0)
        ERROR("Could not find plugin \"%s\" in %s", plugin_name, dir);

    return ret;
}

/*
 * The 'register_*' functions follow
 */
int plugin_register_config(const char *type, int (*callback)(config_item_t *))
{
    return cf_register(type, callback);
}

int plugin_register_init(const char *name, plugin_init_cb callback)
{
    callback_func_t cf = { .cf_init = callback };
    return create_register_callback(&list_init, name, &cf);
}

int plugin_register_shutdown(const char *name, plugin_shutdown_cb callback)
{
    callback_func_t cf = { .cf_shutdown = callback };
    return create_register_callback(&list_shutdown, name, &cf);
}

int plugin_register_log(const char *group, const char *name,
                        plugin_log_cb callback, user_data_t const *ud)
{
    if (group == NULL) {
        ERROR("group name is NULL.");
        return EINVAL;
    }

    char *full_name = plugin_full_name(group, name);
    if (full_name == NULL)
        return -1;

    callback_func_t cf = {0};
    cf.cf_log = callback;
    if (ud == NULL) {
        cf.cf_udata = (user_data_t){ .data = NULL, .free_func = NULL };
    } else {
        cf.cf_udata = *ud;
    }

    int status = create_register_callback(&list_log, full_name, &cf);
    free(full_name);
    return status;
}

int plugin_unregister_config(const char *name)
{
    cf_unregister(name);
    return 0;
}

int plugin_unregister_init(const char *name)
{
    return plugin_unregister(list_init, name);
}

strlist_t *plugin_get_loggers(void)
{
    return list_callbacks(&list_log);
}

int plugin_unregister_shutdown(const char *name)
{
    return plugin_unregister(list_shutdown, name);
}

int plugin_unregister_log(const char *name)
{
    return plugin_unregister(list_log, name);
}

int plugin_init_all(void)
{
    int ret = 0;

    mdb = mdb_alloc();
    if (mdb == NULL) {
        ERROR("Failed to alloc mdb structures.");
        return -1;
    }

    /* Init the value cache */
    int status = mdb_init(mdb);
    if (status != 0) {
        mdb_free(mdb);
        ERROR("Failed to init mdb structures.");
        return -1;
    }

    if (IS_TRUE(global_option_get("collect-internal-stats")))
        plugin_register_read("ncollectd", plugin_update_internal_statistics);

    /* Calling all init callbacks before checking if read callbacks
     * are available allows the init callbacks to register the read
     * callback. */
    llentry_t *le = llist_head(list_init);
    while (le != NULL) {
        callback_func_t *cf = le->value;
        plugin_ctx_t old_ctx = plugin_set_ctx(cf->cf_ctx);
        plugin_init_cb callback = cf->cf_init;
        status = (*callback)();
        plugin_set_ctx(old_ctx);

        if (status != 0) {
            ERROR("Initialization of plugin '%s' failed with status %i. "
                  "Plugin will be unloaded.", le->key, status);
            /* Plugins that register read callbacks from the init
             * callback should take care of appropriate error
             * handling themselves. */
            /* FIXME: Unload _all_ functions */
            plugin_unregister_read(le->key);
            ret = -1;
        }

        le = le->next;
    }

    plugin_init_notify();
    plugin_init_write();
    plugin_init_read();

    return ret;
}

int plugin_shutdown_all(void)
{
    destroy_all_callbacks(&list_init);

    cf_unregister_all();

    stop_read_threads();

    /* ask all plugins to write out the state they kept. */
    /* blocks until all write threads have shut down. */
    plugin_shutdown_write();
    plugin_shutdown_notify();

    llentry_t *le = NULL;
    if (list_shutdown != NULL)
        le = llist_head(list_shutdown);

    int ret = 0; // Assume success.
    while (le != NULL) {
        callback_func_t *cf = le->value;
        plugin_ctx_t old_ctx = plugin_set_ctx(cf->cf_ctx);
        plugin_shutdown_cb callback = cf->cf_shutdown;

        /* Advance the pointer before calling the callback allows
         * shutdown functions to unregister themselves. If done the
         * other way around the memory 'le' points to will be freed
         * after callback returns. */
        le = le->next;

        if ((*callback)() != 0)
            ret = -1;

        plugin_set_ctx(old_ctx);
    }

    destroy_all_callbacks(&list_shutdown);
    destroy_all_callbacks(&list_log);

    plugin_free_loaded();
    plugin_free_register_match();

    mdb_shutdown(mdb);
    mdb_free(mdb);
    mdb = NULL;

    free(plugindir);

    global_options_free();

    return ret;
}

static int plugin_dispatch_log(const log_msg_t *msg)
{
    if (msg == NULL)
        return EINVAL;

    if (list_log == NULL) {
        if (msg->plugin != NULL)
            fprintf(stderr, "plugin %s %s(%s:%d): %s\n",
                            msg->plugin, msg->func, msg->file, msg->line, msg->msg);
        else
            fprintf(stderr, "%s(%s:%d): %s\n",
                            msg->func, msg->file, msg->line, msg->msg);
        return 0;
    }

    llentry_t *le = llist_head(list_log);
    while (le != NULL) {
        callback_func_t *cf = le->value;
        plugin_log_cb callback = cf->cf_log;

        /* do not switch plugin context; rather keep the context
         * (interval) information of the calling plugin */

        (*callback)(msg, &cf->cf_udata);

        le = le->next;
    }

    return 0;
}

void plugin_log(int level, const char *file, int line, const char *func, const char *format, ...)
{
#ifndef NCOLLECTD_DEBUG
    if (level >= LOG_DEBUG)
        return;
#endif

    char const *name = plugin_get_ctx().name;
    if (name == NULL)
        name = "UNKNOWN";

    va_list ap;
    va_start(ap, format);
    char msg[1024];
    vsnprintf(msg, sizeof(msg), format, ap);
    msg[sizeof(msg) - 1] = '\0';
    va_end(ap);

    log_msg_t log = {
        .severity = level,
        .time = cdtime(),
        .plugin = name,
        .file = file,
        .line = line,
        .func = func,
        .msg = msg,
    };

    plugin_dispatch_log(&log);
}

void daemon_log(int level, const char *file, int line, const char *func, const char *format, ...)
{
#ifndef NCOLLECTD_DEBUG
    if (level >= LOG_DEBUG)
        return;
#endif

    va_list ap;
    va_start(ap, format);
    char msg[1024];
    vsnprintf(msg, sizeof(msg), format, ap);
    msg[sizeof(msg) - 1] = '\0';
    va_end(ap);

    log_msg_t log = {
        .severity = level,
        .time = cdtime(),
        .plugin = NULL,
        .file = file,
        .line = line,
        .func = func,
        .msg = msg,
    };

    plugin_dispatch_log(&log);
}

int parse_log_severity(const char *severity)
{
    int log_level = -1;

    if ((strcasecmp(severity, "emerg") == 0) ||
        (strcasecmp(severity, "alert") == 0) ||
        (strcasecmp(severity, "crit") == 0) ||
        (strcasecmp(severity, "err") == 0)) {
        log_level = LOG_ERR;
    } else if (strcasecmp(severity, "warning") == 0) {
        log_level = LOG_WARNING;
    } else if (strcasecmp(severity, "notice") == 0) {
        log_level = LOG_NOTICE;
    } else if (strcasecmp(severity, "info") == 0) {
        log_level = LOG_INFO;
#ifdef NCOLLECTD_DEBUG
    } else if (strcasecmp(severity, "debug") == 0) {
        log_level = LOG_DEBUG;
#endif
    }

    return log_level;
}

static void plugin_ctx_destructor(void *ctx)
{
    free(ctx);
}

static plugin_ctx_t ctx_init = {/* interval = */ 0};

static plugin_ctx_t *plugin_ctx_create(void)
{
    plugin_ctx_t *ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        ERROR("Failed to allocate plugin context: %s", STRERRNO);
        return NULL;
    }

    *ctx = ctx_init;
    assert(plugin_ctx_key_initialized);
    pthread_setspecific(plugin_ctx_key, ctx);
    DEBUG("Created new plugin context.");
    return ctx;
}

void plugin_init_ctx(void)
{
    pthread_key_create(&plugin_ctx_key, plugin_ctx_destructor);
    plugin_ctx_key_initialized = true;
} /* void plugin_init_ctx */

plugin_ctx_t plugin_get_ctx(void)
{
    assert(plugin_ctx_key_initialized);
    plugin_ctx_t *ctx = pthread_getspecific(plugin_ctx_key);

    if (ctx == NULL) {
        ctx = plugin_ctx_create();
        /* this must no happen -- exit() instead? */
        if (ctx == NULL)
            return ctx_init;
    }

    return *ctx;
}

plugin_ctx_t plugin_set_ctx(plugin_ctx_t ctx)
{
    assert(plugin_ctx_key_initialized);
    plugin_ctx_t *c = pthread_getspecific(plugin_ctx_key);

    if (c == NULL) {
        c = plugin_ctx_create();
        /* this must no happen -- exit() instead? */
        if (c == NULL)
            return ctx_init;
    }

    plugin_ctx_t old = *c;
    *c = ctx;

    return old;
}

cdtime_t plugin_get_interval(void)
{
    cdtime_t interval = plugin_get_ctx().interval;
    if (interval > 0)
        return interval;

    PLUGIN_ERROR("Unable to determine interval from context.");

    return cf_get_default_interval();
}

typedef struct {
    plugin_ctx_t ctx;
    void *(*start_routine)(void *);
    void *arg;
} plugin_thread_t;

static void *plugin_thread_start(void *arg)
{
    plugin_thread_t *plugin_thread = arg;

    void *(*start_routine)(void *) = plugin_thread->start_routine;
    void *plugin_arg = plugin_thread->arg;

    plugin_set_ctx(plugin_thread->ctx);

    free(plugin_thread);

    return start_routine(plugin_arg);
}

int plugin_thread_create(pthread_t *thread, void *(*start_routine)(void *),
                                            void *arg, char const *name)
{
    plugin_thread_t *plugin_thread = malloc(sizeof(*plugin_thread));
    if (plugin_thread == NULL)
        return ENOMEM;

    plugin_thread->ctx = plugin_get_ctx();
    plugin_thread->start_routine = start_routine;
    plugin_thread->arg = arg;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if (name != NULL)
        set_thread_setaffinity(&attr, name);

    int ret = pthread_create(thread, &attr, plugin_thread_start, plugin_thread);
    pthread_attr_destroy(&attr);
    if (ret != 0) {
        free(plugin_thread);
        return ret;
    }

    if (name != NULL)
        set_thread_name(*thread, name);

    return 0;
}

char *plugin_procpath(const char *path)
{
    static const char *proc_path = NULL;
    if (proc_path == NULL)
        proc_path = global_option_get("proc-path");

    if (path == NULL)
        return strdup(proc_path);

    size_t len = strlen(proc_path) + strlen(path) + 2;
    char *rpath = malloc(len);
    if (rpath == NULL)
        return NULL;

    ssnprintf(rpath, len, "%s/%s", proc_path, path);
    return rpath;
}

char *plugin_syspath(const char *path)
{
    static const char *sys_path = NULL;
    if (sys_path == NULL)
        sys_path = global_option_get("sys-path");

    if (path == NULL)
        return strdup(sys_path);

    size_t len = strlen(sys_path) + strlen(path) + 2;
    char *rpath = malloc (len);
    if (rpath == NULL)
        return NULL;

    ssnprintf(rpath, len, "%s/%s", sys_path, path);
    return rpath;
}

void plugin_set_hostname(char const *hostname)
{
    hostname_set(hostname);
}

const char *plugin_get_hostname(void)
{
    return hostname_g;
}

#ifdef HAVE_CAPABILITY
int plugin_check_capability(int arg)
{
    cap_value_t cap_value = (cap_value_t)arg;
    cap_t cap;
    cap_flag_value_t cap_flag_value;

    if (!CAP_IS_SUPPORTED(cap_value))
        return -1;

    if (!(cap = cap_get_proc())) {
        ERROR("check_capability: cap_get_proc failed.");
        return -1;
    }

    if (cap_get_flag(cap, cap_value, CAP_EFFECTIVE, &cap_flag_value) < 0) {
        ERROR("check_capability: cap_get_flag failed.");
        cap_free(cap);
        return -1;
    }

    cap_free(cap);

    return cap_flag_value != CAP_SET;
}
#else
int plugin_check_capability(__attribute__((unused)) int arg)
{
    WARNING("check_capability: unsupported capability implementation. "
            "Some plugin(s) may require elevated privileges to work properly.");
    return 0;
}
#endif

char *plugin_full_name(const char *group, const char *name)
{
    if (group == NULL)
        return NULL;

    char *full_name = NULL;
    if (name != NULL) {
        size_t full_name_size = strlen(group) + strlen(name) + 2;
        full_name = malloc(full_name_size);
        if (full_name == NULL) {
            ERROR("malloc failed.");
            return NULL;
        }
        ssnprintf(full_name, full_name_size, "%s/%s", group, name);
    } else {
        full_name = strdup(group);
        if (full_name == NULL) {
            ERROR("strdup failed.");
            return NULL;
        }
    }

    return full_name;
}
