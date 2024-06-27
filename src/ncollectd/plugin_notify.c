// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2005-2014 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>
// SPDX-FileContributor: Manoj Srivastava <srivasta at google.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "globals.h"
#include "configfile.h"
#include "plugin_internal.h"
#include "filter.h"
#include "libutils/common.h"
#include "libutils/time.h"
#include "libutils/complain.h"
#include "queue.h"

#include <stdatomic.h>

typedef struct {
    queue_elem_t super;
    notification_t *n;
} notify_queue_elem_t ;

struct notify_queue_stats_s;
typedef struct notify_queue_stats_s notify_queue_stats_t;
struct notify_queue_stats_s {
    char *plugin;
    atomic_ullong notify_time;
    atomic_ullong notify_calls;
    atomic_ullong notify_calls_failures;
    notify_queue_stats_t *next;
};

typedef struct {
    queue_thread_t super;
    notify_queue_stats_t *stats;
    plugin_notification_cb notify_cb;
    user_data_t ud;
} notify_queue_thread_t;

static queue_t notify_queue = {
    .kind = "notification",
    .complaint = C_COMPLAIN_INIT_STATIC,
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .cond = PTHREAD_COND_INITIALIZER,
    .tail = NULL,
    .threads = NULL,
};

static pthread_mutex_t notify_queue_stats_lock = PTHREAD_MUTEX_INITIALIZER;
static notify_queue_stats_t *notify_queue_stats;

static atomic_ullong notifications_dispatched;

static void notify_queue_elem_free(void *arg)
{
    notify_queue_elem_t *elem = arg;

    if (elem == NULL)
        return;

    notification_free(elem->n);

    free(elem);
}

static void notify_queue_thread_free(void *arg)
{
    notify_queue_thread_t *notifier = arg;

    if (notifier == NULL)
        return;

    pthread_mutex_lock(&notify_queue_stats_lock);
    if (notifier->stats != NULL) {
        notify_queue_stats_t *prev = NULL;
        notify_queue_stats_t *stats = notify_queue_stats;
        while (stats != NULL) {
            if (notifier->stats == stats) {
                notify_queue_stats_t *next = stats->next;
                free(stats);
                notifier->stats = NULL;
                if (prev == NULL)
                    notify_queue_stats = next;
                else
                    prev->next = next;
                break;
            }
            prev = stats;
            stats = stats->next;
        }
    }
    pthread_mutex_unlock(&notify_queue_stats_lock);

    free(notifier);
}

static void *plugin_notify_thread(void *args)
{
    notify_queue_thread_t *notifier = args;

    DEBUG("start", notifier->name);

    while (notifier->super.loop) {
        notify_queue_elem_t *elem = (notify_queue_elem_t *)
                                   queue_dequeue(&notify_queue, (queue_thread_t *)notifier, 0);
        if (elem == NULL)
            continue;

        DEBUG("%s: de-queue %p (remaining queue length: %ld)",
              notifier->super.name, elem, notifier->super.queue_length);

        /* Should elem be written to all plugins or this plugin in particular? */
        if ((elem->super.plugin == NULL) ||
            (strcasecmp(elem->super.plugin, notifier->super.name) == 0)) {

            plugin_ctx_t ctx = elem->super.ctx;
            ctx.name = (char *)notifier->super.name;
            plugin_set_ctx(ctx);

            cdtime_t start = cdtime();
            int status = notifier->notify_cb(elem->n, &notifier->ud);
            cdtime_t end = cdtime();

            cdtime_t diff = end - start;

            atomic_fetch_add(&notifier->stats->notify_time, diff);
            atomic_fetch_add(&notifier->stats->notify_calls, 1);
            if (status != 0)
                atomic_fetch_add(&notifier->stats->notify_calls_failures, 1);
        }

        /* Free the element if it is not referenced by another queue or thread. */
        queue_ref_single(&notify_queue, (queue_elem_t *)elem, -1);
    }

    DEBUG("%s: teardown", notifier->super.name);

    /* Cleanup before leaving */
    free_userdata(&notifier->ud);
    notifier->ud.data = NULL;
    notifier->ud.free_func = NULL;

    pthread_exit(NULL);

    return NULL;
}

strlist_t *plugin_get_notificators(void)
{
    return queue_get_threads(&notify_queue);
}

int plugin_unregister_notification(const char *name)
{
    return queue_thread_stop(&notify_queue, name);
}

int plugin_notify(const char *plugin, const notification_t *notify)
{
    if (notify == NULL)
        return EINVAL;

    notification_t *n = notification_clone(notify);
    if (n == NULL) {
        ERROR("notification_clone failed.");
        return -1;
    }

    if (n->time == 0)
        n->time = cdtime();

    label_set_add_set(&n->label, false, labels_g);

    notify_queue_elem_t *elem = calloc(1, sizeof(*elem));
    if (elem == NULL) {
        notification_free(n);
        return ENOMEM;
    }

    atomic_fetch_add(&notifications_dispatched, 1);

    elem->n = n;

    return queue_enqueue(&notify_queue, plugin, (queue_elem_t *)elem);
}

