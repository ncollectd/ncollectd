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
#include "libutils/avltree.h"
#include "libutils/common.h"
#include "libutils/time.h"
#include "libutils/complain.h"
#include "libmdb/mdb.h"
#include "queue.h"

#include <stdatomic.h>

typedef struct {
    queue_elem_t super;
    metric_family_t *fam;
} write_queue_elem_t ;

struct write_queue_stats_s;
typedef struct write_queue_stats_s write_queue_stats_t;
struct write_queue_stats_s {
    char *plugin;
    atomic_ullong write_time;
    atomic_ullong write_calls;
    atomic_ullong write_calls_failures;
    write_queue_stats_t *next;
};

typedef struct {
    queue_thread_t super;
    write_queue_stats_t *stats;
    plugin_write_cb write_cb;
    plugin_flush_cb flush_cb;
    cdtime_t flush_interval;
    cdtime_t flush_timeout;
    user_data_t ud;
} write_queue_thread_t;

static queue_t write_queue = {
    .kind = "write",
    .complaint = C_COMPLAIN_INIT_STATIC,
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .cond = PTHREAD_COND_INITIALIZER,
    .tail = NULL,
    .threads = NULL,
};

static pthread_mutex_t write_queue_stats_lock = PTHREAD_MUTEX_INITIALIZER;
static write_queue_stats_t *write_queue_stats;

static atomic_ullong metrics_dispatched;

static filter_t *pre_cache_filter;
static filter_t *post_cache_filter;

extern mdb_t *mdb;

static void write_queue_elem_free(void *arg)
{
    write_queue_elem_t *elem = arg;

    if (elem == NULL)
        return;

    metric_family_free(elem->fam);

    free(elem);
}

static void write_queue_thread_free(void *arg)
{
    write_queue_thread_t *writer = arg;

    if (writer == NULL)
        return;

    pthread_mutex_lock(&write_queue_stats_lock);
    if (writer->stats != NULL) {

        write_queue_stats_t *prev = NULL;
        write_queue_stats_t *stats = write_queue_stats;
        while (stats != NULL) {
            if (writer->stats == stats) {
                write_queue_stats_t *next = stats->next;
                free(stats);
                writer->stats = NULL;
                if (prev == NULL)
                    write_queue_stats = next;
                else
                    prev->next = next;
                break;
            }
            prev = stats;
            stats = stats->next;
        }
    }
    pthread_mutex_unlock(&write_queue_stats_lock);

    free(writer);
}

static void *plugin_write_thread(void *args)
{
    write_queue_thread_t *writer = args;

    DEBUG("start", writer->name);

    cdtime_t next_flush = 0;
    if (writer->flush_cb != NULL)
        next_flush = cdtime() + writer->flush_interval;

    while (writer->super.loop) {
        write_queue_elem_t *elem = (write_queue_elem_t *)queue_dequeue(&write_queue,
                                                                       (queue_thread_t*)writer,
                                                                       next_flush);
        if (elem != NULL) {
            DEBUG("%s: de-queue %p (remaining queue length: %ld)",
                  writer->super.name, elem, writer->super.queue_length);

            /* Should elem be written to all plugins or this plugin in particular? */
            if ((elem->super.plugin == NULL) ||
                (strcasecmp(elem->super.plugin, writer->super.name) == 0)) {
                plugin_ctx_t ctx = elem->super.ctx;
                ctx.name = (char *)writer->super.name;
                plugin_set_ctx(ctx);

                cdtime_t start = cdtime();
                int status = writer->write_cb(elem->fam, &writer->ud);
                cdtime_t end = cdtime();

                cdtime_t diff = end - start;

                atomic_fetch_add(&writer->stats->write_time, diff);
                atomic_fetch_add(&writer->stats->write_calls, 1);
                if (status != 0)
                    atomic_fetch_add(&writer->stats->write_calls_failures, 1);
            }

            /* Free the element if it is not referenced by another queue or thread. */
            queue_ref_single(&write_queue, (queue_elem_t *)elem, -1);
        }

        if (writer->flush_cb != NULL) {
            cdtime_t now = cdtime();
            if (now >= next_flush) {
                writer->flush_cb(writer->flush_timeout, &writer->ud);
                next_flush = now + writer->flush_interval;
            }
        }

    }

    DEBUG("%s: teardown", writer->super.name);

    /* Cleanup before leaving */
    free_userdata(&writer->ud);
    writer->ud.data = NULL;
    writer->ud.free_func = NULL;

    pthread_exit(NULL);

    return NULL;
}

