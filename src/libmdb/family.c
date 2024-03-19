// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "libmdb/family_metric_list.h"
#include "libmdb/family.h"

#define METRIC_FAMILY_TABLE_SIZE 256

static void family_free(void *arg, __attribute__((unused)) void *unused)
{
    family_t *fam = arg;
    if (fam == NULL)
        return;

    free(fam->name);
    free(fam->help);
    free(fam->unit);

    free(fam);
}

int family_init(mdb_family_t *mdbfam)
{
    htable_init(&mdbfam->family_table, METRIC_FAMILY_TABLE_SIZE);
    return 0;
}

void family_destroy(mdb_family_t *mdbfam)
{
     htable_destroy(&mdbfam->family_table, family_free, NULL);
}

static int family_find_cmp(const void *a, const void *b)
{
    const char *str = (const char *)a;
    const family_t *fam = (const family_t *)b;
    return strcmp(str, fam->name);
}

static int family_insert_cmp(const void *a, const void *b)
{
    const family_t *fama = (const family_t *)a;
    const family_t *famb = (const family_t *)b;
    return strcmp(fama->name, famb->name);
}

int family_getsert(mdb_family_t *mdbfam, metric_family_t *mfam)
{
    htable_hash_t hash = htable_hash(mfam->name, HTABLE_HASH_INIT);
    family_t *fam = htable_find(&mdbfam->family_table, hash, mfam->name, family_find_cmp);

    if (fam == NULL) {
        fam = calloc(1, sizeof(*fam));
        if (fam == NULL) {
//            ERROR("calloc failed");
            return -1;
        }
        /* coverity[REVERSE_INULL] */
        if (mfam->name != NULL) {
            fam->name = strdup(mfam->name); // FIXME strpool
            if (fam->name == NULL) {
//                ERROR("strdup failed");
                free(fam);
                return -1;
            }
        }
        if (mfam->help != NULL) {
            fam->help = strdup(mfam->help);
            if (fam->help == NULL) {
//                ERROR("strdup failed");
                free(fam->name);
                free(fam);
                return -1;
            }
        }
        if (mfam->unit != NULL) {
            fam->unit = strdup(mfam->unit); // FIXME strpool ¿?
            if (fam->unit == NULL) {
//                ERROR("strdup failed");
                free(fam->name);
                free(fam->help);
                free(fam);
                return -1;
            }
        }
        fam->type = mfam->type;

        int status = htable_add(&mdbfam->family_table, hash, fam, family_insert_cmp);
        if (status != 0) {
            // ERROR FIXME
            return -1;
        }
    }

    return 0;
}

mdb_family_metric_list_t *family_get_list(mdb_family_t *mdbfam)
{
    mdb_family_metric_list_t *faml = calloc(1, sizeof(*faml));
    if (faml == NULL)
        return NULL;

    faml->num = mdbfam->family_table.used;
    faml->ptr = NULL;

    if (faml->num == 0)
        return faml;

    mdb_family_metric_t *fams = calloc(faml->num, sizeof(*fams));
    if (fams == NULL) {
        free(faml);
        return NULL;
    }

    faml->ptr = fams;

    for (size_t i=0, n=0;  i < mdbfam->family_table.size; i++) {
        if (mdbfam->family_table.tbl[i].data != NULL) {
            family_t *f  = mdbfam->family_table.tbl[i].data;

            fams[n].name = f->name != NULL ? strdup(f->name) : NULL;
            fams[n].help = f->help != NULL ? strdup(f->help) : NULL;
            fams[n].unit = f->unit != NULL ? strdup(f->unit) : NULL;
            fams[n].type = f->type;

            n++;

            if (n == faml->num)
                break;
        }
    }

    return faml;
}
