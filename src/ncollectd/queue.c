// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2005-2014 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>
// SPDX-FileContributor: Manoj Srivastava <srivasta at google.com>

#include "ncollectd.h"
#include "plugin_internal.h"
#include "libutils/common.h"
#include "libutils/random.h"
#include "libutils/time.h"
#include "libutils/complain.h"

#include "queue.h"

static int queue_ref_single_internal(queue_t *queue, queue_elem_t *elem, long dir)
{
    elem->ref_count += dir;

    assert(elem->ref_count >= 0);

    if (elem->ref_count == 0) {
        if (queue->tail == elem) {
            queue->tail = NULL;
            assert(elem->next == NULL);
        }

        free(elem->plugin);

        if (queue->free_elem_cb != NULL)
            queue->free_elem_cb(elem);
        else
            free(elem);

        return 1;
    }

    return 0;
}

int queue_ref_single(queue_t *queue, queue_elem_t *elem, long dir)
{
    int result = 0;

    pthread_mutex_lock(&queue->lock);
    result = queue_ref_single_internal(queue, elem, dir);
    pthread_mutex_unlock(&queue->lock);

    return result;
}

static void queue_ref_all(queue_t *queue, queue_elem_t *start, long dir)
{
    while (start != NULL) {
        queue_elem_t *elem = start;
        start = elem->next;

        queue_ref_single_internal(queue, elem, dir);
    }
}

int queue_enqueue(queue_t *queue, const char *plugin, queue_elem_t *ins_head)
{
    if (ins_head == NULL)
        return EINVAL;

    char *dup_plugin = NULL;
    if (plugin != NULL) {
        dup_plugin = strdup(plugin);
        if (dup_plugin == NULL) {
            PLUGIN_ERROR("strdup failed");
            return ENOMEM;
        }
    }

    ins_head->ctx = plugin_get_ctx();
    ins_head->plugin = dup_plugin;
    ins_head->ref_count = 0;
    ins_head->next = NULL;

    pthread_mutex_lock(&queue->lock);

    if (queue->threads == NULL) {
        c_complain_once(LOG_WARNING, &queue->complaint,
                        "No %s callback has been registered. "
                        "Please load at least one output plugin, "
                        "if you want the collected data to be stored.", queue->kind);

        /* Element in the ins_head queue already have zero reference count
         * but without any write threads there is noone to free them.
         * make sure they are freed by de-refing them 0 times. */
        queue_ref_all(queue, ins_head, 0);
        pthread_mutex_unlock(&queue->lock);

        return ENOENT;
    }

    queue_elem_t *ins_tail = NULL;
    long num_elems = 0;
    /* More than one element may be enqueued at once. Count elements and find local tail. */
    queue_elem_t *elem = ins_head;
    while (elem != NULL) {
        ins_tail = elem;
        num_elems++;
        elem = elem->next;
    }

    /* Add reference to new elements to existing queue (if there is one) and update the tail. */
    if (queue->tail != NULL)
        queue->tail->next = ins_head;
    queue->tail = ins_tail;

    /* Iterate through all registered write plugins/threads to:
     * a) update their head pointer if their queue is currently empty.
     * b) find the thread with the longest queue to apply limits later. */
    queue_thread_t *slowest_thread = queue->threads;

    queue_thread_t *thread = queue->threads;
    while(thread != NULL) {
        if (thread->head == NULL) {
            /* coverity[USE_AFTER_FREE] */ 
            thread->head = ins_head;
        }

        /* Mark the new elements as to be consumed by this thread */
        queue_ref_all(queue, ins_head, 1);
        thread->queue_length += num_elems;

        if (thread->queue_length > slowest_thread->queue_length)
            slowest_thread = thread;

        thread = thread->next;
    }

    /* Enforce limit_high (unless it is infinite (e.g. == 0)) */
    while ((queue->limit_high != 0) && (slowest_thread->queue_length > queue->limit_high)) {
        /* Select a random element to drop between the last position in the slowest
         * thread's queue and queue positon "limit_low". This makes sure that
         * write plugins that do not let the queue get longer than "limit_low"
         * will never drop values, regardless of what other plugins do. */
        long drop_pos = cdrand_u() % (slowest_thread->queue_length - queue->limit_low)
                        + queue->limit_low;

        /* Walk the queue and count elements until element number drop_pos is
         * found. to_drop will point to the element in question and to_spare
         * will point to one element before to_drop (it it exists). */
        queue_elem_t *to_spare = NULL;
        queue_elem_t *to_drop = slowest_thread->head;

        long queue_pos = slowest_thread->queue_length - 1;
        while(queue_pos > drop_pos) {
            to_spare = to_drop;
            to_drop = to_drop->next;
            queue_pos--;
        }

        /* Unlink to_drop from linked list if it is not the head (in which case
         * to_spare is NULL and it will be unlinked below) */
        if (to_spare != NULL)
            to_spare->next = to_drop->next;

        /* Iterate through all registered write plugins/threads to:
         * a) update the head if it references to_drop.
         * b) update the thread's queue length if to_drop was still in it's queue. */
        thread = queue->threads;
        while(thread != NULL) {
            if (thread->head == to_drop)
                thread->head = to_drop->next;

            /* Reduce reference count and queue length for every affected queue.
             * There may still be references held in write_threads if the element was
             * just de-queued in any and is currently used in the callback,
             * so freeing may be delayed until it is dropped there. */
            if (drop_pos < thread->queue_length) {
                thread->queue_length--;
                if (queue_ref_single_internal(queue, to_drop, -1))
                    break;
            }

            thread = thread->next;
        }

        queue->dropped++;
    }

    pthread_cond_broadcast(&queue->cond);
    pthread_mutex_unlock(&queue->lock);

    return 0;
}

