/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

typedef enum {
    COLLECT_BACKEND     = (1 <<  0),
    COLLECT_CACHE       = (1 <<  1),
    COLLECT_CONNECTIONS = (1 <<  2),
    COLLECT_DIRDNS      = (1 <<  3),
    COLLECT_ESI         = (1 <<  4),
    COLLECT_FETCH       = (1 <<  5),
    COLLECT_HCB         = (1 <<  6),
    COLLECT_OBJECTS     = (1 <<  7),
    COLLECT_BANS        = (1 <<  8),
    COLLECT_SESSION     = (1 <<  9),
    COLLECT_SHM         = (1 <<  10),
    COLLECT_SMA         = (1 <<  11),
    COLLECT_SMS         = (1 <<  12),
    COLLECT_STRUCT      = (1 <<  13),
    COLLECT_TOTALS      = (1 <<  14),
    COLLECT_UPTIME      = (1 <<  15),
    COLLECT_VCL         = (1 <<  16),
    COLLECT_WORKERS     = (1 <<  17),
    COLLECT_VSM         = (1 <<  18),
    COLLECT_LCK         = (1 <<  19),
    COLLECT_MEMPOOL     = (1 <<  20),
    COLLECT_MGT         = (1 <<  21),
    COLLECT_SMF         = (1 <<  22),
    COLLECT_VBE         = (1 <<  23),
    COLLECT_MSE         = (1 <<  24),
    COLLECT_GOTO        = (1 <<  25),
    COLLECT_SMU         = (1 <<  26),
    COLLECT_BROTLI      = (1 <<  27),
    COLLECT_ACCG_DIAG   = (1 <<  28),
    COLLECT_ACCG        = (1 <<  29),
    COLLECT_WORKSPACE   = (1 <<  30),
} cvarnish_flag_t;
