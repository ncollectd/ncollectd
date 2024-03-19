// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2017 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "libutils/buf.h"
#include "libutils/itoa.h"
#include "libutils/dtoa.h"

#include <stdint.h>
#include <stdlib.h>

static size_t buf_pagesize(void)
{
    static size_t cached_pagesize;

    if (!cached_pagesize) {
        long tmp = sysconf(_SC_PAGESIZE);
        if (tmp >= 1)
            cached_pagesize = (size_t)tmp;
        else
            cached_pagesize = 1024;
    }

    return cached_pagesize;
}

/* buf_resize resizes an dynamic buffer to ensure that "need" bytes can be
 * stored in it. When called with an empty buffer, i.e. buf->size == 0, it will
 * allocate just enough memory to store need+1 bytes. Subsequent calls will
 * only allocate memory when needed, doubling the allocated memory size each
 * time until the page size is reached, then allocating. */
int buf_resize(buf_t *buf, size_t need)
{
     if (buf->fixed)
         return ENOMEM;

    if (buf_avail(buf) > need)
        return 0;

    size_t new_size;
    if (buf->size == 0) {
        /* New buffers: try to use a reasonable default. */
        new_size = 512;
    } else if (buf->size < buf_pagesize()) {
        /* Small buffers: double the size. */
        new_size = 2 * buf->size;
    } else {
        /* Large buffers: allocate an additional page. */
        size_t pages = (buf->size + buf_pagesize()) / buf_pagesize();
        new_size = (pages + 1) * buf_pagesize();
    }

    /* Check that the new size is large enough. If not, calculate the exact number
     * of bytes needed. */
    if (new_size < (buf->pos + need))
        new_size = buf->pos + need;

    uint8_t *new_ptr = realloc(buf->ptr, new_size);
    if (new_ptr == NULL)
        return ENOMEM;

    buf->ptr = new_ptr;
    buf->size = new_size;

    return 0;
}

void buf_reset2page(buf_t *buf)
{
    if (buf == NULL)
        return;

    if (buf->fixed)
        return;

    buf->pos = 0;

    /* Truncate the buffer to the page size. This is deemed a good compromise
     * between freeing memory (after a large buffer has been constructed) and
     * performance (avoid unnecessary allocations). */
    size_t new_size = buf_pagesize();
    if (buf->size > new_size) {
        uint8_t *new_ptr = realloc(buf->ptr, new_size);
        if (new_ptr != NULL) {
            buf->ptr = new_ptr;
            buf->size = new_size;
        }
    }
}

int buf_putitoa(buf_t *buf, int64_t value)
{
    if (buf_avail(buf) < ITOA_MAX+1) {
        if (buf_resize(buf, ITOA_MAX+1) != 0)
            return ENOMEM;
    }

    size_t len = itoa(value, (char *)(buf->ptr + buf->pos));
    buf->pos += len;
    return 0;
}

int buf_putdtoa(buf_t *buf, double value)
{
    if (buf_avail(buf) < DTOA_MAX) {
        if (buf_resize(buf, DTOA_MAX) != 0)
            return ENOMEM;
    }

    size_t len = dtoa(value, (char *)(buf->ptr + buf->pos), buf_avail(buf));
    buf->pos += len;
    return 0;
}

