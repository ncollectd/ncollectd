// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libexpr/expr.h"

#include <string.h>

#include "libutils/avltree.h"

struct expr_symtab {
    expr_symtab_entry_t default_cb;
    c_avl_tree_t *tree;
};

#if 0
static int expr_id_cmp(const void *a, const void *b)
{
    const expr_id_t *aid = a;
    const expr_id_t *bid = b;

    if (aid == NULL)
        return -1;
    if (bid == NULL)
        return 1;
    if (aid->num != bid->num)
        return aid->num - bid->num;

    for (size_t i = 0; i < aid->num; i++) {
        if (aid->ptr[i].type != bid->ptr[i].type)
            return -1;
        if (aid->ptr[i].type == EXPR_ID_NAME) {
            int cmp = strcmp(aid->ptr[i].name, bid->ptr[i].name);
            if (cmp != 0)
                return cmp;
        } else {
            int cmp = aid->ptr[i].idx - bid->ptr[i].idx;
            if (cmp != 0)
                return cmp;
        }
    }

    return 0;
}
#endif

static int expr_id_cmp(const void *a, const void *b)
{
    const expr_id_t *aid = a;
    const expr_id_t *bid = b;

    if (aid == NULL)
        return -1;
    if (bid == NULL)
        return 1;

    if ((aid->num < 1) || (bid->num < 1))
        return aid->num - bid->num;

    if ((aid->ptr[0].type != EXPR_ID_NAME) || (bid->ptr[0].type != EXPR_ID_NAME))
        return -1;

    return strcmp(aid->ptr[0].name, bid->ptr[0].name);
}

expr_symtab_t *expr_symtab_alloc(void)
{
    expr_symtab_t *symtab = calloc(1, sizeof(*symtab));
    if (symtab == NULL)
        return NULL;

    symtab->tree = c_avl_create(expr_id_cmp);
    if (symtab->tree == NULL) {
        free(symtab);
        return NULL;
    }

    return symtab;
}

static void expr_id_free(expr_id_t *id)
{
    if (id == NULL)
        return;

    if (id->ptr != NULL) {
        for (size_t i = 0; i < id->num; i++) {
            if (id->ptr[i].type == EXPR_ID_NAME)
                free(id->ptr[i].name);
        }
        free(id->ptr);
    }

    free(id);
}

void expr_symtab_entry_free(expr_symtab_entry_t *entry)
{
    if (entry == NULL)
        return;

    expr_id_free(entry->id);

    if ((entry->type == EXPR_SYMTAB_VALUE) && (entry->value.type == EXPR_VALUE_STRING))
        free(entry->value.string);

    free(entry);
}

void expr_symtab_free(expr_symtab_t *symtab)
{
    if (symtab == NULL)
        return;

    if (symtab->tree != NULL) {
        while (true) {
            char *name = NULL;
            expr_symtab_entry_t *entry = NULL;
            int status = c_avl_pick(symtab->tree, (void *)&name, (void *)&entry);
            if (status != 0)
                break;
            expr_symtab_entry_free(entry);
        }
        c_avl_destroy(symtab->tree);
    }

    free(symtab);
}

static expr_id_t *expr_id_clone(expr_id_t *id)
{
    expr_id_t *nid = calloc(1, sizeof(*nid));
    if (nid == NULL) {
        // FIXME ERROR
        return NULL;
    }

    nid->num = id->num;
    nid->ptr = calloc(nid->num, sizeof(nid->ptr[0]));
    if (nid->ptr == NULL) {
        expr_id_free(nid);
        return NULL;
    }

    for (size_t i = 0; i < nid->num; i++) {
        nid->ptr[i].type = id->ptr[i].type;
        if (id->ptr[i].type == EXPR_ID_NAME) {
            nid->ptr[i].name = strdup(id->ptr[i].name);
            if (nid->ptr[i].name == NULL) {
                expr_id_free(nid);
                return NULL;
            }
        } else {
            nid->ptr[i].idx = id->ptr[i].idx;
        }
    }

    return nid;
}

