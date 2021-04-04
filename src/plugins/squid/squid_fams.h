enum {
  FAM_SQUID_CLIENT_HTTP_REQUESTS_TOTAL,
  FAM_SQUID_CLIENT_HTTP_HITS_TOTAL,
  FAM_SQUID_CLIENT_HTTP_ERRORS_TOTAL,
  FAM_SQUID_CLIENT_HTTP_IN_BYTES_TOTAL,
  FAM_SQUID_CLIENT_HTTP_OUT_BYTES_TOTAL,
  FAM_SQUID_CLIENT_HTTP_HIT_OUT_BYTES_TOTAL,
  FAM_SQUID_SERVER_ALL_REQUESTS_TOTAL,
  FAM_SQUID_SERVER_ALL_ERRORS_TOTAL,
  FAM_SQUID_SERVER_ALL_IN_BYTES_TOTAL,
  FAM_SQUID_SERVER_ALL_OUT_BYTES_TOTAL,
  FAM_SQUID_SERVER_HTTP_REQUESTS_TOTAL,
  FAM_SQUID_SERVER_HTTP_ERRORS_TOTAL,
  FAM_SQUID_SERVER_HTTP_IN_BYTES_TOTAL,
  FAM_SQUID_SERVER_HTTP_OUT_BYTES_TOTAL,
  FAM_SQUID_SERVER_FTP_REQUESTS_TOTAL,
  FAM_SQUID_SERVER_FTP_ERRORS_TOTAL,
  FAM_SQUID_SERVER_FTP_IN_BYTES_TOTAL,
  FAM_SQUID_SERVER_FTP_OUT_BYTES_TOTAL,
  FAM_SQUID_SERVER_OTHER_REQUESTS_TOTAL,
  FAM_SQUID_SERVER_OTHER_ERRORS_TOTAL,
  FAM_SQUID_SERVER_OTHER_IN_BYTES_TOTAL,
  FAM_SQUID_SERVER_OTHER_OUT_BYTES_TOTAL,
  FAM_SQUID_ICP_SENT_PKTS_TOTAL,
  FAM_SQUID_ICP_RECV_PKTS_TOTAL,
  FAM_SQUID_ICP_SENT_QUERIES_TOTAL,
  FAM_SQUID_ICP_SENT_REPLIES_TOTAL,
  FAM_SQUID_ICP_RECV_QUERIES_TOTAL,
  FAM_SQUID_ICP_RECV_REPLIES_TOTAL,
  FAM_SQUID_ICP_QUERY_TIMEOUTS_TOTAL,
  FAM_SQUID_ICP_REPLIES_QUEUED_TOTAL,
  FAM_SQUID_ICP_SENT_BYTES_TOTAL,
  FAM_SQUID_ICP_RECV_BYTES_TOTAL,
  FAM_SQUID_ICP_Q_SENT_BYTES_TOTAL,
  FAM_SQUID_ICP_R_SENT_BYTES_TOTAL,
  FAM_SQUID_ICP_Q_RECV_BYTES_TOTAL,
  FAM_SQUID_ICP_R_RECV_BYTES_TOTAL,
  FAM_SQUID_ICP_TIMES_USED_TOTAL,
  FAM_SQUID_CD_TIMES_USED_TOTAL,
  FAM_SQUID_CD_SENT_MSGS_TOTAL,
  FAM_SQUID_CD_RECV_MSGS_TOTAL,
  FAM_SQUID_CD_MEMORY_TOTAL,
  FAM_SQUID_CD_LOCAL_MEMORY_TOTAL,
  FAM_SQUID_CD_SENT_BYTES_TOTAL,
  FAM_SQUID_CD_RECV_BYTES_TOTAL,
  FAM_SQUID_UNLINK_REQUESTS_TOTAL,
  FAM_SQUID_PAGE_FAULTS_TOTAL,
  FAM_SQUID_SELECT_LOOPS_TOTAL,
  FAM_SQUID_CPU_TIME_TOTAL,
  FAM_SQUID_WALL_TIME_TOTAL,
  FAM_SQUID_SWAP_OUTS_TOTAL,
  FAM_SQUID_SWAP_INS_TOTAL,
  FAM_SQUID_SWAP_FILES_CLEANED_TOTAL,
  FAM_SQUID_ABORTED_REQUESTS_TOTAL, 
  FAM_SQUID_MAX,
};