queue_elem_t *queue_dequeue(queue_t *queue, queue_thread_t *writer, cdtime_t abstime)
{
    queue_elem_t *elem = NULL;

    pthread_mutex_lock(&queue->lock);

    elem = writer->head;
    if (elem == NULL) {
        if (abstime > 0) {
            /* coverity[BAD_CHECK_OF_WAIT_COND] */
            pthread_cond_timedwait(&queue->cond, &queue->lock, &CDTIME_T_TO_TIMESPEC(abstime));
        } else {
            /* coverity[BAD_CHECK_OF_WAIT_COND] */
            pthread_cond_wait(&queue->cond, &queue->lock);
        }

        pthread_mutex_unlock(&queue->lock);
        return NULL;
    }

    /* Unlink early so that queue_enqueue can freely manipulate the
     * head while the lock is not held in the callback. */
    writer->head = elem->next;
    writer->queue_length--;

    pthread_mutex_unlock(&queue->lock);

    return elem;
}

long queue_length(queue_t *queue)
{
    long length = 0;

    pthread_mutex_lock(&queue->lock);

    queue_thread_t *thread = queue->threads;
    while(thread != NULL) {
        if (thread->queue_length > length)
            length = thread->queue_length;
        thread = thread->next;
    }

    pthread_mutex_unlock(&queue->lock);

    return length;
}

uint64_t queue_dropped(queue_t *queue)
{
    uint64_t dropped = 0;

    pthread_mutex_lock(&queue->lock);
    dropped = queue->dropped;
    pthread_mutex_unlock(&queue->lock);

    return dropped;
}

strlist_t *queue_get_threads(queue_t *queue)
{
    pthread_mutex_lock(&queue->lock);

    if (queue->threads == NULL) {
        pthread_mutex_unlock(&queue->lock);
        return NULL;
    }

    int size = 0;
    for (queue_thread_t *piv = queue->threads; piv != NULL; piv = piv->next) {
        size++;
    }

    strlist_t *sl = strlist_alloc(size);
    if (sl == NULL) {
        pthread_mutex_unlock(&queue->lock);
        return NULL;
    }

    for (queue_thread_t *piv = queue->threads; piv != NULL; piv = piv->next) {
        strlist_append(sl, piv->name);
    }

    pthread_mutex_unlock(&queue->lock);

    return sl;
}

int queue_thread_start(queue_t *queue, queue_thread_t *thread, char *name,
                                       void *(*start_routine)(void *), void *arg)
{
    thread->name = name;
    thread->loop = true;
    thread->queue_length = 0;
    thread->head = NULL;

    pthread_mutex_lock(&queue->lock);

    int status = pthread_create(&thread->thread, NULL, start_routine, arg);
    if (status != 0) {
        ERROR("pthread_create failed with status %i: %s.", status, STRERROR(status));
        pthread_mutex_unlock(&queue->lock);
        return status;
    }

    char thread_name[THREAD_NAME_MAX];
    ssnprintf(thread_name, sizeof(thread_name), "%s", name);
    set_thread_name(thread->thread, thread_name);

    thread->next = queue->threads;
    queue->threads = thread;

    pthread_mutex_unlock(&queue->lock);

    return status;
}

int queue_thread_stop(queue_t *queue, const char *name)
{
    pthread_mutex_lock(&queue->lock);

    /* Build to completely new thread lists. One with threads to_stop and another
     * with threads to_keep. If name is NULL to_keep will be empty and to_stop
     * will contain all threads. If name is NULL to_stop will contain the
     * relevant thread and to_keep will contain all remaining threads. */
    queue_thread_t *to_stop = NULL;
    queue_thread_t *to_keep = NULL;

    for (queue_thread_t *piv = queue->threads; piv != NULL;) {
        queue_thread_t *next = piv->next;

        if ((name == NULL) || (strcasecmp(name, piv->name) == 0)) {
            piv->loop = false;
            piv->next = to_stop;
            to_stop = piv;
        } else {
            piv->next = to_keep;
            to_keep = piv;
        }

        piv = next;
    }

    queue->threads = to_keep;

    pthread_cond_broadcast(&queue->cond);
    pthread_mutex_unlock(&queue->lock);

    /* Return error if the requested thread was not found */
    if ((to_stop == NULL) && (name != NULL))
        return ENOENT;

    int status = 0;

    while (to_stop != NULL) {
        /* coverity[MISSING_LOCK] */
        queue_thread_t *next = to_stop->next;

        int ret = pthread_join(to_stop->thread, NULL);
        if (ret != 0) {
            ERROR("pthread_join failed for %s.", to_stop->name);
            status = ret;
        }

        free(to_stop->name);

        /* Drop references to all remaining queue elements */
        if (to_stop->head != NULL) {
            queue_ref_all(queue, to_stop->head, -1);
            to_stop->head = NULL;
            to_stop->queue_length = 0;
        }

        if (queue->free_thread_cb)
            queue->free_thread_cb(to_stop);
        else
            free(to_stop);
        to_stop = next;
    }

    return status;
}
