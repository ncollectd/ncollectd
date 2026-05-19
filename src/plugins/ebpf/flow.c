// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) Tobias Waldekranz
// SPDX-FileContributor: Tobias Waldekranz <tobias at waldekranz.com>

// From https://github.com/wkz/ply

#include <assert.h>
#include <errno.h>

#include "ply.h"
#include "internal.h"

#include "built-in.h"


struct if_priv {
    unsigned rewritten;

    int16_t miss_label;
    int16_t end_label;
};


static int iftest_ir_post(__attribute__((unused)) const struct func *func, struct node *n,
                 struct ply_probe *pb)
{
    struct node *ifn = n->up;
    struct if_priv *ifp = ifn->sym->priv;
    int reg;

    struct node *expr = n->prev;
    struct node *estmt = n->next->next->next;

    if (expr->sym->irs.loc == LOC_REG) {
        reg = expr->sym->irs.reg;
    } else {
        reg = BPF_REG_0;
        ir_emit_sym_to_reg(pb->ir, reg, expr->sym);
    }

    ifp->miss_label = ir_alloc_label(pb->ir);
    if (estmt)
        ifp->end_label = ir_alloc_label(pb->ir);

    ir_emit_insn(pb->ir, JMP_IMM(BPF_JEQ, 0, ifp->miss_label), reg, 0);
    return 0;
}

static struct func iftest_func = {
    .name = ":iftest",
    .type = &t_void,
    .static_ret = 1,

    .ir_post = iftest_ir_post,
};


static int ifjump_ir_post(__attribute__((unused)) const struct func *func, struct node *n,
                 struct ply_probe *pb)
{
    struct node *ifn = n->up;
    struct if_priv *ifp = ifn->sym->priv;

    if (ifp->end_label)
        ir_emit_insn(pb->ir, JMP_IMM(BPF_JA, 0, ifp->end_label), 0, 0);

    ir_emit_label(pb->ir, ifp->miss_label);
    return 0;
}

static struct func ifjump_func = {
    .name = ":ifjump",
    .type = &t_void,
    .static_ret = 1,

    .ir_post = ifjump_ir_post,
};


static int if_ir_post(__attribute__((unused)) const struct func *func, struct node *n,
                 struct ply_probe *pb)
{
    struct if_priv *ifp = n->sym->priv;

    if (ifp->end_label)
        ir_emit_label(pb->ir, ifp->end_label);
    return 0;
}

static int if_rewrite(__attribute__((unused)) const struct func *func, struct node *n,
                __attribute__((unused)) struct ply_probe *pb)
{
    struct if_priv *ifp = n->sym->priv;

    if (ifp->rewritten)
        return 0;

    struct node *expr = n->expr.args;
    struct node *stmt = expr->next;

    node_insert(expr, node_expr(&n->loc, ":iftest", NULL));    
    node_insert(stmt, node_expr(&n->loc, ":ifjump", NULL));
    ifp->rewritten = 1;
    return 0;
}

static void if_type_free(void *data)
{
    if (data == NULL)
        return;

    free(data);
}

static int if_type_infer(__attribute__((unused)) const struct func *func, struct node *n,
                         __attribute__((unused)) struct ply_probe *pb) 
{
    struct node *expr = n->expr.args;

    if (!n->sym->priv) {
        n->sym->priv = xcalloc(1, sizeof(struct if_priv));
        n->sym->free = if_type_free;
    }

    if (!expr->sym->type)
        return 0;

    if (type_base(expr->sym->type)->ttype != T_SCALAR) {
        PLUGIN_ERROR("%s: condition of '%s' must be a scalar value, "
                     "but '%s' is of type '%s'.", STRNODELOC(expr), STRNODE(n), STRNODE(expr),
                     STRTYPE(expr->sym->type));
        return -EINVAL;
    }

    n->sym->type = &t_void;
    return 0;
}

static struct func if_func = {
    .name = "if",
    .type = &t_vargs_func,
    .static_ret = 1,
    .type_infer = if_type_infer,
    .rewrite = if_rewrite,

    .ir_post = if_ir_post,
};


static struct ply_return exit_ev_handler(struct buffer_ev *ev, __attribute__((unused)) void *_null)
{
    int *code = (void *)ev->data;

    PLUGIN_DEBUG("Exit:%d", *code);
    return (struct ply_return){ .val = *code, .exit = 1 };
}

static int exit_rewrite(__attribute__((unused)) const struct func *func, struct node *n,
                        __attribute__((unused)) struct ply_probe *pb)
{
    struct buffer_evh *evh = n->sym->priv;
    uint64_t id = evh->id;

    struct node *expr = n->expr.args;
    n->expr.args = NULL;

    struct node *ev = node_expr(&n->loc, ":struct",
                                __node_num(&n->loc, sizeof(evh->id), NULL, &id),
                                node_expr(&n->loc, ":struct", expr, NULL),
                                NULL);

    struct node *bwrite = node_expr(&n->loc, "bwrite",
                                    node_expr(&n->loc, "ctx", NULL),
                                    node_expr(&n->loc, "stdbuf", NULL),
                                    ev,
                                    NULL);

    node_replace(n, bwrite);
    return 1;
}

static int exit_type_infer(__attribute__((unused)) const struct func *func, struct node *n,
                           __attribute__((unused)) struct ply_probe *pb)
{
    struct node *expr = n->expr.args;

    if (n->sym->type)
        return 0;

    if (!expr->sym->type)
        return 0;

    if (type_base(expr->sym->type)->ttype != T_SCALAR) {
        PLUGIN_ERROR("%s: argument to '%s' must be a scalar value, "
                     "but '%s' is of type '%s'.", STRNODELOC(expr), STRNODE(n), STRNODE(expr),
                     STRTYPE(expr->sym->type));
        return -EINVAL;
    }

    struct buffer_evh *evh = xcalloc(1, sizeof(*evh));

    evh->handle = exit_ev_handler;
    buffer_evh_register(evh);

    /* TODO: leaked */
    n->sym->priv = evh;
    n->sym->type = &t_void;
    return 0;
}

static struct func exit_func = {
    .name = "exit",
    .type = &t_unary_func,
    .type_infer = exit_type_infer,

    .rewrite = exit_rewrite,
};

void flow_init(void)
{
    built_in_register(&iftest_func);
    built_in_register(&ifjump_func);
    built_in_register(&if_func);
    built_in_register(&exit_func);
}
