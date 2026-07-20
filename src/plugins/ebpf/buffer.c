// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) Tobias Waldekranz
// SPDX-FileContributor: Tobias Waldekranz <tobias at waldekranz.com>

// From https://github.com/wkz/ply

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>

#include "ply.h"
#include "internal.h"

#include "built-in.h"

TAILQ_HEAD(buffer_evhs, buffer_evh);
static struct buffer_evhs evhs = TAILQ_HEAD_INITIALIZER(evhs);

static struct buffer_evh *buffer_evh_find(uint64_t id)
{
    struct buffer_evh *evh;

    TAILQ_FOREACH(evh, &evhs, node) {
        if (evh->id == id)
            return evh;
    }

    return NULL;
}

static struct ply_return buffer_evh_call(struct buffer_ev *ev, size_t size)
{
    struct buffer_evh *evh = buffer_evh_find(ev->id);
    if (!evh) {
        PLUGIN_ERROR("unknown event: id:%#"PRIx64" size:%#zx.",
           ev->id, size);
        return (struct ply_return) { .err = 1, .val = ENOSYS };
    }

    return evh->handle(ev, evh->priv);
}

void buffer_evh_register(struct buffer_evh *evh)
{
    static uint64_t next_id = 0;

    evh->id = next_id++;
    TAILQ_INSERT_TAIL(&evhs, evh, node);
}


struct lost_event {
    struct perf_event_header hdr;
    uint64_t id;
    uint64_t lost;
} __attribute__((packed));

struct buffer_q {
    int fd;
    volatile struct perf_event_mmap_page *mem;

    void *buf;
};

struct buffer {
    int mapfd;
    uint32_t ncpus;

    struct pollfd *poll;
    struct buffer_q q[];
};

static inline uint64_t __get_head(volatile struct perf_event_mmap_page *mem)
{
    volatile uint64_t head = mem->data_head;
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#endif
    asm volatile("" ::: "memory");
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    return head;
}

static inline void __set_tail(volatile struct perf_event_mmap_page *mem, uint64_t tail)
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#endif
    asm volatile("" ::: "memory");
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    mem->data_tail = tail;
}

static struct ply_return buffer_q_drain(struct ply *ply, struct buffer_q *q)
{
    struct ply_return ret = {0};

    uint64_t size = q->mem->data_size;
    uint64_t offs = q->mem->data_offset;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    uint8_t *base = (uint8_t *)q->mem + offs;
#pragma GCC diagnostic pop

    struct buffer_ev *ev;
    for (uint64_t head = __get_head(q->mem); q->mem->data_tail != head;
         __set_tail(q->mem, q->mem->data_tail + ev->hdr.size)) {
        uint64_t tail = q->mem->data_tail;

        uint8_t *this = base + (tail % size);
        ev = (void *)this;
        uint8_t *next = base + ((tail + ev->hdr.size) % size);

        if (next < this) {
            size_t left = (base + size) - this;

            q->buf = realloc(q->buf, ev->hdr.size);
            memcpy(q->buf, this, left);
            memcpy((uint8_t *)q->buf + left, base, ev->hdr.size - left);
            ev = q->buf;
        }

        switch (ev->hdr.type) {
        case PERF_RECORD_SAMPLE:
            ret = buffer_evh_call(ev, ev->hdr.size);
            break;
        case PERF_RECORD_LOST: {
            struct lost_event *lost = (void *)ev;

            if (ply->strict) {
                PLUGIN_ERROR("lost %"PRIu64" events.", lost->lost);
                ret.err = 1;
                ret.val = EOVERFLOW;
            } else {
                PLUGIN_WARNING("lost %"PRIu64" events.", lost->lost);
            }
        }   break;
        default:
            PLUGIN_ERROR("unknown perf event %#"PRIx32".", ev->hdr.type);
            ret.err = 1;
            ret.val = EINVAL;
            break;
        }

        if (ret.err || ret.exit)
            break;
    }

    return ret;
}

