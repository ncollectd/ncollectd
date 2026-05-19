// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) Tobias Waldekranz
// SPDX-FileContributor: Tobias Waldekranz <tobias at waldekranz.com>

// From https://github.com/wkz/ply

#include <string.h>
#include "internal.h"

struct sym *__sym_alloc(struct symtab *st, const char *name, const struct func *func)
{
    st->syms = realloc(st->syms, ++st->len * sizeof(*st->syms));
    assert(st->syms);

    st->syms[st->len - 1] = xcalloc(1, sizeof(struct sym));
    struct sym *sym = st->syms[st->len - 1];
    sym->st    = st;
    sym->name  = name;
    sym->func  = func;
    sym->mapfd = -1;
    return sym;
}

static struct sym *sym_alloc_ident(struct symtab *st, struct node *n,
                   const struct func *func)
{
    struct sym **sym;

    symtab_foreach(st, sym) {
        if ((*sym)->name && !strcmp((*sym)->name, n->expr.func))
            return *sym;
    }

    return __sym_alloc(st, n->expr.func, func);
}

struct sym *sym_alloc(struct symtab *st, struct node *n, const struct func *func)
{
    if ((n->ntype == N_EXPR) && n->expr.ident)
        return sym_alloc_ident(st, n, func);

    return __sym_alloc(st, NULL, func);
}

void symtab_reset(struct symtab *st)
{
    struct sym **sym;
    symtab_foreach(st, sym) {
        if ((*sym)->free != NULL)
            (*sym)->free((*sym)->priv);
        free(*sym);
    }
    free(st->syms);
    st->syms = NULL;
    st->len = 0;
}

void symtab_free(struct symtab *st)
{   
    struct sym **sym;
    symtab_foreach(st, sym) {
        if ((*sym)->free != NULL)
            (*sym)->free((*sym)->priv);
        free(*sym);
    }
    free(st->syms);
    free(st);
}

void sym_dump(struct sym *sym, FILE *fp)
{
    type_dump(sym->type, sym->name, fp);
}

void symtab_dump(struct symtab *st, FILE *fp)
{
    struct sym **sym;
    symtab_foreach(st, sym) {
        if (!(*sym)->name)
            continue;

        sym_dump(*sym, fp);
        fputc('\n', fp);
    }
}
