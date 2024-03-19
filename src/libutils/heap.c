// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2009 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

#include "ncollectd.h"

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

#include "libutils/heap.h"

struct c_heap_s {
    pthread_mutex_t lock;
    int (*compare)(const void *, const void *);

    void **list;
    size_t list_len;    /* # entries used */
    size_t list_size; /* # entries allocated */
};

enum reheap_direction { DIR_UP, DIR_DOWN };

static void reheap(c_heap_t *h, size_t root, enum reheap_direction dir)
{
    /* Calculate the positions of the children */
    size_t left = (2 * root) + 1;
    /* coverity[MISSING_LOCK] */
    if (left >= h->list_len)
        left = 0;

    size_t right = (2 * root) + 2;
    if (right >= h->list_len)
        right = 0;

    size_t min;
    /* Check which one of the children is smaller. */
    if ((left == 0) && (right == 0)) {
        return;
    } else if (left == 0) {
        min = right;
    } else if (right == 0) {
        min = left;
    } else {
        int status = h->compare(h->list[left], h->list[right]);
        if (status > 0)
            min = right;
        else
            min = left;
    }

    int status = h->compare(h->list[root], h->list[min]);
    if (status <= 0) {
        /* We didn't need to change anything, so the rest of the tree should be
         * okay now. */
        return;
    } else {
        void *tmp;

        tmp = h->list[root];
        h->list[root] = h->list[min];
        h->list[min] = tmp;
    }

    if ((dir == DIR_UP) && (root == 0))
        return;

    if (dir == DIR_UP)
        reheap(h, (root - 1) / 2, dir);
    else if (dir == DIR_DOWN)
        reheap(h, min, dir);
}

c_heap_t *c_heap_create(int (*compare)(const void *, const void *))
{
    if (compare == NULL)
        return NULL;

    c_heap_t *h = calloc(1, sizeof(*h));
    if (h == NULL)
        return NULL;

    pthread_mutex_init(&h->lock, /* attr = */ NULL);
    h->compare = compare;

    h->list = NULL;
    h->list_len = 0;
    h->list_size = 0;

    return h;
}

void c_heap_destroy(c_heap_t *h)
{
    if (h == NULL)
        return;

    h->list_len = 0;
    h->list_size = 0;
    free(h->list);
    h->list = NULL;

    pthread_mutex_destroy(&h->lock);

    free(h);
}

int c_heap_insert(c_heap_t *h, void *ptr)
{
    if ((h == NULL) || (ptr == NULL))
        return -EINVAL;

    pthread_mutex_lock(&h->lock);

    assert(h->list_len <= h->list_size);
    if (h->list_len == h->list_size) {
        void **tmp;

        tmp = realloc(h->list, (h->list_size + 16) * sizeof(*h->list));
        if (tmp == NULL) {
            pthread_mutex_unlock(&h->lock);
            return -ENOMEM;
        }

        h->list = tmp;
        h->list_size += 16;
    }

    /* Insert the new node as a leaf. */
    size_t index = h->list_len;
    h->list[index] = ptr;
    h->list_len++;

    /* Reorganize the heap from bottom up. */
    reheap(h, /* parent of this node = */ (index - 1) / 2, DIR_UP);

    pthread_mutex_unlock(&h->lock);
    return 0;
}

void *c_heap_get_root(c_heap_t *h)
{
    void *ret = NULL;

    if (h == NULL)
        return NULL;

    pthread_mutex_lock(&h->lock);

    if (h->list_len == 0) {
        pthread_mutex_unlock(&h->lock);
        return NULL;
    } else if (h->list_len == 1) {
        ret = h->list[0];
        h->list[0] = NULL;
        h->list_len = 0;
    } else { /* if (h->list_len > 1) */
        ret = h->list[0];
        h->list[0] = h->list[h->list_len - 1];
        h->list[h->list_len - 1] = NULL;
        h->list_len--;

        reheap(h, /* root = */ 0, DIR_DOWN);
    }

    /* free some memory */
    if ((h->list_len + 32) < h->list_size) {
        void **tmp;

        tmp = realloc(h->list, (h->list_len + 16) * sizeof(*h->list));
        if (tmp != NULL) {
            h->list = tmp;
            h->list_size = h->list_len + 16;
        }
    }

    pthread_mutex_unlock(&h->lock);

    return ret;
}
