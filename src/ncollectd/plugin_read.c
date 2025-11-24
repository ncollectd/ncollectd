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
#include "configfile.h"
#include "plugin_internal.h"
#include "libutils/avltree.h"
#include "libutils/common.h"
#include "libutils/heap.h"
#include "libutils/complain.h"
#include "libutils/llist.h"
#include "libutils/random.h"
#include "libutils/time.h"

#include <stdatomic.h>
#include <sys/resource.h>

struct read_stats_s;
typedef struct read_stats_s read_stats_t;
struct read_stats_s {
    const char *plugin;
    atomic_ullong read_time;
    atomic_ullong read_calls;
    atomic_ullong read_calls_failures;
    atomic_ullong read_cpu_user;
    atomic_ullong read_cpu_sys;
    read_stats_t *next;
};

#define RF_SIMPLE 0
#define RF_COMPLEX 1
#define RF_REMOVE 65535
struct read_func_s {
/* 'read_func_t' "inherits" from 'callback_func_t'.
 * The 'rf_super' member MUST be the first one in this structure! */
#define rf_init_cb rf_super.cf_init
#define rf_read_cb rf_super.cf_read
#define rf_write_cb rf_super.cf_write
#define rf_flush_cb rf_super.cf_flush
#define rf_log_cb rf_super.cf_log
#define rf_shutdown_cb rf_super.cf_shutdown_
#define rf_plugin_notification_cb rf_super.cf_notification
#define rf_udata rf_super.cf_udata
#define rf_ctx rf_super.cf_ctx

    callback_func_t rf_super;
    char *rf_name;
    int rf_type;
    cdtime_t rf_interval;
    cdtime_t rf_effective_interval;
    cdtime_t rf_next_read;
    read_stats_t *stats;
};
typedef struct read_func_s read_func_t;

#ifndef DEFAULT_MAX_READ_INTERVAL
#define DEFAULT_MAX_READ_INTERVAL TIME_T_TO_CDTIME_T_STATIC(86400)
#endif
static c_heap_t *read_heap;
static llist_t *read_list;
static int read_loop = 1;
static pthread_mutex_t read_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t read_cond = PTHREAD_COND_INITIALIZER;
static pthread_t *read_threads;
static size_t read_threads_num;
static cdtime_t max_read_interval = DEFAULT_MAX_READ_INTERVAL;

static pthread_mutex_t read_stats_lock = PTHREAD_MUTEX_INITIALIZER;
static read_stats_t *read_stats;

static void destroy_callback(callback_func_t *cf)
{
    if (cf == NULL)
        return;
    free_userdata(&cf->cf_udata);
    free(cf);
}

static cdtime_t plugin_normalize_interval(cdtime_t ctime, cdtime_t interval)
{
    cdtime_t rest = ctime % interval;
    if (rest == 0)
        return ctime;
    return (ctime - rest + interval);
}

static void plugin_read_stats_remove(read_stats_t *rstats)
{
    if (rstats == NULL)
        return;

    pthread_mutex_lock(&read_stats_lock);

    read_stats_t *prev = NULL;
    read_stats_t *stats = read_stats;
    while (stats != NULL) {
        if (rstats == stats) {
            read_stats_t *next = stats->next;
            free(rstats);
            if (prev == NULL)
                read_stats = next;
            else
                prev->next = next;
            break;
        }
        prev = stats;
        stats = stats->next;
    }

    pthread_mutex_unlock(&read_stats_lock);
}

