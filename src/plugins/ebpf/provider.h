/* SPDX-License-Identifier: GPL-2.0-only                              */
/* SPDX-FileCopyrightText: Copyright (C) Tobias Waldekranz            */
/* SPDX-FileContributor: Tobias Waldekranz <tobias at waldekranz.com> */

/* From https://github.com/wkz/ply */

#pragma once

#include <linux/bpf.h>

#include <sys/queue.h>

struct ply;
struct ply_probe;
struct node;

struct provider {
    const char *name;
    enum bpf_prog_type prog_type;

    SLIST_ENTRY(provider) entry;

    int (*probe)    (struct ply_probe *);
    int (*sym_alloc)(struct ply_probe *, struct node *);
    int (*ir_pre)   (struct ply_probe *);
    int (*ir_post)  (struct ply_probe *);
    int (*attach)   (struct ply_probe *);
    int (*start)    (struct ply_probe *);
    int (*stop)     (struct ply_probe *);
    int (*detach)   (struct ply_probe *);
};

struct provider *provider_get(const char *name);
void provider_init(void);

