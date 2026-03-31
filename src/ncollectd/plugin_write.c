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
#include "libutils/avltree.h"
#include "libutils/common.h"
#include "libutils/time.h"
#include "libutils/complain.h"
#include "libmdb/mdb.h"
#include "queue.h"
#include "journal.h"

#include <stdatomic.h>
#include <sys/resource.h>

struct write_stats_s;
typedef struct write_stats_s write_stats_t;
struct write_stats_s {
    const char *plugin;
    atomic_ullong write_time;
    atomic_ullong write_calls;
    atomic_ullong write_calls_failures;
    atomic_ullong write_cpu_user;
    atomic_ullong write_cpu_sys;
    write_stats_t *next;
};

static pthread_mutex_t write_stats_lock = PTHREAD_MUTEX_INITIALIZER;
static write_stats_t *write_stats;

typedef struct {
    queue_elem_t super;
    metric_family_t *fam;
} write_queue_elem_t;

typedef struct {
    queue_thread_t super;
    write_stats_t *stats;
    plugin_write_cb write_cb;
    plugin_flush_cb flush_cb;
    cdtime_t flush_interval;
    cdtime_t flush_timeout;
    user_data_t ud;
} write_queue_thread_t;

static queue_t *write_queue;

typedef struct {
    char *plugin;
    metric_family_t *fam;
} write_journal_elem_t;

typedef struct {
    journal_thread_t super;
    write_stats_t *stats;
    plugin_write_cb write_cb;
    plugin_flush_cb flush_cb;
    cdtime_t flush_interval;
    cdtime_t flush_timeout;
    journal_ctx_t *journal;
    user_data_t ud;
} write_journal_thread_t;

static journal_t *write_journal;
static journal_ctx_t *write_journal_writer;

static atomic_ullong metrics_dispatched;

static plugin_filter_t *pre_cache_filter;
static plugin_filter_t *post_cache_filter;

static bool test_mode;

extern mdb_t *mdb;

enum {
    WRITE_METRIC_FAMILY_NAME_ID = 0x10,
    WRITE_PLUGIN_NAME_ID        = 0x20
};

static int plugin_write_pack(buf_t *buf, metric_family_t *fam, const char *plugin_name)
{
    size_t begin = 0;
    int status = pack_block_begin(buf, &begin);

    status |= pack_id(buf, WRITE_METRIC_FAMILY_NAME_ID);
    status |= metric_family_pack(buf, fam);

    if (plugin_name != NULL)
        status |= pack_string(buf, WRITE_PLUGIN_NAME_ID, plugin_name);

    status |= pack_block_end(buf, begin);
    return status;
}

static int plugin_write_unpack(rbuf_t *rbuf, write_journal_elem_t *elem)
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
        case WRITE_METRIC_FAMILY_NAME_ID:
            elem->fam = metric_family_unpack(&srbuf);
            break;
        case WRITE_PLUGIN_NAME_ID:
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
        if (elem->fam != NULL)
            metric_family_free(elem->fam);
        return -1;
    }

    unpack_avance_block(rbuf, &srbuf);

    return 0;
}

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

    pthread_mutex_lock(&write_stats_lock);
    if (writer->stats != NULL) {
        write_stats_t *prev = NULL;
        write_stats_t *stats = write_stats;
        while (stats != NULL) {
            if (writer->stats == stats) {
                write_stats_t *next = stats->next;
                free(stats);
                writer->stats = NULL;
                if (prev == NULL)
                    write_stats = next;
                else
                    prev->next = next;
                break;
            }
            prev = stats;
            stats = stats->next;
        }
    }
    pthread_mutex_unlock(&write_stats_lock);

    free(writer);
}

