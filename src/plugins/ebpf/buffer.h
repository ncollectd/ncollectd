/* SPDX-License-Identifier: GPL-2.0-only                              */
/* SPDX-FileCopyrightText: Copyright (C) Tobias Waldekranz            */
/* SPDX-FileContributor: Tobias Waldekranz <tobias at waldekranz.com> */

/* From https://github.com/wkz/ply */

#pragma once

#include <stdint.h>
#include <poll.h>

#include <linux/perf_event.h>

#include <sys/queue.h>

#include "ply.h"

struct buffer_ev {
    struct perf_event_header hdr;
    uint32_t size;

    uint64_t id;
    uint8_t  data[];
} __attribute__((packed));

struct buffer_evh {
    TAILQ_ENTRY(buffer_evh) node;

    uint64_t id;
    void *priv;

    struct ply_return (*handle)(struct buffer_ev *ev, void *priv);
};

void buffer_evh_register(struct buffer_evh *evh);

struct buffer;

struct ply_return buffer_service(struct ply *ply, struct buffer *buf, int ready, struct pollfd *fds);
void buffer_fill_pollset(struct buffer *buf, struct pollfd *fds);
nfds_t buffer_get_nfds(struct buffer *buf);

struct buffer *buffer_new(struct ply *ply, int mapfd);