int plugin_dispatch_notification(const notification_t *notif)
{
    return plugin_notify(NULL, notif);
}

int plugin_register_notification(const char *group, const char *name,
                                 plugin_notification_cb callback, user_data_t const *ud)
{
    if (group == NULL) {
        ERROR("group name is NULL.");
        free_userdata(ud);
        return EINVAL;
    }

    char *full_name = plugin_full_name(group, name);
    if (full_name == NULL) {
        free_userdata(ud);
        return ENOMEM;
    }

    notify_queue_stats_t *notifier_stats = calloc(1, sizeof(*notifier_stats));
    if (notifier_stats == NULL) {
        ERROR("calloc failed.");
        free_userdata(ud);
        free(full_name);
        return ENOMEM;
    }

    notify_queue_thread_t *notifier = calloc(1, sizeof(*notifier));
    if (notifier == NULL) {
        ERROR("calloc failed.");
        free_userdata(ud);
        free(full_name);
        free(notifier_stats);
        return ENOMEM;
    }

    notifier_stats->plugin = full_name;
    notifier->stats = notifier_stats;

    notifier->notify_cb = callback;

    if (ud == NULL) {
        notifier->ud = (user_data_t){ .data = NULL, .free_func = NULL };
    } else {
        notifier->ud = *ud;
    }

    int status = queue_thread_start(&notify_queue, (queue_thread_t *)notifier,
                                    full_name, plugin_notify_thread, (void *)notifier);
    if (status != 0) {
        free_userdata(ud);
        free(full_name);
        free(notifier_stats);
        free(notifier);
        return status;
    }

    pthread_mutex_lock(&notify_queue_stats_lock);
    notifier_stats->next = notify_queue_stats;
    notify_queue_stats = notifier_stats;
    pthread_mutex_unlock(&notify_queue_stats_lock);

    return 0;
}

int plugin_init_notify(void)
{
    notify_queue.limit_high = global_option_get_long("notify-queue-limit-high", 0);
    if (notify_queue.limit_high < 0) {
        ERROR("notify-queue-limit-high must be positive or zero.");
        notify_queue.limit_high = 0;
    }

    notify_queue.limit_low = global_option_get_long("notify-queue-limit-low",
                                                    notify_queue.limit_high / 2);
    if (notify_queue.limit_low < 0) {
        ERROR("notify-queue-limit-low must be positive or zero.");
        notify_queue.limit_low = notify_queue.limit_high / 2;
    } else if (notify_queue.limit_low > notify_queue.limit_high) {
        ERROR("notify-queue-limit-low must not be larger than notify-queue-limit-high.");
        notify_queue.limit_low = notify_queue.limit_high;
    }

    notify_queue.free_elem_cb = notify_queue_elem_free;
    notify_queue.free_thread_cb = notify_queue_thread_free;

    return 0;
}

void plugin_shutdown_notify(void)
{
    plugin_unregister_notification(NULL);
}

long plugin_notify_queue_length(void)
{
    return queue_length(&notify_queue);
}

void plugin_notify_stats(metric_family_t *fams)
{
    long length = queue_length(&notify_queue);
    metric_family_append(&fams[FAM_NCOLLECTD_NOTIFY_QUEUE_LENGTH],
                         VALUE_GAUGE(length), NULL, NULL);
    uint64_t dropped = queue_dropped(&notify_queue);
    metric_family_append(&fams[FAM_NCOLLECTD_NOTIFY_QUEUE_DROPPED],
                         VALUE_COUNTER(dropped), NULL, NULL);

    unsigned long long dispatched = atomic_load(&notifications_dispatched);
    metric_family_append(&fams[FAM_NCOLLECTD_NOTIFICATIONS_DISPACHED],
                         VALUE_COUNTER(dispatched), NULL, NULL);

    pthread_mutex_lock(&notify_queue_stats_lock);

    notify_queue_stats_t *stats = notify_queue_stats;
    while(stats != NULL) {
        unsigned long long notify_time = atomic_load(&stats->notify_time);
        unsigned long long notify_calls = atomic_load(&stats->notify_calls);
        unsigned long long notify_calls_failures = atomic_load(&stats->notify_calls_failures);

        metric_family_append(&fams[FAM_NCOLLECTD_PLUGIN_NOTIFY_TIME_SECONDS],
                             VALUE_COUNTER_FLOAT64(CDTIME_T_TO_DOUBLE((cdtime_t)notify_time)), NULL,
                             &(label_pair_const_t){.name="plugin", .value=stats->plugin}, NULL);
        metric_family_append(&fams[FAM_NCOLLECTD_PLUGIN_NOTIFY_CALLS],
                             VALUE_COUNTER(notify_calls), NULL,
                             &(label_pair_const_t){.name="plugin", .value=stats->plugin}, NULL);
        metric_family_append(&fams[FAM_NCOLLECTD_PLUGIN_NOTIFY_FAILURES],
                             VALUE_COUNTER(notify_calls_failures), NULL,
                             &(label_pair_const_t){.name="plugin", .value=stats->plugin}, NULL);

        stats = stats->next;
    }

    pthread_mutex_unlock(&notify_queue_stats_lock);
}