static void write_journal_thread_free(void *arg)
{
    write_journal_thread_t *writer = arg;
    if (writer == NULL)
        return;

    pthread_mutex_lock(&write_stats_lock);
    if (writer->stats != NULL) {
        write_stats_t *prev = NULL;
        write_stats_t *stats = write_stats;
        while (stats != NULL) {
            if (writer->stats == stats) {
                write_stats_t *next = stats->next;
                free(stats);
                writer->stats = NULL;
                if (prev == NULL)
                    write_stats = next;
                else
                    prev->next = next;
                break;
            }
            prev = stats;
            stats = stats->next;
        }
    }
    pthread_mutex_unlock(&write_stats_lock);

    if (writer->journal != NULL)
        journal_ctx_close(writer->journal);

    free(writer);
}


static int plugin_write_fam(write_stats_t *stats, plugin_write_cb write_cb, user_data_t *ud,
                            metric_family_t *fam)
{
    if (fam == NULL)
        return 0;

#ifdef HAVE_RUSAGE_THREAD
    struct rusage usage_start = {0};
    getrusage(RUSAGE_THREAD, &usage_start);
#endif

    cdtime_t start = cdtime();
    int status = write_cb(fam, ud);
    cdtime_t end = cdtime();

    cdtime_t diff = end - start;

#ifdef HAVE_RUSAGE_THREAD
struct rusage usage_finish = {0};
    getrusage(RUSAGE_THREAD, &usage_finish);
    cdtime_t cpu_user_time = TIMEVAL_TO_CDTIME_T(&usage_finish.ru_utime) -
                             TIMEVAL_TO_CDTIME_T(&usage_start.ru_utime);
    cdtime_t cpu_sys_time = TIMEVAL_TO_CDTIME_T(&usage_finish.ru_stime) -
                            TIMEVAL_TO_CDTIME_T(&usage_start.ru_stime);

    atomic_fetch_add(&stats->write_cpu_user, cpu_user_time);
    atomic_fetch_add(&stats->write_cpu_sys, cpu_sys_time);
#endif
    atomic_fetch_add(&stats->write_time, diff);
    atomic_fetch_add(&stats->write_calls, 1);

    if (status != 0)
        atomic_fetch_add(&stats->write_calls_failures, 1);

    return status;
}

static size_t plugin_write_journal_read(write_journal_thread_t *writer)
{

    journal_id_t begin = {0};
    journal_id_t end = {0};

    size_t reads = 0;

    int count = journal_ctx_read_interval(writer->journal, &begin, &end);
    if (count > 0) {
        for (int i = 0; i < count; i++, JOURNAL_ID_ADVANCE(&begin)) {
            end = begin;
            journal_message_t m;
            if (journal_ctx_read_message(writer->journal, &begin, &m) == 0) {
                write_journal_elem_t elem = {0};
                rbuf_t rbuf = {0};
                rbuf_init(&rbuf, m.mess, m.mess_len);
                int status = plugin_write_unpack(&rbuf, &elem);
                if (status == 0) {
                    if ((elem.plugin == NULL) ||
                        (strcasecmp(elem.plugin, writer->super.name) == 0))
                    plugin_write_fam(writer->stats, writer->write_cb, &writer->ud, elem.fam);

                    free(elem.plugin);
                    metric_family_free(elem.fam);
                }
                reads++;
            }

        }

        journal_ctx_read_checkpoint(writer->journal, &end);
    }

    return reads;
}