static void *plugin_read_thread(void __attribute__((unused)) * args)
{
    /* coverity[MISSING_LOCK] */
    while (read_loop != 0) {
        /* Get the read function that needs to be read next.
         * We don't need to hold "read_lock" for the heap, but we need
         * to call c_heap_get_root() and pthread_cond_wait() in the
         * same protected block. */
        pthread_mutex_lock(&read_lock);
        read_func_t *rf = c_heap_get_root(read_heap);
        if (rf == NULL) {
            /* coverity[BAD_CHECK_OF_WAIT_COND] */
            pthread_cond_wait(&read_cond, &read_lock);
            pthread_mutex_unlock(&read_lock);
            continue;
        }
        pthread_mutex_unlock(&read_lock);

        if (rf->rf_interval == 0) {
            /* this should not happen, because the interval is set
             * for each plugin when loading it
             * XXX: issue a warning? */
            rf->rf_interval = plugin_get_interval();
            rf->rf_effective_interval = rf->rf_interval;

            if (rf->rf_ctx.normalize_interval)
                rf->rf_next_read = plugin_normalize_interval(cdtime(), rf->rf_interval);
            else
                rf->rf_next_read = cdtime();
        }

        /* sleep until this entry is due,
         * using pthread_cond_timedwait */
        pthread_mutex_lock(&read_lock);
        /* In pthread_cond_timedwait, spurious wakeups are possible
         * (and really happen, at least on NetBSD with > 1 CPU), thus
         * we need to re-evaluate the condition every time
         * pthread_cond_timedwait returns. */
        int rc = 0;
        while ((read_loop != 0) && (cdtime() < rf->rf_next_read) && rc == 0) {
            rc = pthread_cond_timedwait(&read_cond, &read_lock,
                                        &CDTIME_T_TO_TIMESPEC(rf->rf_next_read));
        }

        /* Must hold 'read_lock' when accessing 'rf->rf_type'. */
        int rf_type = rf->rf_type;
        pthread_mutex_unlock(&read_lock);

        /* Check if we're supposed to stop.. This may have interrupted
         * the sleep, too. */
        if (read_loop == 0) {
            /* Insert 'rf' again, so it can be free'd correctly */
            c_heap_insert(read_heap, rf);
            break;
        }

        /* The entry has been marked for deletion. The linked list
         * entry has already been removed by 'plugin_unregister_read'.
         * All we have to do here is free the 'read_func_t' and
         * continue. */
        if (rf_type == RF_REMOVE) {
            DEBUG("Destroying the '%s' callback.", rf->rf_name);
            free(rf->rf_name);
            plugin_read_stats_remove(rf->stats);
            destroy_callback((callback_func_t *)rf);
            rf = NULL;
            continue;
        }

        DEBUG("Handling '%s'.", rf->rf_name);


#ifdef HAVE_RUSAGE_THREAD
        struct rusage usage_start = {0};
        getrusage(RUSAGE_THREAD, &usage_start);
#endif
        cdtime_t start = cdtime();

        plugin_ctx_t old_ctx = plugin_set_ctx(rf->rf_ctx);
        int status = 0;
        if (rf_type == RF_SIMPLE) {
            plugin_init_cb callback = rf->rf_init_cb;

            status = (*callback)();
        } else {
            assert(rf_type == RF_COMPLEX);

            plugin_read_cb callback = rf->rf_read_cb;
            status = (*callback)(&rf->rf_udata);
        }

        plugin_set_ctx(old_ctx);

        /* update the ''next read due'' field */
        cdtime_t now = cdtime();

#ifdef HAVE_RUSAGE_THREAD
        struct rusage usage_finish = {0};
        getrusage(RUSAGE_THREAD, &usage_finish);
        cdtime_t cpu_user_time = TIMEVAL_TO_CDTIME_T(&usage_finish.ru_utime) -
                                 TIMEVAL_TO_CDTIME_T(&usage_start.ru_utime);
        cdtime_t cpu_sys_time = TIMEVAL_TO_CDTIME_T(&usage_finish.ru_stime) -
                                TIMEVAL_TO_CDTIME_T(&usage_start.ru_stime);

        atomic_fetch_add(&rf->stats->read_cpu_user, cpu_user_time);
        atomic_fetch_add(&rf->stats->read_cpu_sys, cpu_sys_time);
#endif

        /* If the function signals failure, we will increase the
         * intervals in which it will be called. */
        if (status != 0) {
            rf->rf_effective_interval *= 2;
            if (rf->rf_effective_interval > max_read_interval)
                rf->rf_effective_interval = max_read_interval;

            NOTICE("read-function of plugin '%s' failed. Will suspend it for %.3f seconds.",
                   rf->rf_name, CDTIME_T_TO_DOUBLE(rf->rf_effective_interval));
        } else {
            /* Success: Restore the interval, if it was changed. */
            rf->rf_effective_interval = rf->rf_interval;
        }

        /* calculate the time spent in the read function */
        cdtime_t elapsed = (now - start);

        atomic_fetch_add(&rf->stats->read_time, elapsed);
        atomic_fetch_add(&rf->stats->read_calls, 1);
        if (status != 0)
            atomic_fetch_add(&rf->stats->read_calls_failures, 1);

        if (elapsed > rf->rf_effective_interval)
            WARNING("read-function of the '%s' plugin took %.3f "
                    "seconds, which is above its read interval (%.3f seconds). You might "
                    "want to adjust the 'Interval' or 'ReadThreads' settings.",
                    rf->rf_name, CDTIME_T_TO_DOUBLE(elapsed),
                    CDTIME_T_TO_DOUBLE(rf->rf_effective_interval));

        DEBUG("read-function of the '%s' plugin took %.6f seconds.",
               rf->rf_name, CDTIME_T_TO_DOUBLE(elapsed));

        DEBUG("Effective interval of the '%s' plugin is %.3f seconds.",
               rf->rf_name, CDTIME_T_TO_DOUBLE(rf->rf_effective_interval));

        /* Calculate the next (absolute) time at which this function
         * should be called. */
        rf->rf_next_read += rf->rf_effective_interval;

        /* Check, if 'rf_next_read' is in the past. */
        if (rf->rf_next_read < now) {
            /* 'rf_next_read' is in the past. Insert 'now'
             * so this value doesn't trail off into the
             * past too much. */
            if (rf->rf_ctx.normalize_interval)
                rf->rf_next_read = plugin_normalize_interval(now, rf->rf_effective_interval);
            else
                rf->rf_next_read = now;
        }

        DEBUG("plugin_read_thread: Next read of the '%s' plugin at %.3f.",
                    rf->rf_name, CDTIME_T_TO_DOUBLE(rf->rf_next_read));

        /* Re-insert this read function into the heap again. */
        c_heap_insert(read_heap, rf);
    }

    pthread_exit(NULL);
    return (void *)0;
}


