/* SPDX-License-Identifier: GPL-2.0-only OR MIT                    */
/* SPDX-FileCopyrightText: Copyright (c) 2017-2018 Fabrice Bellard */
/* SPDX-FileContributor: Fabrice Bellard                           */

/* Regular Expression Engine */

#ifndef LIBREGEXP_H
#define LIBREGEXP_H

#include <stddef.h>
#include <stdint.h>

#define LRE_FLAG_GLOBAL     (1 << 0)
#define LRE_FLAG_IGNORECASE (1 << 1)
#define LRE_FLAG_MULTILINE  (1 << 2)
#define LRE_FLAG_DOTALL     (1 << 3)
#define LRE_FLAG_UNICODE    (1 << 4)
#define LRE_FLAG_STICKY     (1 << 5)
#define LRE_FLAG_INDICES    (1 << 6) /* Unused by libregexp, just recorded. */
#define LRE_FLAG_NAMED_GROUPS (1 << 7) /* named groups are present in the regexp */
#define LRE_FLAG_UNICODE_SETS (1 << 8)

#define LRE_RET_MEMORY_ERROR (-1)
#define LRE_RET_TIMEOUT      (-2)

uint8_t *lre_compile(int *plen, char *error_msg, int error_msg_size,
                     const char *buf, size_t buf_len, int re_flags,
                     void *opaque);
int lre_get_capture_count(const uint8_t *bc_buf);
int lre_get_flags(const uint8_t *bc_buf);
const char *lre_get_groupnames(const uint8_t *bc_buf);
int lre_exec(uint8_t **capture,
             const uint8_t *bc_buf, const uint8_t *cbuf, int cindex, int clen,
             int cbuf_type, void *opaque);

int lre_parse_escape(const uint8_t **pp, int allow_utf16);

/* must be provided by the user, return non zero if overflow */
int lre_check_stack_overflow(void *opaque, size_t alloca_size);
/* must be provided by the user, return non zero if time out */
int lre_check_timeout(void *opaque);
void *lre_realloc(void *opaque, void *ptr, size_t size);

#endif /* LIBREGEXP_H */