#define METRIC_FAMILY_LIST_STACK_SIZE 256

static int plugin_dispatch_metric_internal_post(metric_family_t *fam)
{
    /* Update the value cache */
    mdb_insert_metric_family(mdb, fam);

    if (post_cache_filter != NULL) {
        metric_family_t *fams[METRIC_FAMILY_LIST_STACK_SIZE];
        metric_family_list_t faml = METRIC_FAMILY_LIST_CREATE();
        if (fam->metric.num > METRIC_FAMILY_LIST_STACK_SIZE) {
            metric_family_list_alloc(&faml, fam->metric.num);
        } else {
            faml = METRIC_FAMILY_LIST_CREATE_STATIC(fams);
        }
        metric_family_list_append(&faml, fam);

        int status = filter_process(post_cache_filter, &faml);
        if (status < 0)
            WARNING("Running the post-cache chain failed with status %d.", status);

        metric_family_list_reset(&faml);
    } else {
        plugin_write(NULL, fam, false);
    }

    return 0;
}

static int plugin_dispatch_metric_internal(metric_family_t *fam)
{
    if (fam == NULL)
        return EINVAL;

    if (pre_cache_filter != NULL) {
        metric_family_t *fams[METRIC_FAMILY_LIST_STACK_SIZE];
        metric_family_list_t faml = METRIC_FAMILY_LIST_CREATE();
        if (fam->metric.num > METRIC_FAMILY_LIST_STACK_SIZE) {
            metric_family_list_alloc(&faml, fam->metric.num);
        } else {
            faml = METRIC_FAMILY_LIST_CREATE_STATIC(fams);
        }
        metric_family_list_append(&faml, fam);

        int status = filter_process(pre_cache_filter, &faml);
        if (status < 0) {
            WARNING("Running the pre-cache chain failed with status %d.", status);
        } else if (status == FILTER_RESULT_STOP) {
            metric_family_list_reset(&faml);
            return 0;
        }

        for (size_t i = 0; i < faml.pos; i++) {
            plugin_dispatch_metric_internal_post(faml.ptr[i]);
            faml.ptr[i] = NULL;
        }

        metric_family_list_reset(&faml);
    } else {
        plugin_dispatch_metric_internal_post(fam);
    }

    return 0;
}

static int plugin_dispatch_metric_internal_filtered(metric_family_t *fam, filter_t *filter)
{
    if (fam == NULL)
        return EINVAL;

    if (filter != NULL) {
        metric_family_t *fams[METRIC_FAMILY_LIST_STACK_SIZE];
        metric_family_list_t faml = METRIC_FAMILY_LIST_CREATE();
        if (fam->metric.num > METRIC_FAMILY_LIST_STACK_SIZE) {
            metric_family_list_alloc(&faml, fam->metric.num);
        } else {
            faml = METRIC_FAMILY_LIST_CREATE_STATIC(fams);
        }
        metric_family_list_append(&faml, fam);

        int status = filter_process(filter, &faml);
        if (status < 0) {
            WARNING("Running the filter chain failed with status %d.", status);
        } else if (status == FILTER_RESULT_STOP) {
            metric_family_list_reset(&faml);
            return 0;
        }

        for (size_t i = 0; i < faml.pos; i++) {
            plugin_dispatch_metric_internal(faml.ptr[i]);
            faml.ptr[i] = NULL;
        }

        metric_family_list_reset(&faml);
    } else {
        plugin_dispatch_metric_internal(fam);
    }

    return 0;
}

