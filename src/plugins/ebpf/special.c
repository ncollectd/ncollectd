// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C)  Namhyung Kim
// SPDX-FileContributor: Namhyung Kim <namhyung at gmail.com>

// From https://github.com/wkz/ply

#include <errno.h>

#include "ply.h"
#include "internal.h"

static int special_exec(struct ply_probe *pb)
{
    return bpf_prog_test_run(pb->bpf_fd);
}

static int special_sym_alloc(__attribute__((unused)) struct ply_probe *pb,
                             __attribute__((unused)) struct node *n)
{
    return -ENOENT;
}

static int special_probe(struct ply_probe *pb)
{
    pb->special = 1;
    return 0;
}

struct provider begin_provider = {
    .name = "BEGIN",
    .prog_type = BPF_PROG_TYPE_RAW_TRACEPOINT,
    .probe     = special_probe,
    .sym_alloc = special_sym_alloc,
    .start     = special_exec,
};

struct provider end_provider = {
    .name = "END",
    .prog_type = BPF_PROG_TYPE_RAW_TRACEPOINT,
    .probe     = special_probe,
    .sym_alloc = special_sym_alloc,
    .stop      = special_exec,
};
