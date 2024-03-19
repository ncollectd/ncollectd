/* SPDX-License-Identifier: GPL-2.0-only OR MIT                         */
/* SPDX-FileCopyrightText: Copyright (C) 2005-2014 Florian octo Forster */
/* SPDX-FileContributor: Florian octo Forster <octo at collectd.org>    */
/* SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>              */
/* SPDX-FileContributor: Manoj Srivastava <srivasta at google.com>      */

#pragma once

struct queue_elem_s;
typedef struct queue_elem_s queue_elem_t;
struct queue_elem_s {
    char *plugin;
    plugin_ctx_t ctx;
    long ref_count;
    queue_elem_t *next;
};

struct queue_thread_s;
typedef struct queue_thread_s queue_thread_t;
struct queue_thread_s {
    char *name;
    bool loop;
    long queue_length;
    pthread_t thread;
    queue_elem_t *head;
    queue_thread_t *next;
};

typedef struct {
    const char *kind;
    long limit_high;
    long limit_low;
    uint64_t dropped;
    c_complain_t complaint;
    void (*free_elem_cb)(void *);
    void (*free_thread_cb)(void *);
    pthread_mutex_t lock;
    pthread_cond_t cond;
    queue_elem_t *tail;
    queue_thread_t *threads;
} queue_t;

int queue_ref_single(queue_t *queue, queue_elem_t *elem, long dir);

int queue_enqueue(queue_t *queue, const char *plugin, queue_elem_t *ins_head);

queue_elem_t *queue_dequeue(queue_t *queue, queue_thread_t *writer, cdtime_t abstime);

long queue_length(queue_t *queue);

uint64_t queue_dropped(queue_t *queue);

strlist_t *queue_get_threads(queue_t *queue);

int queue_thread_start(queue_t *queue, queue_thread_t *thread, char *name,
                                       void *(*start_routine)(void *), void *arg);

int queue_thread_stop(queue_t *queue, const char *name);
