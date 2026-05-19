// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) Tobias Waldekranz
// SPDX-FileContributor: Tobias Waldekranz <tobias at waldekranz.com>

// From https://github.com/wkz/ply

#include <errno.h>

#include "ply.h"
#include "internal.h"

int func_pass_ir_post(__attribute__((unused)) const struct func *func, struct node *n,
                      struct ply_probe *pb)
{
    struct node *child = n->expr.args;

    ir_init_sym(pb->ir, n->sym);
    ir_emit_sym_to_sym(pb->ir, n->sym, child->sym);

    return 0;
}

static int func_validate_expr(const struct func *func, struct node *n, int strict)
{
    struct tfield *f;
    struct node *arg;
    int fargs, nargs = 0;

    if (func->type->ttype != T_FUNC) {
        nargs = node_nargs(n);

        if (nargs) {
            fargs = type_nargs(func->type);
            goto too_many;
        }

        return 0;
    }

    for (f = func->type->func.args, arg = n->expr.args;
         f && f->type && arg; f++, arg = arg->next, nargs++) {
        if ((!strict && (f->type->ttype == T_VOID))
            || (!strict && !arg->sym->type)
            || (!strict && (arg->sym->type->ttype == T_VOID))
            || type_compatible(arg->sym->type, f->type))
            continue;

        PLUGIN_ERROR("%s: %s argument to '%s' is of type '%s', expected '%s'.",
                     STRNODELOC(n), STRORDER(nargs), STRNODE(n), STRTYPE(arg->sym->type),
                     STRTYPE(f->type));
    }

    if ((!f || !f->type) && !arg)
        return 0;

    nargs = node_nargs(n);
    fargs = type_nargs(func->type);
    if (f && f->type) {
        PLUGIN_ERROR("%s: too few arguments to %s; expected%s %d, got %d.",
                     STRNODELOC(n), STRNODE(n),
                     func->type->func.vargs? " at least" : "", fargs, nargs);
        return -EINVAL;
    }

    if (func->type->func.vargs)
        return 0;

too_many:
    PLUGIN_ERROR("%s: too many arguments to %s; expected %d, got %d.",
                 STRNODELOC(n), STRNODE(n), fargs, nargs);
    return -EINVAL;
}

int func_static_validate(const struct func *func, struct node *n)
{
    int err = 0;

    if (!func->type)
        goto check_callback;

    switch (n->ntype) {
    case N_EXPR:
        /* if (func->type->ttype != T_FUNC) { */
        /*     _ne(n, "%N is not callable.\n", n); */
        /*     return -EINVAL; */
        /* } */
        err = func_validate_expr(func, n, 0);
        break;

    /* case N_IDENT: */
    /*     if (func->type->ttype == T_FUNC) { */
    /*         _ne(n, "%N is a function.\n", n); */
    /*         return -EINVAL; */
    /*     } */
    /*     break; */

    default:
        /* num, str. nothing to validate. */
        break;
    }

check_callback:
    if (!err && func->static_validate)
        err = func->static_validate(func, n);

    return err;
}

struct type *func_return_type(const struct func *func)
{
    if (!func->type)
        return NULL;

    if (func->type->ttype == T_FUNC)
        return func->type->func.type;

    return func->type;
}