static int buffer_q_init(struct ply *ply, struct buffer *buf, uint32_t cpu)
{
    struct perf_event_attr attr = { 0 };
    struct buffer_q *q = &buf->q[cpu];

    attr.type          = PERF_TYPE_SOFTWARE;
    attr.config        = PERF_COUNT_SW_BPF_OUTPUT;
    attr.sample_type   = PERF_SAMPLE_RAW;
    attr.wakeup_events = 1;

    q->fd = perf_event_open(&attr, -1, cpu, -1, 0);
    if (q->fd < 0) {
        PLUGIN_ERROR("Could not create queue.");
        return q->fd;
    }

    int err = bpf_map_update(buf->mapfd, &cpu, &q->fd, BPF_ANY);
    if (err) {
        PLUGIN_ERROR("Could not link map to queue.");
        return err;
    }

    size_t size = sysconf(_SC_PAGESIZE) * (ply->buf_pages + 1);
    q->mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, q->fd, 0);
    if (q->mem == MAP_FAILED) {
        PLUGIN_ERROR("Could not mmap queue.");
        return -1;
    }

    buf->poll[cpu].fd     = q->fd;
    buf->poll[cpu].events = POLLIN;
    return 0;
}

struct ply_return buffer_service(struct ply *ply, struct buffer *buf, int ready, struct pollfd *fds)
{
    struct ply_return ret = {0};

    for (uint32_t cpu = 0; ready && (cpu < buf->ncpus); cpu++) {
        if (!(fds[cpu].revents & POLLIN))
            continue;

        ret = buffer_q_drain(ply, &buf->q[cpu]);
        ready--;
        if (ret.err || ret.exit)
            break;
    }

    return ret;
}

void buffer_fill_pollset(struct buffer *buf, struct pollfd *fds)
{
    memcpy(fds, buf->poll, buf->ncpus * sizeof(*fds));
}

nfds_t buffer_get_nfds(struct buffer *buf)
{
    return buf->ncpus;
}

struct buffer *buffer_new(struct ply *ply, int mapfd)
{
    int ncpus = t_buffer.map.len;

    struct buffer *buf = xcalloc(1, sizeof(*buf) + ncpus * sizeof(buf->q[0]));
    if (buf == NULL)
        return NULL;

    buf->mapfd = mapfd;
    buf->ncpus = ncpus;

    buf->poll = xcalloc(ncpus, sizeof(*buf->poll));
    if (buf->poll == NULL) {
        free(buf);
        return NULL;
    }

    for (int cpu = 0; cpu < ncpus; cpu++) {
        int err = buffer_q_init(ply, buf, cpu);
        if (err) {
            free(buf->poll);
            free(buf);
            return NULL;
        }
    }

    return buf;
}

static int stdbuf_static_validate(__attribute__((unused)) const struct func *func, struct node *n)
{
    n->expr.ident = 1;
    return 0;
}

static struct func stdbuf_func = {
    .name = "stdbuf",
    .type = &t_buffer,
    .static_ret = 1,
    .static_validate = stdbuf_static_validate,
};

static int bwrite_ir_post(__attribute__((unused)) const struct func *func, struct node *n,
                          struct ply_probe *pb)
{
    struct node *ctx  = n->expr.args;
    struct node *buf  = ctx->next;
    struct node *data = buf->next;

    ir_emit_perf_event_output(pb->ir, buf->sym, ctx->sym, data->sym);
    return 0;
}

static struct tfield f_bwrite[] = {
    { .type = &t_void },
    { .type = &t_buffer },
    { .type = &t_void },
    { .type = NULL }
};

struct type t_bwrite = {
    .ttype = T_FUNC,
    .func = { .type = &t_void, .args = f_bwrite },
};

static struct func bwrite_func = {
    .name = "bwrite",
    .type = &t_bwrite,
    .static_ret = 1,
    .ir_post = bwrite_ir_post,
};

void buffer_init(void)
{
    built_in_register(&stdbuf_func);
    built_in_register(&bwrite_func);
}