static int plugin_compare_read_func(const void *arg0, const void *arg1)
{

    const read_func_t *rf0 = arg0;
    const read_func_t *rf1 = arg1;

    if (rf0->rf_next_read < rf1->rf_next_read)
        return -1;
    else if (rf0->rf_next_read > rf1->rf_next_read)
        return 1;
    else
        return 0;
}

/* Add a read function to both, the heap and a linked list. The linked list if
 * used to look-up read functions, especially for the remove function. The heap
 * is used to determine which plugin to read next. */
static int plugin_insert_read(read_func_t *rf)
{
    if (rf->rf_ctx.normalize_interval)
        rf->rf_next_read = plugin_normalize_interval(cdtime(), rf->rf_interval);
    else
        rf->rf_next_read = cdtime();
    rf->rf_effective_interval = rf->rf_interval;

    pthread_mutex_lock(&read_lock);

    if (read_list == NULL) {
        read_list = llist_create();
        if (read_list == NULL) {
            pthread_mutex_unlock(&read_lock);
            ERROR("read_list failed.");
            return -1;
        }
    }

    if (read_heap == NULL) {
        read_heap = c_heap_create(plugin_compare_read_func);
        if (read_heap == NULL) {
            pthread_mutex_unlock(&read_lock);
            ERROR("c_heap_create failed.");
            return -1;
        }
    }

    llentry_t *le = llist_search(read_list, rf->rf_name);
    if (le != NULL) {
        pthread_mutex_unlock(&read_lock);
        PLUGIN_WARNING("The read function '%s' is already registered. "
                       "Check for duplicates in your configuration!", rf->rf_name);
        return EINVAL;
    }

    le = llentry_create(rf->rf_name, rf);
    if (le == NULL) {
        pthread_mutex_unlock(&read_lock);
        ERROR("llentry_create failed.");
        return -1;
    }

    int status = c_heap_insert(read_heap, rf);
    if (status != 0) {
        pthread_mutex_unlock(&read_lock);
        ERROR("c_heap_insert failed.");
        llentry_destroy(le);
        return -1;
    }

    /* This does not fail. */
    llist_append(read_list, le);

    /* Wake up all the read threads. */
    pthread_cond_broadcast(&read_cond);
    pthread_mutex_unlock(&read_lock);
    return 0;
}

static void destroy_read_heap(void)
{
    if (read_heap == NULL)
        return;

    while (true) {
        read_func_t *rf;

        rf = c_heap_get_root(read_heap);
        if (rf == NULL)
            break;
        free(rf->rf_name);
        destroy_callback((callback_func_t *)rf);
    }

    c_heap_destroy(read_heap);
    read_heap = NULL;
}

static void start_read_threads(size_t num)
{
    if (read_threads != NULL)
        return;

    read_threads = calloc(num, sizeof(*read_threads));
    if (read_threads == NULL) {
        ERROR("plugin: start_read_threads: calloc failed.");
        return;
    }

    read_threads_num = 0;
    for (size_t i = 0; i < num; i++) {
        char name[THREAD_NAME_MAX];

        ssnprintf(name, sizeof(name), "reader#%" PRIu64, (uint64_t)read_threads_num);

        pthread_attr_t attr;
        pthread_attr_init(&attr);
        set_thread_setaffinity(&attr, name);

        int status = pthread_create(read_threads + read_threads_num, &attr, plugin_read_thread,
                                    /* arg = */ NULL);
        pthread_attr_destroy(&attr);
        if (status != 0) {
            ERROR("plugin: start_read_threads: pthread_create failed with status %i " "(%s).",
                   status, STRERROR(status));
            return;
        }

        set_thread_name(read_threads[read_threads_num], name);

        read_threads_num++;
    }
}

