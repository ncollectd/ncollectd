/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

enum {
    COLLECT_IP       = 1 << 1,
    COLLECT_ICMP     = 1 << 2,
    COLLECT_UDP      = 1 << 3,
    COLLECT_UDPLITE  = 1 << 4,
    COLLECT_UDPLITE6 = 1 << 5,
    COLLECT_IP6      = 1 << 6,
    COLLECT_ICMP6    = 1 << 7,
    COLLECT_UDP6     = 1 << 8,
    COLLECT_TCP      = 1 << 9,
    COLLECT_MPTCP    = 1 << 10,
    COLLECT_SCTP     = 1 << 11
};
