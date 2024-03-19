/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

typedef enum {
    COLLECT_GLOBALS            = (1 <<  0),
    COLLECT_ACL                = (1 <<  1),
    COLLECT_ARIA               = (1 <<  2),
    COLLECT_BINLOG             = (1 <<  3),
    COLLECT_COMMANDS           = (1 <<  4),
    COLLECT_FEATURES           = (1 <<  5),
    COLLECT_HANDLERS           = (1 <<  6),
    COLLECT_INNODB             = (1 <<  7),
    COLLECT_INNODB_CMP         = (1 <<  8),
    COLLECT_INNODB_CMPMEM      = (1 <<  9),
    COLLECT_INNODB_TABLESPACE  = (1 << 10),
    COLLECT_MYISAM             = (1 << 11),
    COLLECT_PERF_LOST          = (1 << 12),
    COLLECT_QCACHE             = (1 << 13),
    COLLECT_SLAVE              = (1 << 14),
    COLLECT_SSL                = (1 << 15),
    COLLECT_WSREP              = (1 << 16),
    COLLECT_CLIENT_STATS       = (1 << 17),
    COLLECT_USER_STATS         = (1 << 18),
    COLLECT_INDEX_STATS        = (1 << 19),
    COLLECT_TABLE_STATS        = (1 << 20),
    COLLECT_TABLE              = (1 << 21),
    COLLECT_RESPONSE_TIME      = (1 << 22),
    COLLECT_MASTER_STATS       = (1 << 23),
    COLLECT_SLAVE_STATS        = (1 << 24),
    COLLECT_HEARTBEAT          = (1 << 25),
} cmysql_flag_t;
