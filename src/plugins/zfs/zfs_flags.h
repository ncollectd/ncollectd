/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

enum {
    COLLECT_ABDSTATS          = 1 << 1,
    COLLECT_ARCSTATS          = 1 << 2,
    COLLECT_DBUFSTATS         = 1 << 3,
    COLLECT_DMU_TX            = 1 << 4,
    COLLECT_DNODESTATS        = 1 << 5,
    COLLECT_FM                = 1 << 6,
    COLLECT_QAT               = 1 << 7,
    COLLECT_VDEV_CACHE_STATS  = 1 << 8,
    COLLECT_VDEV_MIRROR_STATS = 1 << 9,
    COLLECT_XUIO_STATS        = 1 << 10,
    COLLECT_ZFETCHSTATS       = 1 << 11,
    COLLECT_ZIL               = 1 << 12,
    COLLECT_STATE             = 1 << 13,
    COLLECT_IO                = 1 << 14,
    COLLECT_OBJSET            = 1 << 15,
};
