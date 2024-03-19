// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2006 Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Florian Forster <octo at collectd.org>

#include <stdlib.h>
#include <string.h>

#include "libutils/llist.h"

/*
 * Private data types
 */
struct llist_s {
    llentry_t *head;
    llentry_t *tail;
    int size;
};

/*
 * Public functions
 */
llist_t *llist_create(void)
{
    llist_t *ret;

    ret = calloc(1, sizeof(*ret));
    if (ret == NULL)
        return NULL;

    return ret;
}

void llist_destroy(llist_t *l)
{
    llentry_t *e_this;
    llentry_t *e_next;

    if (l == NULL)
        return;

    for (e_this = l->head; e_this != NULL; e_this = e_next) {
        e_next = e_this->next;
        llentry_destroy(e_this);
    }

    free(l);
}

llentry_t *llentry_create(char *key, void *value)
{
    llentry_t *e;

    e = malloc(sizeof(*e));
    if (e) {
        e->key = key;
        e->value = value;
        e->next = NULL;
    }

    return e;
}

void llentry_destroy(llentry_t *e)
{
    free(e);
}

void llist_append(llist_t *l, llentry_t *e)
{
    e->next = NULL;

    if (l->tail == NULL)
        l->head = e;
    else
        l->tail->next = e;

    l->tail = e;

    ++(l->size);
}

void llist_prepend(llist_t *l, llentry_t *e)
{
    e->next = l->head;
    l->head = e;

    if (l->tail == NULL)
        l->tail = e;

    ++(l->size);
}

void llist_remove(llist_t *l, llentry_t *e)
{
    llentry_t *prev;

    if ((l == NULL) || (e == NULL))
        return;

    prev = l->head;
    while ((prev != NULL) && (prev->next != e))
        prev = prev->next;

    if (prev != NULL)
        prev->next = e->next;
    if (l->head == e)
        l->head = e->next;
    if (l->tail == e)
        l->tail = prev;

    --(l->size);
}

int llist_size(llist_t *l)
{
    return l ? l->size : 0;
}

static int llist_strcmp(llentry_t *e, const void *ud)
{
    if ((e == NULL) || (ud == NULL))
        return -1;
    return strcmp(e->key, (const char *)ud);
}

llentry_t *llist_search(llist_t *l, const char *key)
{
    return llist_search_custom(l, llist_strcmp, key);
}

llentry_t *llist_search_custom(llist_t *l, int (*compare)(llentry_t *, const void *), const void *user_data)
{
    llentry_t *e;

    if (l == NULL)
        return NULL;

    e = l->head;
    while (e != NULL) {
        llentry_t *next = e->next;

        if (compare(e, user_data) == 0)
            break;

        e = next;
    }

    return e;
}

llentry_t *llist_head(llist_t *l)
{
    if (l == NULL)
        return NULL;
    return l->head;
}

llentry_t *llist_tail(llist_t *l)
{
    if (l == NULL)
        return NULL;
    return l->tail;
}
