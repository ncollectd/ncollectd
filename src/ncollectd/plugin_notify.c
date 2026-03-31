// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2005-2014 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>
// SPDX-FileContributor: Manoj Srivastava <srivasta at google.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#define _GNU_SOURCE

#include "ncollectd.h"
#include "globals.h"
#include "configfile.h"
#include "plugin_internal.h"
#include "filter.h"
#include "libutils/common.h"
#include "libutils/time.h"
#include "libutils/complain.h"
#include "queue.h"
#include "journal.h"

#include <stdatomic.h>
#include <sys/resource.h>

struct notify_stats_s;
typedef struct notify_stats_s notify_stats_t;
struct notify_stats_s {
    const char *plugin;
    atomic_ullong notify_time;
    atomic_ullong notify_calls;
    atomic_ullong notify_calls_failures;
    atomic_ullong notify_cpu_user;
    atomic_ullong notify_cpu_sys;
    notify_stats_t *next;
};

static pthread_mutex_t notify_stats_lock = PTHREAD_MUTEX_INITIALIZER;
static notify_stats_t *notify_stats;

typedef struct {
    queue_elem_t super;
    notification_t *n;
} notify_queue_elem_t ;

typedef struct {
    queue_thread_t super;
    notify_stats_t *stats;
    plugin_notification_cb notify_cb;
    user_data_t ud;
} notify_queue_thread_t;

static queue_t *notify_queue;

typedef struct {
    char *plugin;
    notification_t *n;
} notify_journal_elem_t;

typedef struct {
    journal_thread_t super;
    notify_stats_t *stats;
    plugin_notification_cb notify_cb;
    journal_ctx_t *journal;
    user_data_t ud;
} notify_journal_thread_t;

static journal_t *notify_journal;
static journal_ctx_t *notify_journal_writer;

static atomic_ullong notifications_dispatched;

enum {
    NOTIFY_NOTIFICATION_ID = 0x10,
    NOTIFY_PLUGIN_NAME_ID  = 0x20
};

static int plugin_notify_pack(buf_t *buf, const notification_t *n, const char *plugin_name)
{
    size_t begin = 0;
    int status = pack_block_begin(buf, &begin);

    status |= pack_id(buf, NOTIFY_NOTIFICATION_ID);
    status |= notification_pack(buf, n);

    if (plugin_name != NULL)
        status |= pack_string(buf, NOTIFY_PLUGIN_NAME_ID, plugin_name);

    status |= pack_block_end(buf, begin);
    return status;
}

static int plugin_notify_unpack(rbuf_t *rbuf, notify_journal_elem_t *elem)
{
    rbuf_t srbuf = {0};
    int status = unpack_block(rbuf, &srbuf);
    if (status != 0) {
        return -1;
    }

    while (true) {
        uint8_t id = 0;

        status |= unpack_id(&srbuf, &id);

        switch (id & 0xf0) {
        case NOTIFY_NOTIFICATION_ID:
            elem->n = notification_unpack(&srbuf);
            break;
        case NOTIFY_PLUGIN_NAME_ID:
            status |= unpack_string(&srbuf, id, &elem->plugin);
            break;
        }

        if (status != 0)
            break;

        if (rbuf_remain(&srbuf) == 0)
            break;
    }

    if (status != 0) {
        free(elem->plugin);
        if (elem->n != NULL)
            notification_free(elem->n);
        return -1;
    }

    unpack_avance_block(rbuf, &srbuf);

    return 0;
}

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

    pthread_mutex_lock(&notify_stats_lock);
    if (notifier->stats != NULL) {
        notify_stats_t *prev = NULL;
        notify_stats_t *stats = notify_stats;
        while (stats != NULL) {
            if (notifier->stats == stats) {
                notify_stats_t *next = stats->next;
                free(stats);
                notifier->stats = NULL;
                if (prev == NULL)
                    notify_stats = next;
                else
                    prev->next = next;
                break;
            }
            prev = stats;
            stats = stats->next;
        }
    }
    pthread_mutex_unlock(&notify_stats_lock);

    free(notifier);
}