void stop_read_threads(void)
{
    if (read_threads == NULL)
        return;

    INFO("collectd: Stopping %" PRIsz " read threads.", read_threads_num);

    pthread_mutex_lock(&read_lock);
    read_loop = 0;
    DEBUG("plugin: stop_read_threads: Signalling 'read_cond'");
    pthread_cond_broadcast(&read_cond);
    pthread_mutex_unlock(&read_lock);

    for (size_t i = 0; i < read_threads_num; i++) {
        if (pthread_join(read_threads[i], NULL) != 0) {
            ERROR("plugin: stop_read_threads: pthread_join failed.");
        }
        read_threads[i] = (pthread_t)0;
    }
    free(read_threads);
    read_threads_num = 0;

    pthread_mutex_lock(&read_lock);
    llist_destroy(read_list);
    read_list = NULL;
    pthread_mutex_unlock(&read_lock);

    destroy_read_heap();
}

int plugin_register_read(const char *name, int (*callback)(void))
{
    read_func_t *rf = calloc(1, sizeof(*rf));
    if (rf == NULL) {
        ERROR("calloc failed.");
        return ENOMEM;
    }

    read_stats_t *stats = calloc(1, sizeof(*stats));
    if (stats == NULL) {
        ERROR("calloc failed.");
        free(rf);
        return ENOMEM;
    }

    rf->rf_init_cb = callback;
    rf->rf_udata.data = NULL;
    rf->rf_udata.free_func = NULL;
    rf->rf_ctx = plugin_get_ctx();

    rf->rf_name = strdup(name);
    if (rf->rf_name == NULL) {
        ERROR("strdup failed.");
        free(rf);
        free(stats);
        return ENOMEM;
    }

    rf->rf_type = RF_SIMPLE;
    rf->rf_interval = plugin_get_interval();
    rf->rf_ctx.interval = rf->rf_interval;

    rf->stats = stats;
    rf->stats->plugin = name;

    int status = plugin_insert_read(rf);
    if (status != 0) {
        free(rf->rf_name);
        free(stats);
        free(rf);
        return -1;
    }

    pthread_mutex_lock(&read_stats_lock);
    stats->next = read_stats;
    read_stats = stats;
    pthread_mutex_unlock(&read_stats_lock);

    return status;
}

int plugin_register_complex_read(const char *group, const char *name, plugin_read_cb callback,
                                 cdtime_t interval, user_data_t const *user_data)
{
    if (group == NULL) {
        ERROR("group name is NULL.");
        return EINVAL;
    }

    char *full_name = plugin_full_name(group, name);
    if (full_name == NULL) {
        free_userdata(user_data);
        return ENOMEM;
    }

    read_func_t *rf = calloc(1, sizeof(*rf));
    if (rf == NULL) {
        free(full_name);
        free_userdata(user_data);
        ERROR("calloc failed.");
        return ENOMEM;
    }

    read_stats_t *stats = calloc(1, sizeof(*stats));
    if (stats == NULL) {
        ERROR("calloc failed.");
        free(full_name);
        free_userdata(user_data);
        free(rf);
        return ENOMEM;
    }

    rf->rf_read_cb = callback;
    rf->rf_name = full_name;
    rf->rf_type = RF_COMPLEX;
    rf->rf_interval = (interval != 0) ? interval : plugin_get_interval();

    /* Set user data */
    if (user_data == NULL) {
        rf->rf_udata.data = NULL;
        rf->rf_udata.free_func = NULL;
    } else {
        rf->rf_udata = *user_data;
    }

    rf->rf_ctx = plugin_get_ctx();
    rf->rf_ctx.interval = rf->rf_interval;

    rf->stats = stats;
    rf->stats->plugin = name;

    int status = plugin_insert_read(rf);
    if (status != 0) {
        free_userdata(&rf->rf_udata);
        free(rf->rf_name);
        free(rf);
    }

    pthread_mutex_lock(&read_stats_lock);
    stats->next = read_stats;
    read_stats = stats;
    pthread_mutex_unlock(&read_stats_lock);

    return status;
}

