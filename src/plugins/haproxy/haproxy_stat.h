/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

enum {
    HA_STAT_PXNAME,
    HA_STAT_SVNAME,
    HA_STAT_QCUR,
    HA_STAT_QMAX,
    HA_STAT_SCUR,
    HA_STAT_SMAX,
    HA_STAT_SLIM,
    HA_STAT_STOT,
    HA_STAT_BIN ,
    HA_STAT_BOUT,
    HA_STAT_DREQ,
    HA_STAT_DRESP,
    HA_STAT_EREQ,
    HA_STAT_ECON,
    HA_STAT_ERESP,
    HA_STAT_WRETR,
    HA_STAT_WREDIS,
    HA_STAT_STATUS,
    HA_STAT_WEIGHT,
    HA_STAT_ACT,
    HA_STAT_BCK,
    HA_STAT_CHKFAIL,
    HA_STAT_CHKDOWN,
    HA_STAT_LASTCHG,
    HA_STAT_DOWNTIME,
    HA_STAT_QLIMIT,
    HA_STAT_PID,
    HA_STAT_IID,
    HA_STAT_SID,
    HA_STAT_THROTTLE,
    HA_STAT_LBTOT,
    HA_STAT_TRACKED,
    HA_STAT_TYPE,
    HA_STAT_RATE,
    HA_STAT_RATE_LIM,
    HA_STAT_RATE_MAX,
    HA_STAT_CHECK_STATUS,
    HA_STAT_CHECK_CODE,
    HA_STAT_CHECK_DURATION,
    HA_STAT_HRSP_1XX,
    HA_STAT_HRSP_2XX,
    HA_STAT_HRSP_3XX,
    HA_STAT_HRSP_4XX,
    HA_STAT_HRSP_5XX,
    HA_STAT_HRSP_OTHER,
    HA_STAT_HANAFAIL,
    HA_STAT_REQ_RATE,
    HA_STAT_REQ_RATE_MAX,
    HA_STAT_REQ_TOT,
    HA_STAT_CLI_ABRT,
    HA_STAT_SRV_ABRT,
    HA_STAT_COMP_IN,
    HA_STAT_COMP_OUT,
    HA_STAT_COMP_BYP,
    HA_STAT_COMP_RSP,
    HA_STAT_LASTSESS,
    HA_STAT_LAST_CHK,
    HA_STAT_LAST_AGT,
    HA_STAT_QTIME,
    HA_STAT_CTIME,
    HA_STAT_RTIME,
    HA_STAT_TTIME,
    HA_STAT_AGENT_STATUS,
    HA_STAT_AGENT_CODE,
    HA_STAT_AGENT_DURATION,
    HA_STAT_CHECK_DESC,
    HA_STAT_AGENT_DESC,
    HA_STAT_CHECK_RISE,
    HA_STAT_CHECK_FALL,
    HA_STAT_CHECK_HEALTH,
    HA_STAT_AGENT_RISE,
    HA_STAT_AGENT_FALL,
    HA_STAT_AGENT_HEALTH,
    HA_STAT_ADDR,
    HA_STAT_COOKIE,
    HA_STAT_MODE,
    HA_STAT_ALGO,
    HA_STAT_CONN_RATE,
    HA_STAT_CONN_RATE_MAX,
    HA_STAT_CONN_TOT,
    HA_STAT_INTERCEPTED,
    HA_STAT_DCON,
    HA_STAT_DSES,
    HA_STAT_WREW,
    HA_STAT_CONNECT,
    HA_STAT_REUSE,
    HA_STAT_CACHE_LOOKUPS,
    HA_STAT_CACHE_HITS,
    HA_STAT_SRV_ICUR,
    HA_STAT_SRV_ILIM,
    HA_STAT_QT_MAX,
    HA_STAT_CT_MAX,
    HA_STAT_RT_MAX,
    HA_STAT_TT_MAX,
    HA_STAT_EINT,
    HA_STAT_IDLE_CONN_CUR,
    HA_STAT_SAFE_CONN_CUR,
    HA_STAT_USED_CONN_CUR,
    HA_STAT_NEED_CONN_EST,
    HA_STAT_UWEIGHT,
    HA_STAT_AGG_SRV_CHECK_STATUS,
    HA_STAT_MAX,
};