static expr_symtab_entry_t *expr_symtab_entry_alloc(expr_id_t *id, expr_symtab_entry_type_t type)
{
    expr_symtab_entry_t *entry = calloc(1, sizeof(*entry));
    if (entry == NULL)
        return NULL;

    entry->type = type;

    entry->id = expr_id_clone(id);
    if (entry->id == NULL) {
        free(entry);
        return NULL;
    }

    return entry;
}

int expr_symtab_append(expr_symtab_t *symtab, expr_symtab_entry_t *entry)
{
    if ((symtab == NULL) || (entry == NULL))
        return -1;

    expr_symtab_entry_t *lentry = NULL;
    int status = c_avl_get(symtab->tree, entry->id, (void *)&lentry);
    if (status == 0)
        return -1;

    status = c_avl_insert(symtab->tree, entry->id, entry);
    if (status != 0)
        return -1;

    return 0;
}

expr_symtab_entry_t *expr_symtab_lookup(expr_symtab_t *symtab, expr_id_t *id)
{
    if ((symtab == NULL) || (id == NULL))
        return NULL;

    expr_symtab_entry_t *entry = NULL;
    int status = c_avl_get(symtab->tree, id, (void *)&entry);
    if (status == 0)
        return entry;

    if (symtab->default_cb.cb != NULL)
        return &symtab->default_cb;

    return NULL;
}

int expr_symtab_append_number(expr_symtab_t *symtab, expr_id_t *id, double number)
{
    expr_symtab_entry_t *entry = expr_symtab_entry_alloc(id, EXPR_SYMTAB_VALUE);
    if (entry == NULL)
        return -1;

    entry->value.number = number;

    int status = expr_symtab_append(symtab, entry);
    if (status != 0) {
        expr_symtab_entry_free(entry);
        return -1;
    }

    return 0;
}

int expr_symtab_append_boolean(expr_symtab_t *symtab, expr_id_t *id, bool boolean)
{
    expr_symtab_entry_t *entry = expr_symtab_entry_alloc(id, EXPR_SYMTAB_VALUE);
    if (entry == NULL)
        return -1;

    entry->value.boolean = boolean;

    int status = expr_symtab_append(symtab, entry);
    if (status != 0) {
        expr_symtab_entry_free(entry);
        return -1;
    }

    return 0;
}

int expr_symtab_append_string(expr_symtab_t *symtab, expr_id_t *id, char *str)
{
    expr_symtab_entry_t *entry = expr_symtab_entry_alloc(id, EXPR_SYMTAB_VALUE);
    if (entry == NULL)
        return -1;

    entry->value.string = strdup(str);
    if (entry->value.string == NULL) {
        expr_symtab_entry_free(entry);
        return -1;
    }

    int status = expr_symtab_append(symtab, entry);
    if (status != 0) {
        expr_symtab_entry_free(entry);
        return -1;
    }

    return 0;
}

int expr_symtab_append_value(expr_symtab_t *symtab, expr_id_t *id, expr_value_t *value)
{
    expr_symtab_entry_t *entry = expr_symtab_entry_alloc(id, EXPR_SYMTAB_VALUE_REF);
    if (entry == NULL)
        return -1;

    entry->value_ref = value;

    int status = expr_symtab_append(symtab, entry);
    if (status != 0) {
        expr_symtab_entry_free(entry);
        return -1;
    }

    return 0;
}

int expr_symtab_append_name_value(expr_symtab_t *symtab, char *name,  expr_value_t *value)
{
    expr_id_item_t id_item = {.type = EXPR_ID_NAME, .name = name};
    expr_id_t id = {.num = 1, .ptr = &id_item};

    return expr_symtab_append_value(symtab, &id, value);
}

int expr_symtab_append_callback(expr_symtab_t *symtab, expr_id_t *id, expr_symtab_cb_t cb, void *data)
{
    expr_symtab_entry_t *entry = expr_symtab_entry_alloc(id, EXPR_SYMTAB_CALLBACK);
    if (entry == NULL)
        return -1;

    entry->cb = cb;
    entry->data = data;

    int status = expr_symtab_append(symtab, entry);
    if (status != 0) {
        expr_symtab_entry_free(entry);
        return -1;
    }

    return 0;
}

int expr_symtab_default(expr_symtab_t *symtab, expr_symtab_cb_t cb, void *data)
{
    symtab->default_cb.cb = cb;
    symtab->default_cb.data = data;

    return 0;
}
