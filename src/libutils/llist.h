/* SPDX-License-Identifier: GPL-2.0-only OR MIT                 */
/* SPDX-FileCopyrightText: Copyright (C) 2006 Florian Forster   */
/* SPDX-FileContributor: Florian Forster <octo at collectd.org> */

#pragma once

struct llentry_s {
    char *key;
    void *value;
    struct llentry_s *next;
};
typedef struct llentry_s llentry_t;

struct llist_s;
typedef struct llist_s llist_t;

llist_t *llist_create(void);

void llist_destroy(llist_t *l);

llentry_t *llentry_create(char *key, void *value);

void llentry_destroy(llentry_t *e);

void llist_append(llist_t *l, llentry_t *e);

void llist_prepend(llist_t *l, llentry_t *e);

void llist_remove(llist_t *l, llentry_t *e);

int llist_size(llist_t *l);

llentry_t *llist_search(llist_t *l, const char *key);

llentry_t *llist_search_custom(llist_t *l, int (*compare)(llentry_t *, const void *),
                                           const void *user_data);

llentry_t *llist_head(llist_t *l);

llentry_t *llist_tail(llist_t *l);
