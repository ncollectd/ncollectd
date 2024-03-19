/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

enum {
    FAM_SQUID_CLIENT_HTTP_REQUESTS,
    FAM_SQUID_CLIENT_HTTP_HITS,
    FAM_SQUID_CLIENT_HTTP_ERRORS,
    FAM_SQUID_CLIENT_HTTP_IN_BYTES,
    FAM_SQUID_CLIENT_HTTP_OUT_BYTES,
    FAM_SQUID_CLIENT_HTTP_HIT_OUT_BYTES,
    FAM_SQUID_SERVER_ALL_REQUESTS,
    FAM_SQUID_SERVER_ALL_ERRORS,
    FAM_SQUID_SERVER_ALL_IN_BYTES,
    FAM_SQUID_SERVER_ALL_OUT_BYTES,
    FAM_SQUID_SERVER_HTTP_REQUESTS,
    FAM_SQUID_SERVER_HTTP_ERRORS,
    FAM_SQUID_SERVER_HTTP_IN_BYTES,
    FAM_SQUID_SERVER_HTTP_OUT_BYTES,
    FAM_SQUID_SERVER_FTP_REQUESTS,
    FAM_SQUID_SERVER_FTP_ERRORS,
    FAM_SQUID_SERVER_FTP_IN_BYTES,
    FAM_SQUID_SERVER_FTP_OUT_BYTES,
    FAM_SQUID_SERVER_OTHER_REQUESTS,
    FAM_SQUID_SERVER_OTHER_ERRORS,
    FAM_SQUID_SERVER_OTHER_IN_BYTES,
    FAM_SQUID_SERVER_OTHER_OUT_BYTES,
    FAM_SQUID_ICP_SENT_PKTS,
    FAM_SQUID_ICP_RECV_PKTS,
    FAM_SQUID_ICP_SENT_QUERIES,
    FAM_SQUID_ICP_SENT_REPLIES,
    FAM_SQUID_ICP_RECV_QUERIES,
    FAM_SQUID_ICP_RECV_REPLIES,
    FAM_SQUID_ICP_QUERY_TIMEOUTS,
    FAM_SQUID_ICP_REPLIES_QUEUED,
    FAM_SQUID_ICP_SENT_BYTES,
    FAM_SQUID_ICP_RECV_BYTES,
    FAM_SQUID_ICP_Q_SENT_BYTES,
    FAM_SQUID_ICP_R_SENT_BYTES,
    FAM_SQUID_ICP_Q_RECV_BYTES,
    FAM_SQUID_ICP_R_RECV_BYTES,
    FAM_SQUID_ICP_TIMES_USED,
    FAM_SQUID_CD_TIMES_USED,
    FAM_SQUID_CD_SENT_MSGS,
    FAM_SQUID_CD_RECV_MSGS,
    FAM_SQUID_CD_MEMORY,
    FAM_SQUID_CD_LOCAL_MEMORY,
    FAM_SQUID_CD_SENT_BYTES,
    FAM_SQUID_CD_RECV_BYTES,
    FAM_SQUID_UNLINK_REQUESTS,
    FAM_SQUID_PAGE_FAULTS,
    FAM_SQUID_SELECT_LOOPS,
    FAM_SQUID_CPU_SECONDS,
    FAM_SQUID_WALL_SECONDS,
    FAM_SQUID_SWAP_OUTS,
    FAM_SQUID_SWAP_INS,
    FAM_SQUID_SWAP_FILES_CLEANED,
    FAM_SQUID_ABORTED_REQUESTS,
    FAM_SQUID_MAX,
};

