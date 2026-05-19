/* SPDX-License-Identifier: GPL-2.0-only                              */
/* SPDX-FileCopyrightText: Copyright (C) Tobias Waldekranz            */
/* SPDX-FileContributor: Tobias Waldekranz <tobias at waldekranz.com> */

/* From https://github.com/wkz/ply */

#pragma once

#include <stdint.h>
#include <stdio.h>

/* symbol information is defined externally */
struct sym;

enum ntype {
    N_EXPR,
    N_NUM,
    N_STRING,
};

/* source location info, this is identical to bison's default YYLTYPE.
 * defining it here means we can keep parser dependencies out of the
 * rest of the code base. */
struct nloc {
    int first_line;
    int first_column;
    int last_line;
    int last_column;
};

struct node {
    struct node *next, *prev, *up;

    struct sym *sym;

    enum ntype ntype;

    union {
        struct {
            char *func;
            struct node *args;
            unsigned ident:1;
        } expr;
        struct {
            union {
                 int64_t s64;
                uint64_t u64;
            };
            unsigned unsignd:1;
            unsigned size:4;
        } num;
        struct {
            char *data;
            unsigned virtual:1;
        } string;
    };

    struct nloc loc;
};

/* debug */

char *strnode(struct node *n, char *buf, size_t buflen);
char *strnode_loc(struct node *n, char *buf, size_t buflen);

#define NODEBUF_SIZE 256
#define STRNODE(n) strnode(n, (char[NODEBUF_SIZE]){0}, NODEBUF_SIZE)
#define STRNODELOC(n) strnode_loc(n, (char[NODEBUF_SIZE]){0}, NODEBUF_SIZE)

typedef int (*nwalk_fn)(struct node *, void *);
int node_walk(struct node *n, nwalk_fn pre, nwalk_fn post, void *ctx);

int node_replace(struct node *n, struct node *new);


/* constructors */
struct node *node_string     (const struct nloc *loc, char *data);
struct node *__node_num      (const struct nloc *loc, size_t size,
                  int64_t *s64, uint64_t *u64);
struct node *node_num        (const struct nloc *loc, char *numstr);
void         node_insert     (struct node *prev, struct node *n);
struct node *node_append     (struct node *head, struct node *tail);
struct node *node_expr_append(const struct nloc *loc, struct node *n, struct node *arg);
struct node *node_expr       (const struct nloc *loc, char *func, ...);
struct node *node_expr_ident (const struct nloc *loc, char *func);

void node_free(struct node *node);

/* helpers */

static inline int node_nargs(struct node *n)
{
    struct node *arg;
    int nargs = 0;

    for (arg = n->expr.args; arg; arg = arg->next, nargs++);

    return nargs;
}

int node_is(struct node *n, const char *func);

#define node_expr_foreach(_expr, _arg) \
    for ((_arg) = (_expr)->expr.args; (_arg); (_arg) = (_arg)->next)

