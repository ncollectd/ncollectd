// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "libutils/htable.h"
#include "libutils/strbuf.h"
#include "libutils/dtoa.h"
#include "libutils/strlist.h"
#include "libmetric/metric.h"
#include "libmetric/metric_match.h"
#include "libmdb/metric_id.h"
#include "libmdb/rindex.h"

#include <regex.h>

#define HTABLE_FAMILY_SIZE      256
#define HTABLE_NAME_SIZE        256
#define HTABLE_LABEL_SIZE       4
#define HTABLE_LABEL_VALUE_SIZE 4


#if 0
static int rindex_append_value(__attribute__((unused)) rindex_metric_t *mcm,
                               __attribute__((unused)) cdtime_t interval,
                               __attribute__((unused)) double value,
                               __attribute__((unused)) cdtime_t time)
{

// FIXME
    return 0;
}
#endif

static void rindex_label_value_free(void *arg, __attribute__((unused)) void *unused)
{
    rindex_label_value_t *mclv = arg;

    if (mclv == NULL)
        return;

    free(mclv->lvalue);
    metric_id_set_destroy(&mclv->ids);
    free(mclv);
}

static int rindex_label_value_find_cmp(const void *a, const void *b)
{
    const char *str = (const char *)a;
    const rindex_label_value_t *mclv = (const rindex_label_value_t *)b;
    return strcmp(str, mclv->lvalue);
}

static int rindex_label_value_insert_cmp(const void *a, const void *b)
{
    const rindex_label_value_t *mclva = (const rindex_label_value_t *)a;
    const rindex_label_value_t *mclvb = (const rindex_label_value_t *)b;
    return strcmp(mclva->lvalue, mclvb->lvalue);
}

static inline rindex_label_value_t *rindex_label_value_get(htable_t *hvalue, char *value)
{
    htable_hash_t hash = htable_hash(value, HTABLE_HASH_INIT);
    return htable_find(hvalue, hash, value, rindex_label_value_find_cmp);
}

static rindex_label_value_t *rindex_label_value_getsert(htable_t *hvalue, char *value)
{
    htable_hash_t hash = htable_hash(value, HTABLE_HASH_INIT);
    rindex_label_value_t *mclv = htable_find(hvalue, hash, value, rindex_label_value_find_cmp);
    if (mclv == NULL) {
        mclv = calloc(1, sizeof(*mclv));
        if (mclv == NULL) {
//            ERROR("calloc failed");
            return NULL;
        }

        mclv->lvalue = strdup(value);
        if (mclv->lvalue == NULL) {
//            ERROR("strdup failed");
            free(mclv);
            return NULL;
        }

        int status = htable_add(hvalue, hash, mclv, rindex_label_value_insert_cmp);
        if (status != 0) {
            // ERROR FIXME
            return NULL;
        }
    }

    return mclv;
}

static void rindex_label_free(void *arg, __attribute__((unused)) void *unused)
{
    rindex_label_t *mcl = arg;

    if (arg == NULL)
        return;

    free(mcl->lname);
    htable_destroy(&mcl->values, rindex_label_value_free, NULL);
    metric_id_set_destroy(&mcl->ids);
    free(mcl);
}

static int rindex_label_find_cmp(const void *a, const void *b)
{
    const char *str = (const char *)a;
    const rindex_label_t *mcl = (const rindex_label_t *)b;
    return strcmp(str, mcl->lname);
}

static int rindex_label_insert_cmp(const void *a, const void *b)
{
    const rindex_label_t *mcla = (const rindex_label_t *)a;
    const rindex_label_t *mclb = (const rindex_label_t *)b;
    return strcmp(mcla->lname, mclb->lname);
}

static inline rindex_label_t *rindex_label_get(htable_t *hlabel, char *name)

{
    htable_hash_t hash = htable_hash(name, HTABLE_HASH_INIT);
    return htable_find(hlabel, hash, name, rindex_label_find_cmp);
}