static void notify_journal_thread_free(void *arg)
{
    notify_journal_thread_t *notifier = arg;

    if (notifier == NULL)
        return;

    pthread_mutex_lock(&notify_stats_lock);
    if (notifier->stats != NULL) {
        notify_stats_t *prev = NULL;
        notify_stats_t *stats = notify_stats;
        while (stats != NULL) {
            if (notifier->stats == stats) {
                notify_stats_t *next = stats->next;
                free(stats);
                notifier->stats = NULL;
                if (prev == NULL)
                    notify_stats = next;
                else
                    prev->next = next;
                break;
            }
            prev = stats;
            stats = stats->next;
        }
    }
    pthread_mutex_unlock(&notify_stats_lock);

    if (notifier->journal != NULL)
        journal_ctx_close(notifier->journal);

    free(notifier);
}

static int plugin_notify_notification(notify_stats_t *stats,
                                      plugin_notification_cb notify_cb, user_data_t *ud,
                                      notification_t *n)
{
#ifdef HAVE_RUSAGE_THREAD
    struct rusage usage_start = {0};
    getrusage(RUSAGE_THREAD, &usage_start);
#endif

    cdtime_t start = cdtime();
    int status = notify_cb(n, ud);
    cdtime_t end = cdtime();

#ifdef HAVE_RUSAGE_THREAD
    struct rusage usage_finish = {0};
    getrusage(RUSAGE_THREAD, &usage_finish);
    cdtime_t cpu_user_time = TIMEVAL_TO_CDTIME_T(&usage_finish.ru_utime) -
                             TIMEVAL_TO_CDTIME_T(&usage_start.ru_utime);
    cdtime_t cpu_sys_time = TIMEVAL_TO_CDTIME_T(&usage_finish.ru_stime) -
                            TIMEVAL_TO_CDTIME_T(&usage_start.ru_stime);

    atomic_fetch_add(&stats->notify_cpu_user, cpu_user_time);
    atomic_fetch_add(&stats->notify_cpu_sys, cpu_sys_time);
#endif

    cdtime_t diff = end - start;

    atomic_fetch_add(&stats->notify_time, diff);
    atomic_fetch_add(&stats->notify_calls, 1);
    if (status != 0)
        atomic_fetch_add(&stats->notify_calls_failures, 1);

    return 0;
}

static void *plugin_notify_queue_thread(void *args)
{
    notify_queue_thread_t *notifier = args;

    DEBUG("start", notifier->name);

    while (notifier->super.loop) {
        notify_queue_elem_t *elem = (notify_queue_elem_t *)
                                   queue_dequeue(notify_queue, (queue_thread_t *)notifier, 0);
        if (elem != NULL) {
            DEBUG("%s: de-queue %p (remaining queue length: %ld)",
                  notifier->super.name, elem, notifier->super.queue_length);

            /* Should elem be written to all plugins or this plugin in particular? */
            if ((elem->super.plugin == NULL) ||
                (strcasecmp(elem->super.plugin, notifier->super.name) == 0)) {

                plugin_ctx_t ctx = elem->super.ctx;
                ctx.name = (char *)notifier->super.name;
                plugin_set_ctx(ctx);

                plugin_notify_notification(notifier->stats, notifier->notify_cb,
                                           &notifier->ud, elem->n);
            }

            /* Free the element if it is not referenced by another queue or thread. */
            queue_ref_single(notify_queue, (queue_elem_t *)elem, -1);
        }
    }

    DEBUG("%s: teardown", notifier->super.name);

    /* Cleanup before leaving */
    free_userdata(&notifier->ud);
    notifier->ud.data = NULL;
    notifier->ud.free_func = NULL;

    pthread_exit(NULL);

    return NULL;
}