int plugin_dispatch_metric_family_array_filtered(metric_family_t *fams, size_t size,
                                                 filter_t *filter, cdtime_t time)
{
    if ((fams == NULL) || (size == 0))
        return EINVAL;

    if (time == 0)
        time = cdtime();
    cdtime_t interval = plugin_get_interval();

    for (size_t i=0; i < size; i++) {
        metric_family_t *fam = &fams[i];

        if (fam->metric.num == 0)
            continue;

        if (fams->name == NULL) {
            metric_family_metric_reset(fam);
            continue;
        }

        metric_list_t ml = fam->metric;
        fam->metric = (metric_list_t){0};
        metric_family_t *fam_copy = metric_family_clone(fam);
        if (fam_copy == NULL) {
            int status = errno;
            ERROR("metric_family_clone failed: %s", STRERROR(status));
            fam->metric = ml;
            return status;
        }

        fam_copy->metric = ml;

        for (size_t j = 0; j < fam_copy->metric.num; j++) {
            if (fam_copy->metric.ptr[j].time == 0)
                fam_copy->metric.ptr[j].time = time;
            if (fam_copy->metric.ptr[j].interval == 0)
                fam_copy->metric.ptr[j].interval = interval;

            label_set_add_set(&fam_copy->metric.ptr[j].label, false, labels_g);
        }

        int status = 0;
        if (filter != NULL)
            status = plugin_dispatch_metric_internal_filtered(fam_copy, filter);
        else
            status = plugin_dispatch_metric_internal(fam_copy);

        if (status != 0) {
            ERROR("plugin_dispatch_metric_internal failed with status %i (%s).",
                   status, STRERROR(status));
        }
    }

    return 0;
}

int plugin_register_write(const char *group, const char *name, plugin_write_cb write_cb,
                          plugin_flush_cb flush_cb, cdtime_t flush_interval, cdtime_t flush_timeout,
                          user_data_t const *ud)
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

    write_queue_stats_t *writer_stats = calloc(1, sizeof(*writer_stats));
    if (writer_stats == NULL) {
        ERROR("calloc failed.");
        free_userdata(ud);
        free(full_name);
        return ENOMEM;
    }

    write_queue_thread_t *writer = calloc(1, sizeof(*writer));
    if (writer == NULL) {
        ERROR("calloc failed.");
        free_userdata(ud);
        free(full_name);
        free(writer_stats);
        return ENOMEM;
    }

    writer_stats->plugin = full_name;
    writer->stats = writer_stats;

    writer->write_cb = write_cb;
    writer->flush_cb = flush_cb;
    if (flush_interval == 0)
        writer->flush_interval = plugin_get_interval();
    else
        writer->flush_interval = flush_interval;
    writer->flush_timeout = flush_timeout;

    if (ud == NULL) {
        writer->ud = (user_data_t){ .data = NULL, .free_func = NULL };
    } else {
        writer->ud = *ud;
    }

    int status = queue_thread_start(&write_queue, (queue_thread_t *)writer,
                                    full_name, plugin_write_thread, (void *)writer);
    if (status != 0) {
        free_userdata(ud);
        free(full_name);
        free(writer);
        free(writer_stats);
        return status;
    }

    pthread_mutex_lock(&write_queue_stats_lock);
    writer_stats->next = write_queue_stats;
    write_queue_stats = writer_stats;
    pthread_mutex_unlock(&write_queue_stats_lock);

    return 0;
}

strlist_t *plugin_get_writers(void)
{
    return queue_get_threads(&write_queue);
}

int plugin_unregister_write(const char *name)
{
    return queue_thread_stop(&write_queue, name);
}

