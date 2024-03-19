// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include <stdio.h>
#include <stdint.h>

#define MIDX_INDEX_MAGIC  0x4e43444d49445849
#define MIDX_INDEX_VERSION 0x01

#define MIDX_DATA_MAGIC 0x4e43444d49445844
#define MIDX_DATA_VERSION 0x01

#define MIDX_DATA_ENTRY_MAGIC 0x4d494458

// __attribute__((packed))

typedef struct __attribute__((packed)) {
    uint64_t magic;
    uint64_t version;
} midx_disk_index_header_t;

typedef struct __attribute__((packed)) {
    uint32_t crc32c;
    uint32_t size;
    uint64_t offset;
} midx_disk_index_entry_t;

typedef struct __attribute__((packed)) {
    uint64_t magic;
    uint64_t version;
} midx_disk_data_header_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t crc32c;
    uint64_t idx;
    uint32_t size;
    uint16_t labels_len;
    uint16_t metric_len;
    uint8_t data[];
} midx_disk_data_entry_t;

typedef struct {
    int fd_idx;
    int fd_data;
} midx_disk_t;

// midx_disk_index_header_t: 16
// midx_disk_index_entry_t: 16
// midx_disk_data_header_t: 16
// midx_disk_data_entry_t: 24

#if 0

midx_disk_open(const char *path);

midx_disk_append(midx_disk_t *midx, metric)

midx_disk_
#endif

"midx.db"
"mdata.db"

/var/lib/ncollectiondb/index.db
/var/lib/ncollectiondb/index.idx

/var/lib/ncollectiondb/data
/var/lib/ncollectiondb/journal

/var/lib/ncollectiondb/index
/var/lib/ncollectiondb/data
/var/lib/ncollectiondb/journal



int main (int argc, char *argv[])
{
    fprintf(stderr, "midx_disk_index_header_t: %d\n", (int)sizeof(midx_disk_index_header_t));
    fprintf(stderr, "midx_disk_index_entry_t: %d\n", (int)sizeof(midx_disk_index_entry_t));
    fprintf(stderr, "midx_disk_data_header_t: %d\n", (int)sizeof(midx_disk_data_header_t));
    fprintf(stderr, "midx_disk_data_entry_t: %d\n", (int)sizeof(midx_disk_data_entry_t));


    int access(const char *pathname, int mode);
    int faccessat(int dirfd, const char *pathname, int mode, int flags);
    openat(dirfd,

    return 0;
}