static rindex_label_t *rindex_label_getsert(htable_t *hlabel, char *name)
{
    htable_hash_t hash = htable_hash(name, HTABLE_HASH_INIT);
    rindex_label_t *mcl = htable_find(hlabel, hash, name, rindex_label_find_cmp);
    if (mcl == NULL) {
        mcl = calloc(1, sizeof(*mcl));
        if (mcl == NULL) {
//            ERROR("calloc failed");
            return NULL;
        }

        mcl->lname = strdup(name);
        if (mcl->lname == NULL) {
//            ERROR("strdup failed");
            free(mcl);
            return NULL;
        }

        htable_init(&mcl->values, HTABLE_LABEL_VALUE_SIZE); // FIXME error

        int status = htable_add(hlabel, hash, mcl, rindex_label_insert_cmp);
        if (status != 0) {
            // ERROR FIXME
            return NULL;
        }
    }

    return mcl;
}

static void rindex_name_free(void *arg, __attribute__((unused)) void *unused)
{
    rindex_name_t *mcm = arg;

    if (mcm == NULL)
        return;

    free(mcm->name);
    htable_destroy(&mcm->labels, rindex_label_free, NULL);
    metric_id_set_destroy(&mcm->ids);
    free(mcm);
}

static int rindex_name_find_cmp(const void *a, const void *b)
{
    const char *str = (const char *)a;
    const rindex_name_t *mcm = (const rindex_name_t *)b;
    return strcmp(str, mcm->name);
}

static int rindex_name_insert_cmp(const void *a, const void *b)
{
    const rindex_name_t *mcma = (const rindex_name_t *)a;
    const rindex_name_t *mcmb = (const rindex_name_t *)b;
    return strcmp(mcma->name, mcmb->name);
}

static inline rindex_name_t *rindex_name_get(rindex_t *mc, char *name)
{
    htable_hash_t hash = htable_hash(name, HTABLE_HASH_INIT);
    return htable_find(&mc->name_table, hash, name, rindex_name_find_cmp);
}

static rindex_name_t *rindex_name_getsert(rindex_t *mc, const char *name)
{
    htable_hash_t hash = htable_hash(name, HTABLE_HASH_INIT);
    rindex_name_t *mcn = htable_find(&mc->name_table, hash, name, rindex_name_find_cmp);

    if (mcn == NULL) {
        mcn = calloc(1, sizeof(*mcn));
        if (mcn == NULL) {
//            ERROR("calloc failed");
            return NULL;
        }
        mcn->name = strdup(name); // FIXME strpool
        if (mcn->name == NULL) {
//            ERROR("strndup failed");
            free(mcn);
            return NULL;
        }
        htable_init(&mcn->labels, HTABLE_LABEL_SIZE); // FIXME error

        int status = htable_add(&mc->name_table, hash, mcn, rindex_name_insert_cmp);
        if (status != 0) {
            // ERROR FIXME
            return NULL;
        }
    }

    return mcn;
}

#if 0
static void rindex_metric_free(void *arg)
{
    rindex_metric_t *mcm = arg;

    if (mcm == NULL)
        return;

    free(mcm->name);
    label_set_reset(&mcm->label);
    // rindex_value_list_reset(&mcm->value); // FIXME
    free(mcm);
}
#endif

int rindex_insert(rindex_t *rindex, metric_id_t id, const char *metric, const label_set_t *label)
{
    rindex_name_t *mcn = rindex_name_getsert(rindex, metric);
    if (mcn == NULL)
        return -1;

    // Insert mcm in mcf
    int status = metric_id_set_insert(&mcn->ids, id);
    if (status != 0) {
        return -1;
    }

    for (size_t n = 0 ; n  < label->num; n++) {
        label_pair_t *pair = &label->ptr[n];

        rindex_label_t *mcl = rindex_label_getsert(&mcn->labels, pair->name);
        if (mcl == NULL)
            return -1;

        status = metric_id_set_insert(&mcl->ids, id);
        if (status != 0) {
            return -1;
        }

        rindex_label_value_t *mclv = rindex_label_value_getsert(&mcl->values, pair->value);
        if (mclv == NULL)
            return -1;

        status = metric_id_set_insert(&mclv->ids, id);
        if (status != 0) {
            return -1;
        }
    }

    return 0;
}

#if 0
static int rindex_insert_value(rindex_t *mc, metric_family_t *fam, metric_t *m,
                                     char *metric_name, size_t metric_name_len,
                                     label_set_t *labels, double value)
{
    rindex_metric_t *mcm = rindex_get(mc, fam, metric_name, metric_name_len, labels);
    if (mcm == NULL) {
        return ENOENT;
    }

    int status = rindex_append_value(mcm, m->interval, value, m->time);

    return status;
}
#endif


