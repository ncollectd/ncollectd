/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

typedef uint32_t htable_hash_t;

typedef struct {
    htable_hash_t hash;
    uint32_t dib;
    void *data;
} htable_entry_t;

typedef struct {
    uint32_t size;
    uint32_t used;
    htable_entry_t *tbl;
} htable_t;

typedef int (*htable_cmp_t)(const void *, const void *);

#define HTABLE_HASH_INIT ((htable_hash_t)2166136261)

static inline htable_hash_t htable_hash(const char *str, htable_hash_t hash)
{
    const unsigned char *ustr = (const unsigned char *)str;
    while (*ustr != '\0') {
        hash ^= (htable_hash_t)*(ustr++);
        hash *= (htable_hash_t)0x01000193;
    }
    return hash;
}

static inline htable_hash_t htable_nhash(const char *str,  size_t n, htable_hash_t hash)
{
    const unsigned char *ustr = (const unsigned char *)str;
    for (size_t i = 0; i < n ; i++) {
        hash ^= (htable_hash_t)*(ustr++);
        hash *= (htable_hash_t)0x01000193;
    }
    return hash;
}

typedef struct {
    uint32_t n;
} htable_iter_t;

static inline void *htable_next(htable_t *ht, htable_iter_t *iter)
{
    while ((iter->n < ht->size) && (ht->tbl[iter->n].data != NULL)) iter->n++;
    if (iter->n == ht->size)
        return NULL;
    return ht->tbl[iter->n].data;
}

void *htable_find(htable_t *ht, htable_hash_t hash, const void *data, htable_cmp_t cmp);

int htable_add(htable_t *ht, htable_hash_t hash, void *data, htable_cmp_t cmp);

int htable_init(htable_t *ht, size_t size);

typedef void (*htable_free_cb)(void *, void *);

void htable_destroy(htable_t *ht, htable_free_cb free_cb, void *arg);