int plugin_unregister_read(const char *name)
{
    if (name == NULL)
        return -ENOENT;

    pthread_mutex_lock(&read_lock);

    if (read_list == NULL) {
        pthread_mutex_unlock(&read_lock);
        return -ENOENT;
    }

    llentry_t *le = llist_search(read_list, name);
    if (le == NULL) {
        pthread_mutex_unlock(&read_lock);
        WARNING("plugin_unregister_read: No such read function: %s", name);
        return -ENOENT;
    }

    llist_remove(read_list, le);

    read_func_t *rf = le->value;
    assert(rf != NULL);
    rf->rf_type = RF_REMOVE;

    pthread_mutex_unlock(&read_lock);

    llentry_destroy(le);

    DEBUG("plugin_unregister_read: Marked '%s' for removal.", name);

    return 0;
}

/* Read function called when the '-T' command line argument is given. */
int plugin_read_all_once(void)
{
    if (read_heap == NULL) {
        NOTICE("No read-functions are registered.");
        return 0;
    }

    int return_status = 0;
    while (true) {
        read_func_t *rf = c_heap_get_root(read_heap);
        if (rf == NULL)
            break;

        plugin_ctx_t old_ctx = plugin_set_ctx(rf->rf_ctx);

        int status = 0;
        if (rf->rf_type == RF_SIMPLE) {
            plugin_init_cb callback = rf->rf_init_cb;
            status = (*callback)();
        } else {
            plugin_read_cb callback = rf->rf_read_cb;
            status = (*callback)(&rf->rf_udata);
        }

        plugin_set_ctx(old_ctx);

        if (status != 0) {
            NOTICE("read-function of plugin '%s' failed.", rf->rf_name);
            return_status = -1;
        }

        free(rf->rf_name);
        destroy_callback((void *)rf);
    }

    return return_status;
}

strlist_t *plugin_get_readers(void)
{
    pthread_mutex_lock(&read_lock);
    strlist_t *sl = list_callbacks(&read_list);
    pthread_mutex_unlock(&read_lock);
    return sl;
}

int plugin_init_read(void)
{
    max_read_interval = global_option_get_time("max-read-interval", DEFAULT_MAX_READ_INTERVAL);

    /* Start read-threads */
    if (read_heap != NULL) {
        const char *rt;
        int num;

        rt = global_option_get("read-threads");
        num = atoi(rt);
        if (num != -1)
            start_read_threads((num > 0) ? ((size_t)num) : 5);
    }

    return 0;
}

void plugin_read_stats(metric_family_t *fams)
{
    pthread_mutex_lock(&read_stats_lock);

    read_stats_t *stats = read_stats;
    while(stats != NULL) {
        unsigned long long read_time = atomic_load(&stats->read_time);
        unsigned long long read_calls = atomic_load(&stats->read_calls);
        unsigned long long read_calls_failures = atomic_load(&stats->read_calls_failures);
        unsigned long long read_cpu_user = atomic_load(&stats->read_cpu_user);
        unsigned long long read_cpu_sys = atomic_load(&stats->read_cpu_sys);

        metric_family_append(&fams[FAM_NCOLLECTD_PLUGIN_READ_TIME_SECONDS],
                             VALUE_COUNTER_FLOAT64(CDTIME_T_TO_DOUBLE((cdtime_t)read_time)), NULL,
                             &(label_pair_const_t){.name="plugin", .value=stats->plugin}, NULL);
        metric_family_append(&fams[FAM_NCOLLECTD_PLUGIN_READ_CALLS],
                             VALUE_COUNTER(read_calls), NULL,
                             &(label_pair_const_t){.name="plugin", .value=stats->plugin}, NULL);
        metric_family_append(&fams[FAM_NCOLLECTD_PLUGIN_READ_FAILURES],
                             VALUE_COUNTER(read_calls_failures), NULL,
                             &(label_pair_const_t){.name="plugin", .value=stats->plugin}, NULL);
        metric_family_append(&fams[FAM_NCOLLECTD_PLUGIN_READ_CPU_USER],
                             VALUE_COUNTER_FLOAT64(CDTIME_T_TO_DOUBLE(read_cpu_user)), NULL,
                             &(label_pair_const_t){.name="plugin", .value=stats->plugin}, NULL);
        metric_family_append(&fams[FAM_NCOLLECTD_PLUGIN_READ_CPU_SYSTEM],
                             VALUE_COUNTER_FLOAT64(CDTIME_T_TO_DOUBLE(read_cpu_sys)), NULL,
                             &(label_pair_const_t){.name="plugin", .value=stats->plugin}, NULL);

        stats = stats->next;
    }

    pthread_mutex_unlock(&read_stats_lock);
}
