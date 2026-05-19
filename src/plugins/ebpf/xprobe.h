/* SPDX-License-Identifier: GPL-2.0-only                              */
/* SPDX-FileCopyrightText: Copyright (C) Tobias Waldekranz            */
/* SPDX-FileContributor: Tobias Waldekranz <tobias at waldekranz.com> */

/* From https://github.com/wkz/ply */

#pragma once

struct xprobe {
    FILE *ctrl;
    const char *ctrl_name;

    char *pattern;
    char stem[0x40];

    size_t n_evs;
    int *evfds;

    char type;
};

int xprobe_detach(struct ply_probe *pb);
int xprobe_attach(struct ply_probe *pb);