static metric_family_t fams_squid[FAM_SQUID_MAX] = {
    [FAM_SQUID_CLIENT_HTTP_REQUESTS] = {
        .name = "squid_client_http_requests",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CLIENT_HTTP_HITS] = {
        .name = "squid_client_http_hits",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CLIENT_HTTP_ERRORS] = {
        .name = "squid_client_http_errors",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CLIENT_HTTP_IN_BYTES] = {
        .name = "squid_client_http_in_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CLIENT_HTTP_OUT_BYTES] = {
        .name = "squid_client_http_out_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CLIENT_HTTP_HIT_OUT_BYTES] = {
        .name = "squid_client_http_hit_out_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_ALL_REQUESTS] = {
        .name = "squid_server_all_requests",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_ALL_ERRORS] = {
        .name = "squid_server_all_errors",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_ALL_IN_BYTES] = {
        .name = "squid_server_all_in_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_ALL_OUT_BYTES] = {
        .name = "squid_server_all_out_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_HTTP_REQUESTS] = {
        .name = "squid_server_http_requests",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_HTTP_ERRORS] = {
        .name = "squid_server_http_errors",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_HTTP_IN_BYTES] = {
        .name = "squid_server_http_in_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_HTTP_OUT_BYTES] = {
        .name = "squid_server_http_out_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_FTP_REQUESTS] = {
        .name = "squid_server_ftp_requests",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_FTP_ERRORS] = {
        .name = "squid_server_ftp_errors",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_FTP_IN_BYTES] = {
        .name = "squid_server_ftp_in_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_FTP_OUT_BYTES] = {
        .name = "squid_server_ftp_out_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_OTHER_REQUESTS] = {
        .name = "squid_server_other_requests",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_OTHER_ERRORS] = {
        .name = "squid_server_other_errors",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_OTHER_IN_BYTES] = {
        .name = "squid_server_other_in_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_OTHER_OUT_BYTES] = {
        .name = "squid_server_other_out_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_SENT_PKTS] = {
        .name = "squid_icp_sent_pkts",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_RECV_PKTS] = {
        .name = "squid_icp_recv_pkts",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_SENT_QUERIES] = {
        .name = "squid_icp_sent_queries",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_SENT_REPLIES] = {
        .name = "squid_icp_sent_replies",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_RECV_QUERIES] = {
        .name = "squid_icp_recv_queries",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_RECV_REPLIES] = {
        .name = "squid_icp_recv_replies",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_QUERY_TIMEOUTS] = {
        .name = "squid_icp_query_timeouts",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_REPLIES_QUEUED] = {
        .name = "squid_icp_replies_queued",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_SENT_BYTES] = {
        .name = "squid_icp_sent_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_RECV_BYTES] = {
        .name = "squid_icp_recv_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_Q_SENT_BYTES] = {
        .name = "squid_icp_q_sent_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_R_SENT_BYTES] = {
        .name = "squid_icp_r_sent_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_Q_RECV_BYTES] = {
        .name = "squid_icp_q_recv_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_R_RECV_BYTES] = {
        .name = "squid_icp_r_recv_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_TIMES_USED] = {
        .name = "squid_icp_times_used",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CD_TIMES_USED] = {
        .name = "squid_cd_times_used",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CD_SENT_MSGS] = {
        .name = "squid_cd_sent_msgs",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CD_RECV_MSGS] = {
        .name = "squid_cd_recv_msgs",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CD_MEMORY] = {
        .name = "squid_cd_memory",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CD_LOCAL_MEMORY] = {
        .name = "squid_cd_local_memory",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CD_SENT_BYTES] = {
        .name = "squid_cd_sent_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CD_RECV_BYTES] = {
        .name = "squid_cd_recv_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_UNLINK_REQUESTS] = {
        .name = "squid_unlink_requests",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_PAGE_FAULTS] = {
        .name = "squid_page_faults",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SELECT_LOOPS] = {
        .name = "squid_select_loops",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CPU_SECONDS] = {
        .name = "squid_cpu_time",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_WALL_SECONDS] = {
        .name = "squid_wall_time",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SWAP_OUTS] = {
        .name = "squid_swap_outs",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SWAP_INS] = {
        .name = "squid_swap_ins",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SWAP_FILES_CLEANED] = {
        .name = "squid_swap_files_cleaned",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ABORTED_REQUESTS] = {
        .name = "squid_aborted_requests",
        .type = METRIC_TYPE_COUNTER,
    },
};