int rindex_init(rindex_t *rindex)
{
    htable_init(&rindex->name_table, HTABLE_NAME_SIZE);
//    htable_init(&rindex->family_table, HTABLE_FAMILY_SIZE);
//    htable_init(&rindex->metric_table, HTABLE_METRIC_SIZE);
    return 0;
}

int rindex_destroy(rindex_t *rindex)
{
    htable_destroy(&rindex->name_table, rindex_name_free, NULL);
//    htable_destroy(&rindex->family_table, rindex_family_free);
//    htable_destroy(&rindex->metric_table, rindex_metric_free);
    return 0;
}

strlist_t *rindex_get_metrics(rindex_t *rindex)
{
    strlist_t *sl = strlist_alloc(rindex->name_table.used);
    if (sl == NULL)
        return NULL;

    if (rindex->name_table.used == 0)
        return sl;

    for (size_t i=0;  i < rindex->name_table.size; i++) {
        rindex_name_t *mn = rindex->name_table.tbl[i].data;
        if ((mn != NULL) && (mn->name != NULL)) {
            strlist_append(sl, mn->name);
        }
    }

    return sl;
}

strlist_t *rindex_get_metric_labels(rindex_t *rindex, char *metric)
{
    htable_hash_t hash = htable_hash(metric, HTABLE_HASH_INIT);
    rindex_name_t *mn = htable_find(&rindex->name_table, hash, metric, rindex_name_find_cmp);
    if (mn == NULL)
        return NULL;


    strlist_t *sl = strlist_alloc(mn->labels.used);
    if (sl == NULL)
        return NULL;

    if (mn->labels.used == 0)
        return sl;

    for (size_t i = 0;  i < mn->labels.size;  i++) {
        rindex_label_t *ml = mn->labels.tbl[i].data;
        if ((ml != NULL) && (ml->lname != NULL))
            strlist_append(sl, ml->lname);
    }

    return sl;
}

strlist_t *rindex_get_metric_label_value(rindex_t *rindex, char *metric, char *label)
{
    htable_hash_t hash = htable_hash(metric, HTABLE_HASH_INIT);
    rindex_name_t *mn = htable_find(&rindex->name_table, hash, metric, rindex_name_find_cmp);
    if (mn == NULL)
        return NULL;

    hash = htable_hash(label, HTABLE_HASH_INIT);
    rindex_label_t *ml = htable_find(&mn->labels, hash, label,  rindex_label_find_cmp);
    if (ml == NULL)
        return NULL;

    strlist_t *sl = strlist_alloc(mn->labels.used);
    if (sl == NULL)
        return NULL;

    if (ml->values.used == 0)
        return sl;

    for (size_t i = 0;  i < ml->values.size;  i++) {
        rindex_label_value_t *mlv =  ml->values.tbl[i].data;
        if ((mlv != NULL) && (mlv->lvalue != NULL))
            strlist_append(sl, mlv->lvalue);
    }

    return sl;
}

static inline int rindex_id_intersect(metric_id_set_t *a, metric_id_set_t *b)
{
    if (metric_id_size(a) == 0) {
        metric_id_set_clone(a, b);
    } else {
        metric_id_set_t c = {0};
        metric_id_set_swap(&c, a);
        metric_id_set_intersect(a, &c, b);
        metric_id_set_destroy(&c);
    }
    return 0;
}

