/* ANSI-C code produced by gperf version 3.1 */
/* Command-line: gperf squid_counters.gperf  */
/* Computed positions: -k'5,8,$' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gperf@gnu.org>."
#endif

#line 9 "squid_counters.gperf"

#line 11 "squid_counters.gperf"
struct squid_counter { char *key; int fam;};
/* maximum key range = 83, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
squid_counter_hash (register const char *str, register size_t len)
{
  static const unsigned char asso_values[] =
    {
      96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
      96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
      96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
      96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
      96, 96, 96, 96, 96, 96,  5, 96, 96, 96,
      96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
      96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
      96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
      96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
      96, 96, 96, 96, 96, 55, 96, 35,  0,  5,
      10, 10, 30, 96,  5, 15, 96, 25, 60,  0,
       0, 20, 45, 25,  0,  0,  0, 20,  5, 96,
      96,  0, 96, 96, 96, 96, 96, 96, 96, 96,
      96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
      96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
      96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
      96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
      96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
      96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
      96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
      96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
      96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
      96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
      96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
      96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
      96, 96, 96, 96, 96, 96
    };
  return len + asso_values[(unsigned char)str[7]] + asso_values[(unsigned char)str[4]] + asso_values[(unsigned char)str[len - 1]];
}

const struct squid_counter *
squid_counter_get_key (register const char *str, register size_t len)
{
  enum
    {
      FAM_SQUID_COUNTER_TOTAL_KEYWORDS = 53,
      FAM_SQUID_COUNTER_MIN_WORD_LENGTH = 8,
      FAM_SQUID_COUNTER_MAX_WORD_LENGTH = 26,
      FAM_SQUID_COUNTER_MIN_HASH_VALUE = 13,
      FAM_SQUID_COUNTER_MAX_HASH_VALUE = 95
    };

  static const unsigned char lengthtable[] =
    {
       0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  8,
       9, 15,  0, 17,  0,  9,  0, 16, 17, 18, 14, 20, 21, 22,
       8, 14,  0, 26,  0, 18, 14, 20, 21, 22, 13,  0, 15, 16,
      17, 18,  0, 15, 16, 17,  0, 19,  0, 21, 22, 23,  0,  0,
       0, 17, 13, 19, 20, 21, 17, 13, 19, 20, 21, 12,  0,  0,
       0, 16, 12,  0,  9,  0, 16, 12,  0,  0,  0, 16,  0,  0,
       0,  0, 11,  0, 18,  0,  0,  0,  0, 18,  0, 15
    };
  static const struct squid_counter wordlist[] =
    {
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""},
#line 63 "squid_counters.gperf"
      {"swap.ins", FAM_SQUID_SWAP_INS_TOTAL},
#line 62 "squid_counters.gperf"
      {"swap.outs", FAM_SQUID_SWAP_OUTS_TOTAL},
#line 57 "squid_counters.gperf"
      {"unlink.requests", FAM_SQUID_UNLINK_REQUESTS_TOTAL},
      {""},
#line 46 "squid_counters.gperf"
      {"icp.r_kbytes_sent", FAM_SQUID_ICP_R_SENT_BYTES_TOTAL},
      {""},
#line 53 "squid_counters.gperf"
      {"cd.memory", FAM_SQUID_CD_MEMORY_TOTAL},
      {""},
#line 14 "squid_counters.gperf"
      {"client_http.hits", FAM_SQUID_CLIENT_HTTP_HITS_TOTAL},
#line 48 "squid_counters.gperf"
      {"icp.r_kbytes_recv", FAM_SQUID_ICP_R_RECV_BYTES_TOTAL},
#line 15 "squid_counters.gperf"
      {"client_http.errors", FAM_SQUID_CLIENT_HTTP_ERRORS_TOTAL},
#line 55 "squid_counters.gperf"
      {"cd.kbytes_sent", FAM_SQUID_CD_SENT_BYTES_TOTAL},
#line 13 "squid_counters.gperf"
      {"client_http.requests", FAM_SQUID_CLIENT_HTTP_REQUESTS_TOTAL},
#line 16 "squid_counters.gperf"
      {"client_http.kbytes_in", FAM_SQUID_CLIENT_HTTP_IN_BYTES_TOTAL},
#line 17 "squid_counters.gperf"
      {"client_http.kbytes_out", FAM_SQUID_CLIENT_HTTP_OUT_BYTES_TOTAL},
#line 60 "squid_counters.gperf"
      {"cpu_time", FAM_SQUID_CPU_TIME_TOTAL},
#line 56 "squid_counters.gperf"
      {"cd.kbytes_recv", FAM_SQUID_CD_RECV_BYTES_TOTAL},
      {""},
#line 18 "squid_counters.gperf"
      {"client_http.hit_kbytes_out", FAM_SQUID_CLIENT_HTTP_HIT_OUT_BYTES_TOTAL},
      {""},
#line 24 "squid_counters.gperf"
      {"server.http.errors", FAM_SQUID_SERVER_HTTP_ERRORS_TOTAL},
#line 49 "squid_counters.gperf"
      {"icp.times_used", FAM_SQUID_ICP_TIMES_USED_TOTAL},
#line 23 "squid_counters.gperf"
      {"server.http.requests", FAM_SQUID_SERVER_HTTP_REQUESTS_TOTAL},
#line 25 "squid_counters.gperf"
      {"server.http.kbytes_in", FAM_SQUID_SERVER_HTTP_IN_BYTES_TOTAL},
#line 26 "squid_counters.gperf"
      {"server.http.kbytes_out", FAM_SQUID_SERVER_HTTP_OUT_BYTES_TOTAL},
#line 50 "squid_counters.gperf"
      {"cd.times_used", FAM_SQUID_CD_TIMES_USED_TOTAL},
      {""},
#line 43 "squid_counters.gperf"
      {"icp.kbytes_sent", FAM_SQUID_ICP_SENT_BYTES_TOTAL},
#line 37 "squid_counters.gperf"
      {"icp.queries_sent", FAM_SQUID_ICP_SENT_QUERIES_TOTAL},
#line 45 "squid_counters.gperf"
      {"icp.q_kbytes_sent", FAM_SQUID_ICP_Q_SENT_BYTES_TOTAL},
#line 41 "squid_counters.gperf"
      {"icp.query_timeouts", FAM_SQUID_ICP_QUERY_TIMEOUTS_TOTAL},
      {""},
#line 44 "squid_counters.gperf"
      {"icp.kbytes_recv", FAM_SQUID_ICP_RECV_BYTES_TOTAL},
#line 39 "squid_counters.gperf"
      {"icp.queries_recv", FAM_SQUID_ICP_RECV_QUERIES_TOTAL},
#line 47 "squid_counters.gperf"
      {"icp.q_kbytes_recv", FAM_SQUID_ICP_Q_RECV_BYTES_TOTAL},
      {""},
#line 32 "squid_counters.gperf"
      {"server.other.errors", FAM_SQUID_SERVER_OTHER_ERRORS_TOTAL},
      {""},
#line 31 "squid_counters.gperf"
      {"server.other.requests", FAM_SQUID_SERVER_OTHER_REQUESTS_TOTAL},
#line 33 "squid_counters.gperf"
      {"server.other.kbytes_in", FAM_SQUID_SERVER_OTHER_IN_BYTES_TOTAL},
#line 34 "squid_counters.gperf"
      {"server.other.kbytes_out", FAM_SQUID_SERVER_OTHER_OUT_BYTES_TOTAL},
      {""}, {""}, {""},
#line 28 "squid_counters.gperf"
      {"server.ftp.errors", FAM_SQUID_SERVER_FTP_ERRORS_TOTAL},
#line 35 "squid_counters.gperf"
      {"icp.pkts_sent", FAM_SQUID_ICP_SENT_PKTS_TOTAL},
#line 27 "squid_counters.gperf"
      {"server.ftp.requests", FAM_SQUID_SERVER_FTP_REQUESTS_TOTAL},
#line 29 "squid_counters.gperf"
      {"server.ftp.kbytes_in", FAM_SQUID_SERVER_FTP_IN_BYTES_TOTAL},
#line 30 "squid_counters.gperf"
      {"server.ftp.kbytes_out", FAM_SQUID_SERVER_FTP_OUT_BYTES_TOTAL},
#line 20 "squid_counters.gperf"
      {"server.all.errors", FAM_SQUID_SERVER_ALL_ERRORS_TOTAL},
#line 36 "squid_counters.gperf"
      {"icp.pkts_recv", FAM_SQUID_ICP_RECV_PKTS_TOTAL},
#line 19 "squid_counters.gperf"
      {"server.all.requests", FAM_SQUID_SERVER_ALL_REQUESTS_TOTAL},
#line 21 "squid_counters.gperf"
      {"server.all.kbytes_in", FAM_SQUID_SERVER_ALL_IN_BYTES_TOTAL},
#line 22 "squid_counters.gperf"
      {"server.all.kbytes_out", FAM_SQUID_SERVER_ALL_OUT_BYTES_TOTAL},
#line 51 "squid_counters.gperf"
      {"cd.msgs_sent", FAM_SQUID_CD_SENT_MSGS_TOTAL},
      {""}, {""}, {""},
#line 65 "squid_counters.gperf"
      {"aborted_requests", FAM_SQUID_ABORTED_REQUESTS_TOTAL},
#line 52 "squid_counters.gperf"
      {"cd.msgs_recv", FAM_SQUID_CD_RECV_MSGS_TOTAL},
      {""},
#line 61 "squid_counters.gperf"
      {"wall_time", FAM_SQUID_WALL_TIME_TOTAL},
      {""},
#line 38 "squid_counters.gperf"
      {"icp.replies_sent", FAM_SQUID_ICP_SENT_REPLIES_TOTAL},
#line 59 "squid_counters.gperf"
      {"select_loops", FAM_SQUID_SELECT_LOOPS_TOTAL},
      {""}, {""}, {""},
#line 40 "squid_counters.gperf"
      {"icp.replies_recv", FAM_SQUID_ICP_RECV_REPLIES_TOTAL},
      {""}, {""}, {""}, {""},
#line 58 "squid_counters.gperf"
      {"page_faults", FAM_SQUID_PAGE_FAULTS_TOTAL},
      {""},
#line 42 "squid_counters.gperf"
      {"icp.replies_queued", FAM_SQUID_ICP_REPLIES_QUEUED_TOTAL},
      {""}, {""}, {""}, {""},
#line 64 "squid_counters.gperf"
      {"swap.files_cleaned", FAM_SQUID_SWAP_FILES_CLEANED_TOTAL},
      {""},
#line 54 "squid_counters.gperf"
      {"cd.local_memory", FAM_SQUID_CD_LOCAL_MEMORY_TOTAL}
    };

  if (len <= FAM_SQUID_COUNTER_MAX_WORD_LENGTH && len >= FAM_SQUID_COUNTER_MIN_WORD_LENGTH)
    {
      register unsigned int key = squid_counter_hash (str, len);

      if (key <= FAM_SQUID_COUNTER_MAX_HASH_VALUE)
        if (len == lengthtable[key])
          {
            register const char *s = wordlist[key].key;

            if (*str == *s && !memcmp (str + 1, s + 1, len - 1))
              return &wordlist[key];
          }
    }
  return 0;
}
