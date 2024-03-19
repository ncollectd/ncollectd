// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "libutils/htable.h"

void htable_destroy(htable_t *ht, htable_free_cb free_cb, void *arg)
{
    if (ht == NULL)
        return;

    for (size_t n=0; n < ht->size; n++) {
        if (ht->tbl[n].data != NULL)
            free_cb(ht->tbl[n].data, arg);
    }

    free(ht->tbl);
    ht->tbl = NULL;
    ht->size = 0;
    ht->used = 0;
}

int htable_init(htable_t *ht, size_t size)
{
    htable_entry_t *ntbl = calloc(size, sizeof(*ntbl));
    if (!ntbl)
        return ENOMEM;

    ht->size = size;
    ht->used = 0;
    ht->tbl = ntbl;
    return 0;
}

static int htable_resize (htable_t *ht, htable_cmp_t cmp)
{
    uint32_t nsize = ht->used * 2;
    if (nsize <= ht->size)
        return 0;

    htable_entry_t *ntbl = calloc(nsize, sizeof(*ntbl));
    if (!ntbl)
        return ENOMEM;

    htable_t nht;
    nht.size = nsize;
    nht.used = 0;
    nht.tbl = ntbl;

    for (size_t n=0; n < ht->size; n++) {
        if (ht->tbl[n].data != NULL)
            htable_add(&nht, ht->tbl[n].hash, ht->tbl[n].data, cmp);
    }

    ht->size = nsize;
    ht->used = nht.used;
    free(ht->tbl);
    ht->tbl = ntbl;
    return 0;
}

int htable_add (htable_t *ht, htable_hash_t hash, void *data, htable_cmp_t cmp)
{
    uint32_t pos = hash % ht->size;
    uint32_t dib = 0;

    while (1) {
        if (ht->tbl[pos].data == NULL) {
                ht->tbl[pos].hash = hash;
                ht->tbl[pos].dib = dib;
                ht->tbl[pos].data = data;
                ht->used++;
                break;
        }
        if (ht->tbl[pos].hash == hash) {
            if (cmp(data, ht->tbl[pos].data) == 0)
                return 1;
        }
        if (ht->tbl[pos].dib < dib) {
                uint32_t mv_hash = ht->tbl[pos].hash;
                void *mv_data = ht->tbl[pos].data;

                ht->tbl[pos].hash = hash;
                ht->tbl[pos].dib = dib;
                ht->tbl[pos].data = data;

                return htable_add(ht, mv_hash, mv_data, cmp);
        }
        dib++;
        pos = (pos + 1) % ht->size;
    }

    if ((ht->used * 3)  > (ht->size *2))
        return htable_resize(ht, cmp);

    return 0;
}

void *htable_find(htable_t *ht, htable_hash_t hash, const void *cmp_data, htable_cmp_t cmp)
{
    uint32_t pos = hash % ht->size;

    while (1) {
        if (ht->tbl[pos].data == NULL)
            break;

        if ((ht->tbl[pos].hash == hash) && (ht->tbl[pos].data != NULL)) {
            if (cmp(cmp_data, ht->tbl[pos].data) == 0) {
                return ht->tbl[pos].data;
            }
        }
        pos = (pos + 1) % ht->size;
    }

    return NULL;
}