static metric_family_t fams_squid[FAM_SQUID_MAX] = {
  [FAM_SQUID_CLIENT_HTTP_REQUESTS_TOTAL] = {
    .name = "squid_client_http_requests_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_CLIENT_HTTP_HITS_TOTAL] = {
    .name = "squid_client_http_hits_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_CLIENT_HTTP_ERRORS_TOTAL] = {
    .name = "squid_client_http_errors_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_CLIENT_HTTP_IN_BYTES_TOTAL] = {
    .name = "squid_client_http_in_bytes_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_CLIENT_HTTP_OUT_BYTES_TOTAL] = {
    .name = "squid_client_http_out_bytes_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_CLIENT_HTTP_HIT_OUT_BYTES_TOTAL] = {
    .name = "squid_client_http_hit_out_bytes_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_SERVER_ALL_REQUESTS_TOTAL] = {
    .name = "squid_server_all_requests_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_SERVER_ALL_ERRORS_TOTAL] = {
    .name = "squid_server_all_errors_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_SERVER_ALL_IN_BYTES_TOTAL] = {
    .name = "squid_server_all_in_bytes_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_SERVER_ALL_OUT_BYTES_TOTAL] = {
    .name = "squid_server_all_out_bytes_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_SERVER_HTTP_REQUESTS_TOTAL] = {
    .name = "squid_server_http_requests_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_SERVER_HTTP_ERRORS_TOTAL] = {
    .name = "squid_server_http_errors_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_SERVER_HTTP_IN_BYTES_TOTAL] = {
    .name = "squid_server_http_in_bytes_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_SERVER_HTTP_OUT_BYTES_TOTAL] = {
    .name = "squid_server_http_out_bytes_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_SERVER_FTP_REQUESTS_TOTAL] = {
    .name = "squid_server_ftp_requests_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_SERVER_FTP_ERRORS_TOTAL] = {
    .name = "squid_server_ftp_errors_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_SERVER_FTP_IN_BYTES_TOTAL] = {
    .name = "squid_server_ftp_in_bytes_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_SERVER_FTP_OUT_BYTES_TOTAL] = {
    .name = "squid_server_ftp_out_bytes_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_SERVER_OTHER_REQUESTS_TOTAL] = {
    .name = "squid_server_other_requests_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_SERVER_OTHER_ERRORS_TOTAL] = {
    .name = "squid_server_other_errors_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_SERVER_OTHER_IN_BYTES_TOTAL] = {
    .name = "squid_server_other_in_bytes_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_SERVER_OTHER_OUT_BYTES_TOTAL] = {
    .name = "squid_server_other_out_bytes_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_ICP_SENT_PKTS_TOTAL] = {
    .name = "squid_icp_sent_pkts_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_ICP_RECV_PKTS_TOTAL] = {
    .name = "squid_icp_recv_pkts_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_ICP_SENT_QUERIES_TOTAL] = {
    .name = "squid_icp_sent_queries_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_ICP_SENT_REPLIES_TOTAL] = {
    .name = "squid_icp_sent_replies_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_ICP_RECV_QUERIES_TOTAL] = {
    .name = "squid_icp_recv_queries_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_ICP_RECV_REPLIES_TOTAL] = {
    .name = "squid_icp_recv_replies_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_ICP_QUERY_TIMEOUTS_TOTAL] = {
    .name = "squid_icp_query_timeouts_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_ICP_REPLIES_QUEUED_TOTAL] = {
    .name = "squid_icp_replies_queued_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_ICP_SENT_BYTES_TOTAL] = {
    .name = "squid_icp_sent_bytes_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_ICP_RECV_BYTES_TOTAL] = {
    .name = "squid_icp_recv_bytes_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_ICP_Q_SENT_BYTES_TOTAL] = {
    .name = "squid_icp_q_sent_bytes_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_ICP_R_SENT_BYTES_TOTAL] = {
    .name = "squid_icp_r_sent_bytes_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_ICP_Q_RECV_BYTES_TOTAL] = {
    .name = "squid_icp_q_recv_bytes_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_ICP_R_RECV_BYTES_TOTAL] = {
    .name = "squid_icp_r_recv_bytes_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_ICP_TIMES_USED_TOTAL] = {
    .name = "squid_icp_times_used_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_CD_TIMES_USED_TOTAL] = {
    .name = "squid_cd_times_used_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_CD_SENT_MSGS_TOTAL] = {
    .name = "squid_cd_sent_msgs_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_CD_RECV_MSGS_TOTAL] = {
    .name = "squid_cd_recv_msgs_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_CD_MEMORY_TOTAL] = {
    .name = "squid_cd_memory_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_CD_LOCAL_MEMORY_TOTAL] = {
    .name = "squid_cd_local_memory_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_CD_SENT_BYTES_TOTAL] = {
    .name = "squid_cd_sent_bytes_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_CD_RECV_BYTES_TOTAL] = {
    .name = "squid_cd_recv_bytes_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_UNLINK_REQUESTS_TOTAL] = {
    .name = "squid_unlink_requests_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_PAGE_FAULTS_TOTAL] = {
    .name = "squid_page_faults_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_SELECT_LOOPS_TOTAL] = {
    .name = "squid_select_loops_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_CPU_TIME_TOTAL] = {
    .name = "squid_cpu_time_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_WALL_TIME_TOTAL] = {
    .name = "squid_wall_time_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_SWAP_OUTS_TOTAL] = {
    .name = "squid_swap_outs_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_SWAP_INS_TOTAL] = {
    .name = "squid_swap_ins_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_SWAP_FILES_CLEANED_TOTAL] = {
    .name = "squid_swap_files_cleaned_total",
    .type = METRIC_TYPE_COUNTER,
  },
  [FAM_SQUID_ABORTED_REQUESTS_TOTAL] = {
    .name = "squid_aborted_requests_total",
    .type = METRIC_TYPE_COUNTER,
  },
};