static void *plugin_write_journal_thread(void *args)
{
    write_journal_thread_t *writer = args;

    DEBUG("start: '%s'", writer->super.name);
    cdtime_t next_flush = 0;

    plugin_ctx_t ctx = {
        .name = (char *)writer->super.name,
        .interval = 0,
        .normalize_interval = 0,
    };
    plugin_set_ctx(ctx);

    while (writer->super.loop) {
        if (writer->flush_cb != NULL)
            next_flush = cdtime() + writer->flush_interval;

        pthread_mutex_lock(&write_journal->lock);
        if (writer->flush_cb != NULL) {
            pthread_cond_timedwait(&write_journal->cond, &write_journal->lock,
                                   &CDTIME_T_TO_TIMESPEC(next_flush));
        } else {
            pthread_cond_wait(&write_journal->cond, &write_journal->lock);
        }
        pthread_mutex_unlock(&write_journal->lock);

        int no_data = 0;
        do {
            int reads = plugin_write_journal_read(writer);
            no_data = reads == 0 ? no_data + 1 : 0;
        } while (no_data <= 1);

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

static void *plugin_write_queue_thread(void *args)
{
    write_queue_thread_t *writer = args;

    DEBUG("start '%s'", writer->super.name);
    cdtime_t next_flush = 0;

    while (writer->super.loop) {
        if (writer->flush_cb != NULL)
            next_flush = cdtime() + writer->flush_interval;

        write_queue_elem_t *elem = (write_queue_elem_t *)queue_dequeue(write_queue,
                                                                       (queue_thread_t*)writer,
                                                                       next_flush);
        if (elem != NULL) {
            DEBUG("%s: de-queue %p (remaining queue length: %ld)",
                  writer->super.name, (void *)elem, writer->super.queue_length);

            /* Should elem be written to all plugins or this plugin in particular? */
            if ((elem->super.plugin == NULL) ||
                (strcasecmp(elem->super.plugin, writer->super.name) == 0)) {

                plugin_ctx_t ctx = elem->super.ctx;
                ctx.name = (char *)writer->super.name;
                plugin_set_ctx(ctx);

                plugin_write_fam(writer->stats, writer->write_cb, &writer->ud, elem->fam);
            }

            /* Free the element if it is not referenced by another queue or thread. */
            queue_ref_single(write_queue, (queue_elem_t *)elem, -1);
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

static int plugin_dispatch_metric_internal_filtered(metric_family_t *fam, plugin_filter_t *filter)
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
                                                 plugin_filter_t *filter, cdtime_t time)
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

        if (fam->name == NULL) {
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
            metric_family_metric_reset(fam);
            continue;
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

int plugin_register_write_journal(char *full_name, write_stats_t *writer_stats,
                                  plugin_write_cb write_cb, plugin_flush_cb flush_cb,
                                  cdtime_t flush_interval, cdtime_t flush_timeout,
                                  user_data_t const *ud)
{
    write_journal_thread_t *writer = calloc(1, sizeof(*writer));
    if (writer == NULL) {
        ERROR("calloc failed.");
        return -1;
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

    journal_ctx_add_subscriber(write_journal_writer, full_name, JOURNAL_END);

    writer->journal = journal_get_reader(write_journal, full_name);
    if (writer->journal == NULL) {
        ERROR("cannot create new journal context.");
        free(writer);
        return -1;
    }

    int status = journal_thread_start(write_journal, (journal_thread_t *)writer,
                                      full_name, plugin_write_journal_thread, (void *)writer);
    if (status != 0) {
        journal_ctx_close(writer->journal);
        free(writer);
        return -1;
    }

    return 0;
}

int plugin_register_write_queue(char *full_name, write_stats_t *writer_stats,
                                plugin_write_cb write_cb, plugin_flush_cb flush_cb,
                                cdtime_t flush_interval, cdtime_t flush_timeout,
                                user_data_t const *ud)
{
    write_queue_thread_t *writer = calloc(1, sizeof(*writer));
    if (writer == NULL) {
        ERROR("calloc failed.");
        return -1;
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

    int status = queue_thread_start(write_queue, (queue_thread_t *)writer,
                                    full_name, plugin_write_queue_thread, (void *)writer);
    if (status != 0) {
        free(writer);
        return -1;
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

    write_stats_t *writer_stats = calloc(1, sizeof(*writer_stats));
    if (writer_stats == NULL) {
        ERROR("calloc failed.");
        free_userdata(ud);
        free(full_name);
        return ENOMEM;
    }

    int status = 0;

    if (write_queue != NULL) {
        status = plugin_register_write_queue(full_name, writer_stats, write_cb, flush_cb,
                                             flush_interval, flush_timeout, ud);
    } else if (write_journal != NULL) {
        status = plugin_register_write_journal(full_name, writer_stats, write_cb, flush_cb,
                                               flush_interval, flush_timeout, ud);
    }

    if (status != 0) {
        free_userdata(ud);
        free(full_name);
        free(writer_stats);
        return -1;
    }

    pthread_mutex_lock(&write_stats_lock);
    writer_stats->next = write_stats;
    write_stats = writer_stats;
    pthread_mutex_unlock(&write_stats_lock);

    return 0;
}

strlist_t *plugin_get_writers(void)
{
    if (write_queue != NULL)
        return queue_get_threads(write_queue);
    if (write_journal != NULL)
        return journal_get_threads(write_journal);
    return NULL;
}

int plugin_unregister_write(const char *name)
{
    if (write_queue != NULL)
        return queue_thread_stop(write_queue, name);
    if (write_journal != NULL)
        return journal_thread_stop(write_journal, name);
    return 0;
}

int plugin_write(const char *plugin, metric_family_t *fam, bool clone)
{
    if (fam == NULL)
        return EINVAL;

    if (write_queue != NULL) {
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

        return queue_enqueue(write_queue, plugin, (queue_elem_t *)elem);
    } else if (write_journal != NULL) {
        if (write_journal_writer != NULL) {
            buf_t buf = BUF_CREATE;
            buf_resize(&buf, 4096);
            int status = plugin_write_pack(&buf, fam, plugin);
            if (status != 0) {
                ERROR("cannot serialize metric family '%s'", fam->name);
                buf_destroy(&buf);
                return status;
            }

            if(journal_ctx_write(write_journal_writer, buf.ptr, buf_len(&buf)) != 0)
                atomic_fetch_add(&metrics_dispatched, (unsigned long long)fam->metric.num);

            pthread_mutex_lock(&write_journal->lock);
            pthread_cond_broadcast(&write_journal->cond);
            pthread_mutex_unlock(&write_journal->lock);

            buf_destroy(&buf);

            if (!clone)
                metric_family_free(fam);
        }
    }

    return 0;
}

int plugin_config_write(void)
{
    char const *filter_name = global_option_get("pre-cache-filter");
    pre_cache_filter = filter_global_get_by_name(filter_name);

    filter_name = global_option_get("post-cache-filter");
    post_cache_filter = filter_global_get_by_name(filter_name);

    cf_queue_t *metric_queue = global_option_get_metric_queue();

    if (metric_queue->type == CF_QUEUE_MEMORY) {
        write_queue = queue_new("metrics");
        if (write_queue == NULL) {
            ERROR("cannot alloc queue");
            return -1;
        }
        write_queue->limit_high = metric_queue->memory.limit_high;
        write_queue->limit_low = metric_queue->memory.limit_low;
        write_queue->free_elem_cb = write_queue_elem_free;
        write_queue->free_thread_cb = write_queue_thread_free;
    } else {
        if (!test_mode) {
            write_journal = journal_new(metric_queue->journal.path);
            if (write_journal == NULL) {
                ERROR("cannot open journal '%s'.", metric_queue->journal.path);
                return -1;
            }

            journal_config_checksum(write_journal, metric_queue->journal.checksum);
            journal_config_compress(write_journal, metric_queue->journal.compress);
            journal_config_retention_size(write_journal, metric_queue->journal.retention_size);
            journal_config_retention_time(write_journal, metric_queue->journal.retention_time);
            journal_config_segment_size(write_journal, metric_queue->journal.segment_size);

            write_journal->free_thread_cb = write_journal_thread_free;

            journal_init(write_journal);

            write_journal_writer = journal_get_writer(write_journal);
            if (write_journal_writer == NULL) {
                ERROR("cannot open journal '%s' for writting.", metric_queue->journal.path);
                return -1;
            }

            buf_t buf = BUF_CREATE;
            buf_resize(&buf, 4096);
            int status = plugin_write_pack(&buf, NULL, NULL);
            if (status != 0) {
                ERROR("cannot serialize NULL metric family.");
                buf_destroy(&buf);
                return -1;
            }

            journal_ctx_write(write_journal_writer, buf.ptr, buf_len(&buf));

            buf_destroy(&buf);
        }
    }

    return 0;
}

int plugin_init_write(void)
{
    return 0;
}

void plugin_shutdown_write(void)
{
    pre_cache_filter = NULL;
    post_cache_filter = NULL;

    filter_global_free();

    plugin_unregister_write(NULL);

    if (write_journal != NULL ){
        if (write_journal_writer != NULL) {
            journal_ctx_close(write_journal_writer);
            write_journal_writer = NULL;
        }

        journal_close(write_journal);
        write_journal = NULL;
    }
}

void plugin_write_stats(metric_family_t *fams)
{
    if (write_queue != NULL) {
        long length = queue_length(write_queue);
        metric_family_append(&fams[FAM_NCOLLECTD_WRITE_QUEUE_LENGTH],
                             VALUE_GAUGE(length), NULL, NULL);
        uint64_t dropped = queue_dropped(write_queue);
        metric_family_append(&fams[FAM_NCOLLECTD_WRITE_QUEUE_DROPPED],
                             VALUE_COUNTER(dropped), NULL, NULL);
    }

    unsigned long long dispatched = atomic_load(&metrics_dispatched);
    metric_family_append(&fams[FAM_NCOLLECTD_METRICS_DISPACHED],
                         VALUE_COUNTER(dispatched), NULL, NULL);

    pthread_mutex_lock(&write_stats_lock);

    write_stats_t *stats = write_stats;
    while(stats != NULL) {
        unsigned long long write_time = atomic_load(&stats->write_time);
        unsigned long long write_calls = atomic_load(&stats->write_calls);
        unsigned long long write_calls_failures = atomic_load(&stats->write_calls_failures);
        unsigned long long write_cpu_user = atomic_load(&stats->write_cpu_user);
        unsigned long long write_cpu_sys = atomic_load(&stats->write_cpu_sys);

        metric_family_append(&fams[FAM_NCOLLECTD_PLUGIN_WRITE_TIME_SECONDS],
                             VALUE_COUNTER_FLOAT64(CDTIME_T_TO_DOUBLE((cdtime_t)write_time)), NULL,
                             &LABEL_PAIR_CONST("plugin", stats->plugin), NULL);
        metric_family_append(&fams[FAM_NCOLLECTD_PLUGIN_WRITE_CALLS],
                             VALUE_COUNTER(write_calls), NULL,
                             &LABEL_PAIR_CONST("plugin", stats->plugin), NULL);
        metric_family_append(&fams[FAM_NCOLLECTD_PLUGIN_WRITE_FAILURES],
                             VALUE_COUNTER(write_calls_failures), NULL,
                             &LABEL_PAIR_CONST("plugin", stats->plugin), NULL);
        metric_family_append(&fams[FAM_NCOLLECTD_PLUGIN_WRITE_CPU_USER],
                             VALUE_COUNTER_FLOAT64(CDTIME_T_TO_DOUBLE(write_cpu_user)), NULL,
                             &LABEL_PAIR_CONST("plugin", stats->plugin), NULL);
        metric_family_append(&fams[FAM_NCOLLECTD_PLUGIN_WRITE_CPU_SYSTEM],
                             VALUE_COUNTER_FLOAT64(CDTIME_T_TO_DOUBLE(write_cpu_sys)), NULL,
                             &LABEL_PAIR_CONST("plugin", stats->plugin), NULL);

        stats = stats->next;
    }

    pthread_mutex_unlock(&write_stats_lock);
}

void plugin_write_test_mode(bool mode)
{
    test_mode = mode;
}
