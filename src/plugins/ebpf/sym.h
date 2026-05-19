/* SPDX-License-Identifier: GPL-2.0-only                              */
/* SPDX-FileCopyrightText: Copyright (C) Tobias Waldekranz            */
/* SPDX-FileContributor: Tobias Waldekranz <tobias at waldekranz.com> */

/* From https://github.com/wkz/ply */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ir.h"

struct func;
struct node;
struct type;

struct symtab;

typedef void (*sym_free_fn)(void *);

struct sym {
    struct symtab *st;
    const char *name;
    const struct func *func;
    struct type *type;
    struct irstate irs;
    /* TODO: move to priv */
    int mapfd;
    void *priv;
    sym_free_fn free;
};

struct symtab {
    struct sym **syms;
    size_t len;
    unsigned global:1;
};

#define symtab_foreach(_st, _sym) \
    for((_sym) = (_st)->syms; (_sym) < &(_st)->syms[(_st)->len]; (_sym)++)

struct sym *__sym_alloc(struct symtab *st, const char *name, const struct func *func);
struct sym *sym_alloc(struct symtab *st, struct node *n, const struct func *func);

void symtab_reset(struct symtab *st);
void symtab_free(struct symtab *st);

void sym_dump(struct sym *sym, FILE *fp);
void symtab_dump(struct symtab *st, FILE *fp);


static inline int sym_in_reg(struct sym *sym)
{
    return sym->irs.loc == LOC_REG;
}

static inline int sym_on_stack(struct sym *sym)
{
    return sym->irs.loc == LOC_STACK;
}

