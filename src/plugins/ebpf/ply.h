/* SPDX-License-Identifier: GPL-2.0-only                              */
/* SPDX-FileCopyrightText: Copyright (C) Tobias Waldekranz            */
/* SPDX-FileContributor: Tobias Waldekranz <tobias at waldekranz.com> */

/* From https://github.com/wkz/ply */

#pragma once

#include <stdio.h>
#include <poll.h>

#include "sym.h"
#include "utils.h"

struct ksyms;
struct ply;
struct node;
struct ir;

struct ply_return {
    int val;
    unsigned err:1;
    unsigned exit:1;
};

static inline void ply_return_fold(struct ply_return *ret, struct ply_return new)
{
    if (ret->err)
        return;

    if (new.err) {
        *ret = new;
        return;
    }

    if (ret->exit)
        return;

    *ret = new;
}

/* api */
struct ply_probe {
    struct ply_probe *next, *prev;
    struct ply *ply;

    char *probe;
    struct node *ast;

    struct symtab locals;

    struct provider *provider;
    void *provider_data;

    struct ir *ir;
    int bpf_fd;
    int special;
};

struct ply {
    size_t map_elems;
    size_t string_size;
    size_t buf_pages;   /* number of memory pages, per-cpu, per buffer */
    size_t stack_depth;
    bool sort;          /* sort maps before output, requires more memory. */
    bool ksyms_cache;   /* create ksyms cache. */
    bool strict;        /* abort on error. */
    bool verify;        /* capture verifier output, uses 16M of memory. */
    struct sym *stdbuf;
    struct ply_probe *probes;
    struct symtab globals;
    struct ksyms *ksyms;
    char *group;
    int   group_fd;
};

#define ply_probe_foreach(_ply, _probe)                    \
    for ((_probe) = (_ply)->probes;    (_probe); (_probe) = (_probe)->next)

static inline struct ply_probe *sym_to_probe(struct sym *sym)
{
    if (sym->st->global)
        return NULL;

    return container_of(sym->st, struct ply_probe, locals);
}

void ply_maps_print(struct ply *ply);
void ply_map_print(struct ply *ply, struct sym *sym, FILE *fp);
void ply_map_clear(struct ply *ply, struct sym *sym);

struct ply_return ply_service(struct ply *ply, int ready, struct pollfd *fds);
nfds_t ply_get_nfds(struct ply *ply);
void ply_fill_pollset(struct ply *ply, struct pollfd *fds);

int ply_start(struct ply *ply);
int ply_stop(struct ply *ply);

struct ply_return ply_load(struct ply *ply);
int ply_unload(struct ply *ply);

int ply_add_probe(struct ply *ply, struct ply_probe *probe);
int ply_compile(struct ply *ply);


int  ply_fparse(struct ply *ply, FILE *fp);
int  ply_parsef(struct ply *ply, const char *fmt, ...);
void ply_free  (struct ply *ply);
struct ply *ply_alloc(char *group, bool ksyms_cache);

void ply_init(void);