static int rindex_match_metric_labels(metric_id_set_t *result, rindex_name_t *mcm,
                                                               metric_match_set_t *match)
{
    if (match == NULL)
        return 0;

    for (size_t i = 0; i < match->num; i++) {
        metric_match_pair_t *pair = match->ptr[i];
        if (pair == NULL)
            continue;

        rindex_label_t *mcl = rindex_label_get(&mcm->labels, pair->name);

        switch (pair->op) {
        case METRIC_MATCH_OP_NONE:
            break;
        case METRIC_MATCH_OP_EQL:
            if (mcl == NULL) {
                metric_id_set_destroy(result);
            } else {
                rindex_label_value_t *mclv = rindex_label_value_get(&mcl->values,
                                                                    pair->value.string);
                if (mclv == NULL) {
                    metric_id_set_destroy(result);
                } else {
                    rindex_id_intersect(result, &mclv->ids);
                }
            }
            break;
        case METRIC_MATCH_OP_NEQ:
            if (mcl != NULL) {
                rindex_label_value_t *mclv = rindex_label_value_get(&mcl->values,
                                                                    pair->value.string);
                if (mclv != NULL) {
                    metric_id_set_t diff = {0};
                    metric_id_set_difference(&diff, &mcl->ids, &mclv->ids);
                    rindex_id_intersect(result, &diff);
                    metric_id_set_destroy(&diff);
                }
            }
            break;
        case METRIC_MATCH_OP_EQL_REGEX:
            if (mcl == NULL) {
                metric_id_set_destroy(result);
            } else {
                metric_id_set_t dst = {0};
                metric_id_set_t cresult = {0};
                for (size_t j=0; j < mcl->values.size; j++) {
                    if (mcl->values.tbl[j].data != NULL) {
                        rindex_label_value_t *mclv = mcl->values.tbl[j].data;
                        if (regexec(pair->value.regex, mclv->lvalue, 0, NULL, REG_NOSUB)) {
                            metric_id_set_clone(&cresult, &dst);
                            metric_id_set_union(&dst, &cresult, &mclv->ids);
                            metric_id_set_destroy(&cresult);
                        }
                    }
                }
                rindex_id_intersect(result, &dst);
                metric_id_set_destroy(&dst);
            }
            break;
        case METRIC_MATCH_OP_NEQ_REGEX:
            if (mcl != NULL) {
                metric_id_set_t dst = {0};
                metric_id_set_t cresult = {0};
                for (size_t j=0; j < mcl->values.size; j++) {
                    if (mcl->values.tbl[j].data != NULL) {
                        rindex_label_value_t *mclv = mcl->values.tbl[j].data;
                        if (!regexec(pair->value.regex, mclv->lvalue, 0, NULL, REG_NOSUB)) {
                            metric_id_set_clone(&cresult, &dst);
                            metric_id_set_union(&dst, &cresult, &mclv->ids);
                            metric_id_set_destroy(&cresult);
                        }
                    }
                }
                if (metric_id_size(&dst) > 0) {
                    metric_id_set_t diff = {0};
                    metric_id_set_difference(&diff, &mcl->ids, &dst);
                    rindex_id_intersect(result, &diff);
                    metric_id_set_destroy(&diff);
                }
                metric_id_set_destroy(&dst);
            }
            break;
        case METRIC_MATCH_OP_EXISTS:
            if (mcl == NULL) {
                metric_id_set_destroy(result);
            } else {
                rindex_id_intersect(result, &mcl->ids);
            }
            break;
        case METRIC_MATCH_OP_NEXISTS:
            if (mcl != NULL) {
                metric_id_set_t diff = {0};
                metric_id_set_difference(&diff, result, &mcl->ids);
                rindex_id_intersect(result, &diff);
                metric_id_set_destroy(&diff);
            }
            break;
        }

        if (metric_id_size(result) == 0)
            break;
    }

    return 0;
}

int rindex_search(rindex_t *rindex, metric_id_set_t *result, const metric_match_t *match)
{
    if (result == NULL)
        return 0;

    metric_id_set_destroy(result);

    if ((match->name == NULL) || (match->name->num == 0))
        return 0;

    if ((match->name->num == 1) && (match->name->ptr[0]->op == METRIC_MATCH_OP_EQL)) {
        rindex_name_t *mcm = rindex_name_get(rindex, match->name->ptr[0]->value.string);
        if (mcm == NULL) {
            metric_id_set_destroy(result);
        } else {
            rindex_match_metric_labels(result, mcm, match->labels);
        }
        return 0;
    }

    for (size_t i = 0; i < match->name->num; i++) {
        metric_match_pair_t *pair = match->name->ptr[i];
        if (pair == NULL)
            continue;
        switch(pair->op) {
        case METRIC_MATCH_OP_NONE:
            break;
        case METRIC_MATCH_OP_EQL:
            // FIXME
            break;
        case METRIC_MATCH_OP_NEQ:
            // FIXME
            break;
        case METRIC_MATCH_OP_EQL_REGEX:
            // FIXME
            break;
        case METRIC_MATCH_OP_NEQ_REGEX:
            // FIXME
            break;
        case METRIC_MATCH_OP_EXISTS:
            // FIXME
            break;
        case METRIC_MATCH_OP_NEXISTS:
            // FIXME
            break;
        }
    }

    return 0;
}