int plugin_write(const char *plugin, metric_family_t *fam, bool clone)
{
    if (fam == NULL)
        return EINVAL;

    metric_family_t *fam_copy = fam;
    if (clone) {
        fam_copy = metric_family_clone(fam);
        if (fam_copy == NULL) {
          int status = errno;
          ERROR("metric_family_clone failed: %s", STRERROR(status));
          return status;
        }
    }

    write_queue_elem_t *elem = calloc(1, sizeof(*elem));
    if (elem == NULL) {
        metric_family_free(fam_copy);
        return ENOMEM;
    }

    elem->fam = fam_copy;

    atomic_fetch_add(&metrics_dispatched, (unsigned long long)fam_copy->metric.num);

    return queue_enqueue(&write_queue, plugin, (queue_elem_t *)elem);
}

int plugin_init_write(void)
{
    char const *filter_name = global_option_get("pre-cache-filter");
    pre_cache_filter = filter_global_get_by_name(filter_name);

    filter_name = global_option_get("post-cache-filter");
    post_cache_filter = filter_global_get_by_name(filter_name);

    write_queue.limit_high = global_option_get_long("write-queue-limit-high", 0);
    if (write_queue.limit_high < 0) {
        ERROR("write-queue-limit-high must be positive or zero.");
        write_queue.limit_high = 0;
    }

    write_queue.limit_low = global_option_get_long("write-queue-limit-low",
                                                    write_queue.limit_high / 2);
    if (write_queue.limit_low < 0) {
        ERROR("write-queue-limit-low must be positive or zero.");
        write_queue.limit_low = write_queue.limit_high / 2;
    } else if (write_queue.limit_low > write_queue.limit_high) {
        ERROR("write-queue-limit-low must not be larger than write-queue-limit-high.");
        write_queue.limit_low = write_queue.limit_high;
    }

    write_queue.free_elem_cb = write_queue_elem_free;
    write_queue.free_thread_cb = write_queue_thread_free;

    return 0;
}

void plugin_shutdown_write(void)
{
    pre_cache_filter = NULL;
    post_cache_filter = NULL;

    filter_global_free();

    plugin_unregister_write(NULL);
}

void plugin_write_stats(metric_family_t *fams)
{
    long length = queue_length(&write_queue);
    metric_family_append(&fams[FAM_NCOLLECTD_WRITE_QUEUE_LENGTH],
                         VALUE_GAUGE(length), NULL, NULL);
    uint64_t dropped = queue_dropped(&write_queue);
    metric_family_append(&fams[FAM_NCOLLECTD_WRITE_QUEUE_DROPPED],
                         VALUE_COUNTER(dropped), NULL, NULL);
    unsigned long long dispached = atomic_load(&metrics_dispatched);
    metric_family_append(&fams[FAM_NCOLLECTD_METRICS_DISPACHED],
                         VALUE_COUNTER(dispached), NULL, NULL);

    pthread_mutex_lock(&write_queue_stats_lock);

    write_queue_stats_t *stats = write_queue_stats;
    while(stats != NULL) {
        unsigned long long write_time = atomic_load(&stats->write_time);
        unsigned long long write_calls = atomic_load(&stats->write_calls);
        unsigned long long write_calls_failures = atomic_load(&stats->write_calls_failures);

        metric_family_append(&fams[FAM_NCOLLECTD_PLUGIN_WRITE_TIME_SECONDS],
                             VALUE_COUNTER_FLOAT64(CDTIME_T_TO_DOUBLE((cdtime_t)write_time)), NULL,
                             &(label_pair_const_t){.name="plugin", .value=stats->plugin}, NULL);
        metric_family_append(&fams[FAM_NCOLLECTD_PLUGIN_WRITE_CALLS],
                             VALUE_COUNTER(write_calls), NULL,
                             &(label_pair_const_t){.name="plugin", .value=stats->plugin}, NULL);
        metric_family_append(&fams[FAM_NCOLLECTD_PLUGIN_WRITE_FAILURES],
                             VALUE_COUNTER(write_calls_failures), NULL,
                             &(label_pair_const_t){.name="plugin", .value=stats->plugin}, NULL);

        stats = stats->next;
    }

    pthread_mutex_unlock(&write_queue_stats_lock);
}