static size_t plugin_notify_journal_read(notify_journal_thread_t *notifier)
{
    journal_id_t begin = {0};
    journal_id_t end = {0};

    size_t reads = 0;

    int count = journal_ctx_read_interval(notifier->journal, &begin, &end);
    if (count > 0) {
        for (int i = 0; i < count; i++, JOURNAL_ID_ADVANCE(&begin)) {
            end = begin;
            journal_message_t m;
            if (journal_ctx_read_message(notifier->journal, &begin, &m) == 0) {
                notify_journal_elem_t elem = {0};
                rbuf_t rbuf = {0};
                rbuf_init(&rbuf, m.mess, m.mess_len);
                int status = plugin_notify_unpack(&rbuf, &elem);
                if (status == 0) {
                    if ((elem.plugin == NULL) ||
                        (strcasecmp(elem.plugin, notifier->super.name) == 0))

                    plugin_notify_notification(notifier->stats, notifier->notify_cb,
                                               &notifier->ud, elem.n);

                    free(elem.plugin);
                    notification_free(elem.n);
                }
                reads++;
            }
        }

        journal_ctx_read_checkpoint(notifier->journal, &end);
    }

    return reads;
}

static void *plugin_notify_journal_thread(void *args)
{
    notify_journal_thread_t *notifier = args;

    DEBUG("start", notifier->name);

    plugin_ctx_t ctx = {
        .name = (char *)notifier->super.name,
        .interval = 0,
        .normalize_interval = 0,
    };
    plugin_set_ctx(ctx);

    while (notifier->super.loop) {
        pthread_mutex_lock(&notify_journal->lock);
        pthread_cond_wait(&notify_journal->cond, &notify_journal->lock);
        pthread_mutex_unlock(&notify_journal->lock);

        int no_data = 0;
        do {
            int reads = plugin_notify_journal_read(notifier);
            no_data = reads == 0 ? no_data + 1 : 0;
        } while (no_data <= 1);
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
    if (notify_queue != NULL)
        return queue_get_threads(notify_queue);
    if (notify_journal != NULL)
        return journal_get_threads(notify_journal);
    return NULL;
}

int plugin_unregister_notification(const char *name)
{
    if (notify_queue != NULL)
        return queue_thread_stop(notify_queue, name);
    if (notify_journal != NULL)
        return journal_thread_stop(notify_journal, name);
    return 0;
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

    if (notify_queue != NULL) {
        notify_queue_elem_t *elem = calloc(1, sizeof(*elem));
        if (elem == NULL) {
            notification_free(n);
            return ENOMEM;
        }

        atomic_fetch_add(&notifications_dispatched, 1);

        elem->n = n;

        return queue_enqueue(notify_queue, plugin, (queue_elem_t *)elem);
    } else if (notify_journal != NULL) {
        if (notify_journal_writer != NULL) {
            buf_t buf = BUF_CREATE;
            buf_resize(&buf, 4096);
            int status = plugin_notify_pack(&buf, n, plugin);
            if (status != 0) {
                ERROR("cannot serialize notification '%s'", notify->name);
                buf_destroy(&buf);
                return status;
            }

            if(journal_ctx_write(notify_journal_writer, buf.ptr, buf_len(&buf)) != 0) {
                atomic_fetch_add(&notifications_dispatched, 1);
            }

            pthread_mutex_lock(&notify_journal->lock);
            pthread_cond_broadcast(&notify_journal->cond);
            pthread_mutex_unlock(&notify_journal->lock);

            buf_destroy(&buf);
            notification_free(n);
        }
    }

    return 0;
}

int plugin_dispatch_notification(const notification_t *notif)
{
    return plugin_notify(NULL, notif);
}

int plugin_register_notification_queue(char *full_name, notify_stats_t *notifier_stats,
                                       plugin_notification_cb callback, user_data_t const *ud)
{
    notify_queue_thread_t *notifier = calloc(1, sizeof(*notifier));
    if (notifier == NULL) {
        ERROR("calloc failed.");
        return -1;
    }

    notifier_stats->plugin = full_name;
    notifier->stats = notifier_stats;

    notifier->notify_cb = callback;

    if (ud == NULL) {
        notifier->ud = (user_data_t){ .data = NULL, .free_func = NULL };
    } else {
        notifier->ud = *ud;
    }

    int status = queue_thread_start(notify_queue, (queue_thread_t *)notifier,
                                    full_name, plugin_notify_queue_thread, (void *)notifier);
    if (status != 0) {
        free(notifier);
        return -1;
    }

    return 0;
}

int plugin_register_notification_journal(char *full_name, notify_stats_t *notifier_stats,
                                         plugin_notification_cb callback, user_data_t const *ud)
{
    notify_journal_thread_t *notifier = calloc(1, sizeof(*notifier));
    if (notifier == NULL) {
        ERROR("calloc failed.");
        return -1;
    }

    notifier_stats->plugin = full_name;
    notifier->stats = notifier_stats;

    notifier->notify_cb = callback;

    if (ud == NULL) {
        notifier->ud = (user_data_t){ .data = NULL, .free_func = NULL };
    } else {
        notifier->ud = *ud;
    }

    journal_ctx_add_subscriber(notify_journal_writer, full_name, JOURNAL_END);

    notifier->journal = journal_get_reader(notify_journal, full_name);
    if (notifier->journal == NULL) {
        ERROR("cannot create new journal context.");
        free(notifier);
        return -1;
    }

    int status = journal_thread_start(notify_journal, (journal_thread_t *)notifier,
                                      full_name, plugin_notify_journal_thread, (void *)notifier);
    if (status != 0) {
        journal_ctx_close(notifier->journal);
        free(notifier);
        return -1;
    }

    return 0;
}

int plugin_register_notification(const char *group, const char *name,
                                 plugin_notification_cb callback, user_data_t const *ud)
{
    if (group == NULL) {
        ERROR("group name is NULL.");
        free_userdata(ud);
        return -1;
    }

    char *full_name = plugin_full_name(group, name);
    if (full_name == NULL) {
        free_userdata(ud);
        return -1;
    }

    notify_stats_t *notifier_stats = calloc(1, sizeof(*notifier_stats));
    if (notifier_stats == NULL) {
        ERROR("calloc failed.");
        free_userdata(ud);
        free(full_name);
        return -1;
    }

    int status = 0;

    if (notify_queue != NULL) {
        status = plugin_register_notification_queue(full_name, notifier_stats, callback, ud);
    } else if (notify_journal != NULL) {
        status = plugin_register_notification_journal(full_name, notifier_stats, callback, ud);
    }

    if (status != 0) {
        free_userdata(ud);
        free(full_name);
        free(notifier_stats);
        return -1;
    }

    pthread_mutex_lock(&notify_stats_lock);
    notifier_stats->next = notify_stats;
    notify_stats = notifier_stats;
    pthread_mutex_unlock(&notify_stats_lock);

    return 0;
}

int plugin_config_notify(void)
{
    cf_queue_t *notification_queue = global_option_get_notification_queue();

    if (notification_queue->type == CF_QUEUE_MEMORY) {
        notify_queue = queue_new("notifications");
        if (notify_queue == NULL) {
            ERROR("Cannot alloc queue.");
            return -1;
        }
        notify_queue->limit_high = notification_queue->memory.limit_high;
        notify_queue->limit_low = notification_queue->memory.limit_low;
        notify_queue->free_elem_cb = notify_queue_elem_free;
        notify_queue->free_thread_cb = notify_queue_thread_free;
    } else {
        notify_journal = journal_new(notification_queue->journal.path);
        if (notify_journal  == NULL) {

            return -1;
        }

        journal_config_checksum(notify_journal, notification_queue->journal.checksum);
        journal_config_compress(notify_journal, notification_queue->journal.compress);
        journal_config_retention_size(notify_journal, notification_queue->journal.retention_size);
        journal_config_retention_time(notify_journal, notification_queue->journal.retention_time);
        journal_config_segment_size(notify_journal, notification_queue->journal.segment_size);

        notify_journal->free_thread_cb = notify_journal_thread_free;

        journal_init(notify_journal);

        notify_journal_writer = journal_get_writer(notify_journal);
        if (notify_journal_writer == NULL) {
            ERROR("cannot open journal '%s' for writting.", notification_queue->journal.path);
            return -1;
        }

        buf_t buf = BUF_CREATE;
        buf_resize(&buf, 4096);
        int status = plugin_notify_pack(&buf, NULL, NULL);
        if (status != 0) {
            ERROR("cannot serialize NULL notification.");
            buf_destroy(&buf);
            return -1;
        }

        journal_ctx_write(notify_journal_writer, buf.ptr, buf_len(&buf));

        buf_destroy(&buf);
    }

    return 0;
}

int plugin_init_notify(void)
{
    return 0;
}

void plugin_shutdown_notify(void)
{
    plugin_unregister_notification(NULL);

    if (notify_journal != NULL) {
        if (notify_journal_writer != NULL) {
            journal_ctx_close(notify_journal_writer);
            notify_journal_writer = NULL;
        }

        journal_close(notify_journal);
        notify_journal = NULL;
    }
}

long plugin_notify_queue_length(void)
{
    if (notify_queue != NULL)
        return queue_length(notify_queue);
    else
        return 0;
}

void plugin_notify_stats(metric_family_t *fams)
{
    if (notify_queue != NULL) {
        long length = queue_length(notify_queue);
        metric_family_append(&fams[FAM_NCOLLECTD_NOTIFY_QUEUE_LENGTH],
                             VALUE_GAUGE(length), NULL, NULL);
        uint64_t dropped = queue_dropped(notify_queue);
        metric_family_append(&fams[FAM_NCOLLECTD_NOTIFY_QUEUE_DROPPED],
                             VALUE_COUNTER(dropped), NULL, NULL);
    }

    unsigned long long dispatched = atomic_load(&notifications_dispatched);
    metric_family_append(&fams[FAM_NCOLLECTD_NOTIFICATIONS_DISPACHED],
                         VALUE_COUNTER(dispatched), NULL, NULL);

    pthread_mutex_lock(&notify_stats_lock);

    notify_stats_t *stats = notify_stats;
    while(stats != NULL) {
        unsigned long long notify_time = atomic_load(&stats->notify_time);
        unsigned long long notify_calls = atomic_load(&stats->notify_calls);
        unsigned long long notify_calls_failures = atomic_load(&stats->notify_calls_failures);
        unsigned long long notify_cpu_user = atomic_load(&stats->notify_cpu_user);
        unsigned long long notify_cpu_sys = atomic_load(&stats->notify_cpu_sys);

        metric_family_append(&fams[FAM_NCOLLECTD_PLUGIN_NOTIFY_TIME_SECONDS],
                             VALUE_COUNTER_FLOAT64(CDTIME_T_TO_DOUBLE((cdtime_t)notify_time)), NULL,
                             &LABEL_PAIR_CONST("plugin", stats->plugin), NULL);
        metric_family_append(&fams[FAM_NCOLLECTD_PLUGIN_NOTIFY_CALLS],
                             VALUE_COUNTER(notify_calls), NULL,
                             &LABEL_PAIR_CONST("plugin", stats->plugin), NULL);
        metric_family_append(&fams[FAM_NCOLLECTD_PLUGIN_NOTIFY_FAILURES],
                             VALUE_COUNTER(notify_calls_failures), NULL,
                             &LABEL_PAIR_CONST("plugin", stats->plugin), NULL);
        metric_family_append(&fams[FAM_NCOLLECTD_PLUGIN_NOTIFY_CPU_USER],
                             VALUE_COUNTER_FLOAT64(CDTIME_T_TO_DOUBLE(notify_cpu_user)), NULL,
                             &LABEL_PAIR_CONST("plugin", stats->plugin), NULL);
        metric_family_append(&fams[FAM_NCOLLECTD_PLUGIN_NOTIFY_CPU_SYSTEM],
                             VALUE_COUNTER_FLOAT64(CDTIME_T_TO_DOUBLE(notify_cpu_sys)), NULL,
                             &LABEL_PAIR_CONST("plugin", stats->plugin), NULL);

        stats = stats->next;
    }

    pthread_mutex_unlock(&notify_stats_lock);
}
