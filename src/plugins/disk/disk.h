/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

enum {
    FAM_DISK_READ_BYTES,
    FAM_DISK_READ_MERGED,
    FAM_DISK_READ_OPS,
    FAM_DISK_READ_TIME,
    FAM_DISK_READ_WEIGHTED_TIME,
    FAM_DISK_READ_TIMEOUT,
    FAM_DISK_READ_FAILED,
    FAM_DISK_WRITE_BYTES,
    FAM_DISK_WRITE_MERGED,
    FAM_DISK_WRITE_OPS,
    FAM_DISK_WRITE_TIME,
    FAM_DISK_WRITE_WEIGHTED_TIME,
    FAM_DISK_WRITE_TIMEOUT,
    FAM_DISK_WRITE_FAILED,
    FAM_DISK_IO_TIME,
    FAM_DISK_IO_WEIGHTED_TIME,
    FAM_DISK_PENDING_OPERATIONS,
    FAM_DISK_DISCARD_BYTES,
    FAM_DISK_DISCARD_MERGED,
    FAM_DISK_DISCARD_OPS,
    FAM_DISK_DISCARD_TIME,
    FAM_DISK_DISCARD_WEIGHTED_TIME,
    FAM_DISK_FLUSH_OPS,
    FAM_DISK_FLUSH_TIME,
    FAM_DISK_FLUSH_WEIGHTED_TIME,
    FAM_DISK_MAX,
};
