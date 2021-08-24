/* ANSI-C code produced by gperf version 3.1 */
/* Command-line: gperf varnish_stats.gperf  */
/* Computed positions: -k'3,5-6,8,11-12,15,17-18' */

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

#line 9 "varnish_stats.gperf"

#line 11 "varnish_stats.gperf"
struct varnish_stats_metric { char *key; int fam; char *lkey; char *lvalue; char *tag1; char *tag2; char *tag3;};
/* maximum key range = 536, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
varnish_stats_hash (register const char *str, register size_t len)
{
  static const unsigned short asso_values[] =
    {
      547, 547, 547, 547, 547, 547, 547, 547, 547, 547,
      547, 547, 547, 547, 547, 547, 547, 547, 547, 547,
      547, 547, 547, 547, 547, 547, 547, 547, 547, 547,
      547, 547, 547, 547, 547, 547, 547, 547, 547, 547,
      547, 547, 547, 547, 547, 547,   0, 547,   0,  10,
       30,  35,   0,  60, 547, 547, 547, 547, 547, 547,
      547, 547, 547, 547, 547, 200, 547, 547, 547,  60,
       25, 547, 547,   0, 547, 145, 547,  75, 547,  75,
      547, 547, 547, 547,  75, 105, 547, 547, 547, 547,
      547, 547, 547, 547, 547,   0, 547,  20,  25,  10,
       10,   5,  70, 115, 120, 175,   5, 165,  30, 115,
       10,   5,   5, 235,   0,   0,   5,  85,  65, 135,
       25, 175,   0, 547, 547, 547, 547, 547, 547, 547,
      547, 547, 547, 547, 547, 547, 547, 547, 547, 547,
      547, 547, 547, 547, 547, 547, 547, 547, 547, 547,
      547, 547, 547, 547, 547, 547, 547, 547, 547, 547,
      547, 547, 547, 547, 547, 547, 547, 547, 547, 547,
      547, 547, 547, 547, 547, 547, 547, 547, 547, 547,
      547, 547, 547, 547, 547, 547, 547, 547, 547, 547,
      547, 547, 547, 547, 547, 547, 547, 547, 547, 547,
      547, 547, 547, 547, 547, 547, 547, 547, 547, 547,
      547, 547, 547, 547, 547, 547, 547, 547, 547, 547,
      547, 547, 547, 547, 547, 547, 547, 547, 547, 547,
      547, 547, 547, 547, 547, 547, 547, 547, 547, 547,
      547, 547, 547, 547, 547, 547, 547, 547, 547, 547,
      547, 547, 547, 547, 547, 547
    };
  register unsigned int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[17]];
      /*FALLTHROUGH*/
      case 17:
        hval += asso_values[(unsigned char)str[16]];
      /*FALLTHROUGH*/
      case 16:
      case 15:
        hval += asso_values[(unsigned char)str[14]];
      /*FALLTHROUGH*/
      case 14:
      case 13:
      case 12:
        hval += asso_values[(unsigned char)str[11]];
      /*FALLTHROUGH*/
      case 11:
        hval += asso_values[(unsigned char)str[10]];
      /*FALLTHROUGH*/
      case 10:
      case 9:
      case 8:
        hval += asso_values[(unsigned char)str[7]];
      /*FALLTHROUGH*/
      case 7:
      case 6:
        hval += asso_values[(unsigned char)str[5]];
      /*FALLTHROUGH*/
      case 5:
        hval += asso_values[(unsigned char)str[4]];
      /*FALLTHROUGH*/
      case 4:
      case 3:
        hval += asso_values[(unsigned char)str[2]];
        break;
    }
  return hval;
}

const struct varnish_stats_metric *
varnish_stats_get_key (register const char *str, register size_t len)
{
  enum
    {
      VARNISH_STATS_TOTAL_KEYWORDS = 217,
      VARNISH_STATS_MIN_WORD_LENGTH = 7,
      VARNISH_STATS_MAX_WORD_LENGTH = 34,
      VARNISH_STATS_MIN_HASH_VALUE = 11,
      VARNISH_STATS_MAX_HASH_VALUE = 546
    };

  static const unsigned char lengthtable[] =
    {
       0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 11,  0,  0,
       0,  0, 11,  0,  0, 14, 10, 11,  0,  0, 14, 15, 16, 12,
       0, 14, 15, 11,  0,  0,  0,  0,  0,  0,  0, 14,  0, 16,
      17, 13,  9,  0, 21, 22,  0,  9,  0,  0, 12, 13,  0,  0,
      11, 17, 18, 19, 20, 16, 17,  0,  0, 10, 16,  0,  0, 19,
      20, 16,  7,  0,  0, 15, 16, 22, 13,  0, 10,  0, 17, 18,
       9, 10,  0, 17,  0, 19, 15, 11, 22,  8, 14,  0,  0,  0,
       0, 14, 10,  0,  0,  0, 14, 15, 11,  0,  0, 19,  0,  0,
      22,  0, 14, 20,  0, 17, 18, 14,  0,  0, 17, 18, 14, 10,
      21,  0, 18,  9,  0,  0, 27,  0,  0,  0, 11, 12,  0, 14,
       0, 11, 17, 13, 14, 10,  0,  0, 13, 19, 15,  0,  0,  0,
      14, 20, 16,  0,  0,  0,  0, 11, 17, 18,  0,  0, 21,  0,
       0,  0,  0,  0,  0, 13, 19, 15, 16,  0,  0, 19, 20,  0,
       0, 23,  9, 15,  0, 27,  8, 29,  0, 11, 27, 23, 34, 15,
      11,  0,  0,  0,  0,  0, 17,  0, 14, 20, 21,  0,  0,  0,
      20,  0, 12,  0,  9, 15,  0,  0,  0, 14, 15, 11,  0,  0,
       9, 15, 11, 17, 18,  0, 15, 11,  0, 23,  0, 20, 21,  0,
       0, 24, 10, 11,  0,  0,  0, 15,  0, 17, 13,  0, 20, 16,
       0, 18, 14, 25, 11,  0,  0, 14, 15, 16, 17, 33,  9, 20,
      11, 22, 18, 19, 20, 11, 12, 18, 14,  0, 16, 17, 18, 14,
       0, 16,  0,  0,  0, 15, 16,  0, 18,  0, 20,  0,  0,  0,
       0, 20, 21,  0, 13,  0,  0, 11,  0, 13,  0, 15,  0, 12,
       0, 19, 20,  0,  0, 13,  0, 15,  0,  0,  0, 14,  0,  0,
       0,  0,  0, 15, 11,  0,  0,  0,  0,  0, 17,  0,  0,  0,
      11,  0,  0, 14,  0, 16,  0,  0, 14,  0,  0, 12,  0,  0,
      10, 11, 12,  8,  9,  0,  0, 17,  0,  0, 15, 11,  0,  0,
       0, 15, 11, 12,  0,  0,  0, 21,  0,  0,  0,  0,  0, 17,
       0,  0,  0,  0,  0,  0,  0,  0, 16,  0,  0,  0, 20,  0,
      17,  0,  0,  0, 11,  0,  0,  0, 20,  0,  0,  0,  0,  0,
      11,  0, 23,  0,  0,  0,  0,  0, 24,  0,  0,  0,  0,  0,
       0,  0,  0, 18,  0,  0,  0,  0,  0,  0, 15,  0, 12,  0,
      14,  0,  0,  0,  0,  0, 15,  0,  0,  0, 19,  0,  0,  0,
       0,  0,  0,  0,  0,  0, 19,  0,  0,  0,  0,  0,  0,  0,
       0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
       0,  0, 18,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
       0,  0,  0,  0,  0, 20,  0,  0,  0,  0,  0, 11,  0,  0,
       0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
       0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
       0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      16
    };
  static const struct varnish_stats_metric wordlist[] =
    {
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""},
#line 82 "varnish_stats.gperf"
      {"MAIN.s_sess", FAM_VARNISH_SESSIONS, NULL, NULL, NULL, NULL, NULL},
      {""}, {""}, {""}, {""},
#line 86 "varnish_stats.gperf"
      {"MAIN.s_pass", FAM_VARNISH_PASS, NULL, NULL, NULL, NULL, NULL},
      {""}, {""},
#line 111 "varnish_stats.gperf"
      {"MAIN.sc_tx_eof", FAM_VARNISH_SESSION_CLOSE, "reason", "TX_EOF", NULL, NULL, NULL},
#line 59 "varnish_stats.gperf"
      {"MAIN.pools", FAM_VARNISH_THREAD_POOLS, NULL, NULL, NULL, NULL, NULL},
#line 85 "varnish_stats.gperf"
      {"MAIN.s_pipe", FAM_VARNISH_SESSION_PIPE, NULL, NULL, NULL, NULL, NULL},
      {""}, {""},
#line 94 "varnish_stats.gperf"
      {"MAIN.s_pipe_in", FAM_VARNISH_PIPE_HDR_BYTES, NULL, NULL, NULL, NULL, NULL},
#line 109 "varnish_stats.gperf"
      {"MAIN.sc_tx_pipe", FAM_VARNISH_SESSION_CLOSE, "reason", "TX_PIPE", NULL, NULL, NULL},
#line 110 "varnish_stats.gperf"
      {"MAIN.sc_tx_error", FAM_VARNISH_SESSION_CLOSE, "reason", "TX_ERROR", NULL, NULL, NULL},
#line 60 "varnish_stats.gperf"
      {"MAIN.threads", FAM_VARNISH_THREADS, NULL, NULL, NULL, NULL, NULL},
      {""},
#line 22 "varnish_stats.gperf"
      {"MAIN.sess_conn", FAM_VARNISH_SESSION, NULL, NULL, NULL, NULL, NULL},
#line 95 "varnish_stats.gperf"
      {"MAIN.s_pipe_out", FAM_VARNISH_PIPE_IN_BYTES, NULL, NULL, NULL, NULL, NULL},
#line 83 "varnish_stats.gperf"
      {"MAIN.n_pipe", FAM_VARNISH_PIPES, NULL, NULL, NULL, NULL, NULL},
      {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 103 "varnish_stats.gperf"
      {"MAIN.sc_rx_bad", FAM_VARNISH_SESSION_CLOSE, "reason", "RX_BAD", NULL, NULL, NULL},
      {""},
#line 113 "varnish_stats.gperf"
      {"MAIN.sc_overload", FAM_VARNISH_SESSION_CLOSE, "reason", "OVERLOAD", NULL, NULL, NULL},
#line 70 "varnish_stats.gperf"
      {"MAIN.sess_dropped", FAM_VARNISH_SESSION_DROPPED, NULL, NULL, NULL, NULL, NULL},
#line 72 "varnish_stats.gperf"
      {"MAIN.n_object", FAM_VARNISH_OBJECTS, NULL, NULL, NULL, NULL, NULL},
#line 133 "varnish_stats.gperf"
      {"MAIN.bans", FAM_VARNISH_BANS, NULL, NULL, NULL},
      {""},
#line 65 "varnish_stats.gperf"
      {"MAIN.thread_queue_len", FAM_VARNISH_THREAD_QUEUE_LEN, NULL, NULL, NULL, NULL, NULL  },
#line 63 "varnish_stats.gperf"
      {"MAIN.threads_destroyed", FAM_VARNISH_THREADS_DESTROYED, NULL, NULL, NULL, NULL, NULL},
      {""},
#line 186 "varnish_stats.gperf"
      {"SMF.c_req", FAM_VARNISH_SMF_ALLOC_REQUEST, NULL, NULL, "id", NULL, NULL},
      {""}, {""},
#line 81 "varnish_stats.gperf"
      {"MAIN.losthdr", FAM_VARNISH_LOST_HDR, NULL, NULL, NULL, NULL, NULL},
#line 136 "varnish_stats.gperf"
      {"MAIN.bans_req", FAM_VARNISH_BANS_REQ, NULL, NULL, NULL                           },
      {""}, {""},
#line 189 "varnish_stats.gperf"
      {"SMF.c_freed", FAM_VARNISH_SMF_FREED_BYTES, NULL, NULL, "id", NULL, NULL},
#line 74 "varnish_stats.gperf"
      {"MAIN.n_objectcore", FAM_VARNISH_OBJECTS_CORE, NULL, NULL, NULL, NULL, NULL},
#line 112 "varnish_stats.gperf"
      {"MAIN.sc_resp_close", FAM_VARNISH_SESSION_CLOSE, "reason", "RESP_CLOSE", NULL, NULL, NULL},
#line 106 "varnish_stats.gperf"
      {"MAIN.sc_rx_overflow", FAM_VARNISH_SESSION_CLOSE, "reason", "RX_OVERFLOW", NULL, NULL, NULL},
#line 62 "varnish_stats.gperf"
      {"MAIN.threads_created", FAM_VARNISH_THREADS_CREATED, NULL, NULL, NULL, NULL, NULL},
#line 96 "varnish_stats.gperf"
      {"MAIN.sess_closed", FAM_VARNISH_SESSION_CLOSED, NULL, NULL, NULL, NULL, NULL},
#line 75 "varnish_stats.gperf"
      {"MAIN.n_objecthead", FAM_VARNISH_OBJECTS_HEAD, NULL, NULL, NULL, NULL, NULL},
      {""}, {""},
#line 187 "varnish_stats.gperf"
      {"SMF.c_fail", FAM_VARNISH_SMF_ALLOC_FAIL, NULL, NULL, "id", NULL, NULL},
#line 139 "varnish_stats.gperf"
      {"MAIN.bans_tested", FAM_VARNISH_BANS_TESTED, NULL, NULL, NULL                        },
      {""}, {""},
#line 98 "varnish_stats.gperf"
      {"MAIN.sess_readahead", FAM_VARNISH_SESSION_READAHEAD, NULL, NULL, NULL, NULL, NULL},
#line 97 "varnish_stats.gperf"
      {"MAIN.sess_closed_err", FAM_VARNISH_SESSION_CLOSED_ERROR, NULL, NULL, NULL, NULL, NULL},
#line 78 "varnish_stats.gperf"
      {"MAIN.n_lru_nuked", FAM_VARNISH_OBJECTS_LRU_NUKED, NULL, NULL, NULL, NULL, NULL},
#line 219 "varnish_stats.gperf"
      {"VBE.req", FAM_VARNISH_BACKEND_REQUESTS, NULL, NULL, "vcl", "backend", "server"},
      {""}, {""},
#line 66 "varnish_stats.gperf"
      {"MAIN.busy_sleep", FAM_VARNISH_REQUEST_BUSY_SLEEP, NULL, NULL, NULL, NULL, NULL},
#line 128 "varnish_stats.gperf"
      {"MAIN.backend_req", FAM_VARNISH_BACKEND_TOTAL_REQUEST, NULL, NULL, NULL, NULL, NULL},
#line 142 "varnish_stats.gperf"
      {"MAIN.bans_tests_tested", FAM_VARNISH_BANS_TESTS_TESTED, NULL, NULL, NULL                  },
#line 135 "varnish_stats.gperf"
      {"MAIN.bans_obj", FAM_VARNISH_BANS_OBJ, NULL, NULL, NULL                           },
      {""},
#line 159 "varnish_stats.gperf"
      {"MAIN.vmods", FAM_VARNISH_VMODS, NULL, NULL, NULL, NULL, NULL},
      {""},
#line 138 "varnish_stats.gperf"
      {"MAIN.bans_deleted", FAM_VARNISH_BANS_DELETED, NULL, NULL, NULL                       },
#line 45 "varnish_stats.gperf"
      {"MAIN.backend_reuse", FAM_VARNISH_BACKEND_TOTAL_REUSE, NULL, NULL, NULL, NULL, NULL},
#line 203 "varnish_stats.gperf"
      {"MSE.c_req", FAM_VARNISH_MSE_ALLOC_REQUEST, NULL, NULL, "id", NULL, NULL},
#line 129 "varnish_stats.gperf"
      {"MAIN.n_vcl", FAM_VARNISH_VCL, NULL, NULL, NULL, NULL, NULL},
      {""},
#line 41 "varnish_stats.gperf"
      {"MAIN.backend_conn", FAM_VARNISH_BACKEND_TOTAL_CONNECTION, NULL, NULL, NULL, NULL, NULL},
      {""},
#line 64 "varnish_stats.gperf"
      {"MAIN.threads_failed", FAM_VARNISH_THREADS_FAILED, NULL, NULL, NULL, NULL, NULL},
#line 137 "varnish_stats.gperf"
      {"MAIN.bans_added", FAM_VARNISH_BANS_ADDED, NULL, NULL, NULL                         },
#line 206 "varnish_stats.gperf"
      {"MSE.c_freed", FAM_VARNISH_MSE_FREED_BYTES, NULL, NULL, "id", NULL, NULL},
#line 40 "varnish_stats.gperf"
      {"MAIN.beresp_shortlived", FAM_VARNISH_BACKEND_TOTAL_RESPONSE_SHORTLIVED, NULL, NULL, NULL, NULL, NULL},
#line 218 "varnish_stats.gperf"
      {"VBE.conn", FAM_VARNISH_BACKEND_CONNECTIONS, NULL, NULL, "vcl", "backend", "server"},
#line 51 "varnish_stats.gperf"
      {"MAIN.fetch_eof", FAM_VARNISH_FETCH_EOF, NULL, NULL, NULL, NULL, NULL},
      {""}, {""}, {""}, {""},
#line 54 "varnish_stats.gperf"
      {"MAIN.fetch_1xx", FAM_VARNISH_FETCH_1XX, NULL, NULL, NULL, NULL, NULL},
#line 204 "varnish_stats.gperf"
      {"MSE.c_fail", FAM_VARNISH_MSE_ALLOC_FAIL, NULL, NULL, "id", NULL, NULL},
      {""}, {""}, {""},
#line 23 "varnish_stats.gperf"
      {"MAIN.sess_fail", FAM_VARNISH_SESSION_FAIL, NULL, NULL, NULL, NULL, NULL},
#line 53 "varnish_stats.gperf"
      {"MAIN.fetch_none", FAM_VARNISH_FETCH_NONE, NULL, NULL, NULL, NULL, NULL},
#line 21 "varnish_stats.gperf"
      {"MAIN.uptime", FAM_VARNISH_UPTIME_SECONDS, NULL, NULL, NULL, NULL, NULL},
      {""}, {""},
#line 134 "varnish_stats.gperf"
      {"MAIN.bans_completed", FAM_VARNISH_BANS_COMPLETED, NULL, NULL, NULL                     },
      {""}, {""},
#line 42 "varnish_stats.gperf"
      {"MAIN.backend_unhealthy", FAM_VARNISH_BACKEND_TOTAL_UNHEALTHY, NULL, NULL, NULL, NULL, NULL},
      {""},
#line 52 "varnish_stats.gperf"
      {"MAIN.fetch_bad", FAM_VARNISH_FETCH_BAD, NULL, NULL, NULL, NULL, NULL},
#line 58 "varnish_stats.gperf"
      {"MAIN.fetch_no_thread", FAM_VARNISH_FETCH_NO_THREAD, NULL, NULL, NULL, NULL, NULL},
      {""},
#line 153 "varnish_stats.gperf"
      {"MAIN.exp_received", FAM_VARNISH_EXPIRY_RECEIVED, NULL, NULL, NULL, NULL, NULL},
#line 107 "varnish_stats.gperf"
      {"MAIN.sc_rx_timeout", FAM_VARNISH_SESSION_CLOSE, "reason", "RX_TIMEOUT", NULL, NULL, NULL},
#line 55 "varnish_stats.gperf"
      {"MAIN.fetch_204", FAM_VARNISH_FETCH_204, NULL, NULL, NULL, NULL, NULL},
      {""}, {""},
#line 44 "varnish_stats.gperf"
      {"MAIN.backend_fail", FAM_VARNISH_BACKEND_TOTAL_FAIL, NULL, NULL, NULL, NULL, NULL},
#line 131 "varnish_stats.gperf"
      {"MAIN.n_vcl_discard", FAM_VARNISH_VCL_DISCARD, NULL, NULL, NULL, NULL, NULL},
#line 56 "varnish_stats.gperf"
      {"MAIN.fetch_304", FAM_VARNISH_FETCH_304, NULL, NULL, NULL, NULL, NULL},
#line 20 "varnish_stats.gperf"
      {"MAIN.summs", FAM_VARNISH_SUMMS, NULL, NULL, NULL, NULL, NULL},
#line 28 "varnish_stats.gperf"
      {"MAIN.sess_fail_enomem", FAM_VARNISH_SESSION_FAIL_ENOMEM, NULL, NULL, NULL, NULL, NULL},
      {""},
#line 50 "varnish_stats.gperf"
      {"MAIN.fetch_chunked", FAM_VARNISH_FETCH_CHUNKED, NULL, NULL, NULL, NULL, NULL},
#line 196 "varnish_stats.gperf"
      {"SMU.c_req", FAM_VARNISH_SMU_ALLOC_REQUEST, NULL, NULL, "id", NULL, NULL},
      {""}, {""},
#line 24 "varnish_stats.gperf"
      {"MAIN.sess_fail_econnaborted", FAM_VARNISH_SESSION_FAIL_ECONNABORTED, NULL, NULL, NULL, NULL, NULL},
      {""}, {""}, {""},
#line 199 "varnish_stats.gperf"
      {"SMU.c_freed", FAM_VARNISH_SMU_FREED_BYTES, NULL, NULL, "id", NULL, NULL},
#line 88 "varnish_stats.gperf"
      {"MAIN.s_synth", FAM_VARNISH_FETCH_BACKGROUND, NULL, NULL, NULL, NULL, NULL},
      {""},
#line 99 "varnish_stats.gperf"
      {"MAIN.sess_herd", FAM_VARNISH_SESSION_HERD, NULL, NULL, NULL, NULL, NULL         },
      {""},
#line 160 "varnish_stats.gperf"
      {"MAIN.n_gzip", FAM_VARNISH_GZIP, NULL, NULL, NULL, NULL, NULL},
#line 100 "varnish_stats.gperf"
      {"MAIN.sc_rem_close", FAM_VARNISH_SESSION_CLOSE, "reason", "REM_CLOSE", NULL, NULL, NULL},
#line 126 "varnish_stats.gperf"
      {"MAIN.shm_cont", FAM_VARNISH_SHM_CONTENTION, NULL, NULL, NULL, NULL, NULL},
#line 146 "varnish_stats.gperf"
      {"MAIN.bans_dups", FAM_VARNISH_BANS_DUPS, NULL, NULL, NULL                          },
#line 197 "varnish_stats.gperf"
      {"SMU.c_fail", FAM_VARNISH_SMU_ALLOC_FAIL, NULL, NULL, "id", NULL, NULL},
      {""}, {""},
#line 150 "varnish_stats.gperf"
      {"MAIN.n_purges", FAM_VARNISH_PURGES, NULL, NULL, NULL, NULL, NULL},
#line 115 "varnish_stats.gperf"
      {"MAIN.sc_range_short", FAM_VARNISH_SESSION_CLOSE, "reason", "RANGE_SHORT", NULL, NULL, NULL},
#line 38 "varnish_stats.gperf"
      {"MAIN.cache_miss", FAM_VARNISH_CACHE_MISS, NULL, NULL, NULL, NULL, NULL      },
      {""}, {""}, {""},
#line 34 "varnish_stats.gperf"
      {"MAIN.cache_hit", FAM_VARNISH_CACHE_HIT, NULL, NULL, NULL, NULL, NULL},
#line 27 "varnish_stats.gperf"
      {"MAIN.sess_fail_ebadf", FAM_VARNISH_SESSION_FAIL_EBADF, NULL, NULL, NULL, NULL, NULL},
#line 123 "varnish_stats.gperf"
      {"MAIN.shm_records", FAM_VARNISH_SHM_RECORDS, NULL, NULL, NULL, NULL, NULL},
      {""}, {""}, {""}, {""},
#line 192 "varnish_stats.gperf"
      {"SMF.g_space", FAM_VARNISH_SMF_AVAILABLE_BYTES, NULL, NULL, "id", NULL, NULL},
#line 151 "varnish_stats.gperf"
      {"MAIN.n_obj_purged", FAM_VARNISH_PURGED_OBJECTS, NULL, NULL, NULL, NULL, NULL},
#line 36 "varnish_stats.gperf"
      {"MAIN.cache_hitpass", FAM_VARNISH_CACHE_HITPASS, NULL, NULL, NULL, NULL, NULL},
      {""}, {""},
#line 114 "varnish_stats.gperf"
      {"MAIN.sc_pipe_overflow", FAM_VARNISH_SESSION_CLOSE, "reason", "PIPE_OVERFLOW", NULL, NULL, NULL},
      {""}, {""}, {""}, {""}, {""}, {""},
#line 155 "varnish_stats.gperf"
      {"MAIN.hcb_lock", FAM_VARNISH_HCB_LOCK, NULL, NULL, NULL, NULL, NULL},
#line 89 "varnish_stats.gperf"
      {"MAIN.s_req_hdrbytes", FAM_VARNISH_SYNTH_RESPONSE, NULL, NULL, NULL, NULL, NULL},
#line 156 "varnish_stats.gperf"
      {"MAIN.hcb_insert", FAM_VARNISH_HCB_INSERT, NULL, NULL, NULL, NULL, NULL },
#line 79 "varnish_stats.gperf"
      {"MAIN.n_lru_moved", FAM_VARNISH_OBJECTS_LRU_MOVED, NULL, NULL, NULL, NULL, NULL},
      {""}, {""},
#line 212 "varnish_stats.gperf"
      {"VBE.bereq_bodybytes", FAM_VARNISH_BACKEND_REQUEST_BODY_BYTES, NULL, NULL, "vcl", "backend", "server"},
#line 35 "varnish_stats.gperf"
      {"MAIN.cache_hit_grace", FAM_VARNISH_CACHE_HIT_GRACE, NULL, NULL, NULL, NULL, NULL},
      {""}, {""},
#line 141 "varnish_stats.gperf"
      {"MAIN.bans_lurker_tested", FAM_VARNISH_BANS_LURKER_TESTED, NULL, NULL, NULL                 },
#line 174 "varnish_stats.gperf"
      {"LCK.creat", FAM_VARNISH_LOCK_CREATED, NULL, NULL, "id", NULL, NULL},
#line 105 "varnish_stats.gperf"
      {"MAIN.sc_rx_junk", FAM_VARNISH_SESSION_CLOSE, "reason", "RX_JUNK", NULL, NULL, NULL},
      {""},
#line 144 "varnish_stats.gperf"
      {"MAIN.bans_lurker_obj_killed", FAM_VARNISH_BANS_LURKER_OBJ_KILLED, NULL, NULL, NULL             },
#line 222 "varnish_stats.gperf"
      {"VBE.fail", FAM_VARNISH_BACKEND_FAIL, NULL, NULL, "vcl", "backend", "server"},
#line 143 "varnish_stats.gperf"
      {"MAIN.bans_lurker_tests_tested", FAM_VARNISH_BANS_LURKER_TESTS_TESTED, NULL, NULL, NULL           },
      {""},
#line 190 "varnish_stats.gperf"
      {"SMF.g_alloc", FAM_VARNISH_SMF_ALLOC_OUTSTANDING, NULL, NULL, "id", NULL, NULL},
#line 147 "varnish_stats.gperf"
      {"MAIN.bans_lurker_contention", FAM_VARNISH_BANS_LURKER_CONTENTION, NULL, NULL, NULL             },
#line 39 "varnish_stats.gperf"
      {"MAIN.beresp_uncacheable", FAM_VARNISH_BACKEND_TOTAL_RESPONSE_UNCACHEABLE, NULL, NULL, NULL, NULL, NULL},
#line 145 "varnish_stats.gperf"
      {"MAIN.bans_lurker_obj_killed_cutoff", FAM_VARNISH_BANS_LURKER_OBJ_KILLED_CUTOFF, NULL, NULL, NULL      },
#line 157 "varnish_stats.gperf"
      {"MAIN.esi_errors", FAM_VARNISH_ESI_ERRORS, NULL, NULL, NULL, NULL, NULL},
#line 209 "varnish_stats.gperf"
      {"MSE.g_space", FAM_VARNISH_MSE_AVAILABLE_BYTES, NULL, NULL, "id", NULL, NULL},
      {""}, {""}, {""}, {""}, {""},
#line 57 "varnish_stats.gperf"
      {"MAIN.fetch_failed", FAM_VARNISH_FETCH_FAILED, NULL, NULL, NULL, NULL, NULL},
      {""},
#line 77 "varnish_stats.gperf"
      {"MAIN.n_expired", FAM_VARNISH_OBJECTS_EXPIRED, NULL, NULL, NULL, NULL, NULL},
#line 91 "varnish_stats.gperf"
      {"MAIN.s_resp_hdrbytes", FAM_VARNISH_REQUEST_BODY_BYTES, NULL, NULL, NULL, NULL, NULL},
#line 108 "varnish_stats.gperf"
      {"MAIN.sc_rx_close_idle", FAM_VARNISH_SESSION_CLOSE, "reason", "RX_CLOSE_IDLE", NULL, NULL, NULL},
      {""}, {""}, {""},
#line 93 "varnish_stats.gperf"
      {"MAIN.s_pipe_hdrbytes", FAM_VARNISH_RESPONSE_BODY_BYTES, NULL, NULL, NULL, NULL, NULL},
      {""},
#line 87 "varnish_stats.gperf"
      {"MAIN.s_fetch", FAM_VARNISH_FETCH, NULL, NULL, NULL, NULL, NULL},
      {""},
#line 210 "varnish_stats.gperf"
      {"VBE.happy", FAM_VARNISH_BACKEND_UP, NULL, NULL, "vcl", "backend", "server"},
#line 104 "varnish_stats.gperf"
      {"MAIN.sc_rx_body", FAM_VARNISH_SESSION_CLOSE, "reason", "RX_BODY", NULL, NULL, NULL},
      {""}, {""}, {""},
#line 76 "varnish_stats.gperf"
      {"MAIN.n_backend", FAM_VARNISH_BACKENDS, NULL, NULL, NULL, NULL, NULL},
#line 48 "varnish_stats.gperf"
      {"MAIN.fetch_head", FAM_VARNISH_FETCH_HEAD, NULL, NULL, NULL, NULL, NULL},
#line 188 "varnish_stats.gperf"
      {"SMF.c_bytes", FAM_VARNISH_SMF_ALLOCATED_BYTES, NULL, NULL, "id", NULL, NULL},
      {""}, {""},
#line 179 "varnish_stats.gperf"
      {"SMA.c_req", FAM_VARNISH_SMA_ALLOC_REQUEST, NULL, NULL, "id", NULL, NULL},
#line 223 "varnish_stats.gperf"
      {"VBE.fail_eacces", FAM_VARNISH_BACKEND_FAIL, "reason", "EACCES",  "vcl", "backend", "server"},
#line 207 "varnish_stats.gperf"
      {"MSE.g_alloc", FAM_VARNISH_MSE_ALLOC_OUTSTANDING, NULL, NULL, "id", NULL, NULL},
#line 158 "varnish_stats.gperf"
      {"MAIN.esi_warnings", FAM_VARNISH_ESI_WARNINGS, NULL, NULL, NULL, NULL, NULL},
#line 162 "varnish_stats.gperf"
      {"MAIN.n_test_gunzip", FAM_VARNISH_TEST_GUNZIP, NULL, NULL, NULL, NULL, NULL},
      {""},
#line 152 "varnish_stats.gperf"
      {"MAIN.exp_mailed", FAM_VARNISH_EXPIRY_MAILED, NULL, NULL, NULL, NULL, NULL},
#line 182 "varnish_stats.gperf"
      {"SMA.c_freed", FAM_VARNISH_SMA_FREED_BYTES, NULL, NULL, "id", NULL, NULL},
      {""},
#line 121 "varnish_stats.gperf"
      {"MAIN.ws_thread_overflow", FAM_VARNISH_WORKSPACE_THREAD_OVERFLOW, NULL, NULL, NULL, NULL, NULL},
      {""},
#line 29 "varnish_stats.gperf"
      {"MAIN.sess_fail_other", FAM_VARNISH_SESSION_FAIL_OTHER, NULL, NULL, NULL, NULL, NULL         },
#line 92 "varnish_stats.gperf"
      {"MAIN.s_resp_bodybytes", FAM_VARNISH_RESPONSE_HDR_BYTES, NULL, NULL, NULL, NULL, NULL},
      {""}, {""},
#line 122 "varnish_stats.gperf"
      {"MAIN.ws_session_overflow", FAM_VARNISH_WORKSPACE_SESSION_OVERFLOW, NULL, NULL, NULL, NULL, NULL  },
#line 180 "varnish_stats.gperf"
      {"SMA.c_fail", FAM_VARNISH_SMA_ALLOC_FAIL, NULL, NULL, "id", NULL, NULL},
#line 202 "varnish_stats.gperf"
      {"SMU.g_space", FAM_VARNISH_SMU_AVAILABLE_BYTES, NULL, NULL, "id", NULL, NULL},
      {""}, {""}, {""},
#line 172 "varnish_stats.gperf"
      {"MEMPOOL.surplus", FAM_VARNISH_MEMPOOL_SURPLUS, NULL, NULL, "id", NULL, NULL},
      {""},
#line 84 "varnish_stats.gperf"
      {"MAIN.pipe_limited", FAM_VARNISH_PIPE_LIMITED, NULL, NULL, NULL, NULL, NULL},
#line 168 "varnish_stats.gperf"
      {"MEMPOOL.frees", FAM_VARNISH_MEMPOOL_FREES, NULL, NULL, "id", NULL, NULL},
      {""},
#line 226 "varnish_stats.gperf"
      {"VBE.fail_enetunreach", FAM_VARNISH_BACKEND_FAIL, "reason", "ENETUNREACH", "vcl", "backend", "server"},
#line 125 "varnish_stats.gperf"
      {"MAIN.shm_flushes", FAM_VARNISH_SHM_FLUSHES, NULL, NULL, NULL, NULL, NULL},
      {""},
#line 47 "varnish_stats.gperf"
      {"MAIN.backend_retry", FAM_VARNISH_BACKEND_TOTAL_RETRY, NULL, NULL, NULL, NULL, NULL},
#line 16 "varnish_stats.gperf"
      {"MGT.child_stop", FAM_VARNISH_MGT_CHILD_STOP, NULL, NULL, NULL, NULL, NULL},
#line 148 "varnish_stats.gperf"
      {"MAIN.bans_persisted_bytes", FAM_VARNISH_BANS_PERSISTED_BYTES, NULL, NULL, NULL               },
#line 205 "varnish_stats.gperf"
      {"MSE.c_bytes", FAM_VARNISH_MSE_ALLOCATED_BYTES, NULL, NULL, "id", NULL, NULL},
      {""}, {""},
#line 173 "varnish_stats.gperf"
      {"MEMPOOL.randry", FAM_VARNISH_MEMPOOL_RANDRY, NULL, NULL, "id", NULL, NULL},
#line 14 "varnish_stats.gperf"
      {"MGT.child_start", FAM_VARNISH_MGT_CHILD_START, NULL, NULL, NULL, NULL, NULL},
#line 71 "varnish_stats.gperf"
      {"MAIN.req_dropped", FAM_VARNISH_REQUEST_DROPPED, NULL, NULL, NULL, NULL, NULL},
#line 101 "varnish_stats.gperf"
      {"MAIN.sc_req_close", FAM_VARNISH_SESSION_CLOSE, "reason", "REQ_CLOSE", NULL, NULL, NULL},
#line 149 "varnish_stats.gperf"
      {"MAIN.bans_persisted_fragmentation", FAM_VARNISH_BANS_PERSISTED_FRAGMENTATION_BYTES, NULL, NULL, NULL },
#line 193 "varnish_stats.gperf"
      {"SMF.g_smf", FAM_VARNISH_SMF_STRUCTS, NULL, NULL, "id", NULL, NULL},
#line 46 "varnish_stats.gperf"
      {"MAIN.backend_recycle", FAM_VARNISH_BACKEND_TOTAL_RECYCLE, NULL, NULL, NULL, NULL, NULL},
#line 217 "varnish_stats.gperf"
      {"VBE.pipe_in", FAM_VARNISH_BACKEND_PIPE_IN_BYTES, NULL, NULL, "vcl", "backend", "server"},
#line 224 "varnish_stats.gperf"
      {"VBE.fail_eaddrnotavail", FAM_VARNISH_BACKEND_FAIL, "reason", "EADDRNOTAVAIL", "vcl", "backend", "server"},
#line 102 "varnish_stats.gperf"
      {"MAIN.sc_req_http10", FAM_VARNISH_SESSION_CLOSE, "reason", "REQ_HTTP10", NULL, NULL, NULL},
#line 213 "varnish_stats.gperf"
      {"VBE.beresp_hdrbytes", FAM_VARNISH_BACKEND_RESPONSE_HDR_BYTES, NULL, NULL, "vcl", "backend", "server"},
#line 118 "varnish_stats.gperf"
      {"MAIN.client_resp_500", FAM_VARNISH_CLIENT_RESPONSE_500, NULL, NULL, NULL, NULL, NULL},
#line 200 "varnish_stats.gperf"
      {"SMU.g_alloc", FAM_VARNISH_SMU_ALLOC_OUTSTANDING, NULL, NULL, "id", NULL, NULL},
#line 164 "varnish_stats.gperf"
      {"MEMPOOL.pool", FAM_VARNISH_MEMPOOL_POOL, NULL, NULL, "id", NULL, NULL},
#line 37 "varnish_stats.gperf"
      {"MAIN.cache_hitmiss", FAM_VARNISH_CACHE_HITMISS, NULL, NULL, NULL, NULL, NULL},
#line 167 "varnish_stats.gperf"
      {"MEMPOOL.allocs", FAM_VARNISH_MEMPOOL_ALLOCIONS, NULL, NULL, "id", NULL, NULL},
      {""},
#line 171 "varnish_stats.gperf"
      {"MEMPOOL.toosmall", FAM_VARNISH_MEMPOOL_TOOSMALL, NULL, NULL, "id", NULL, NULL},
#line 215 "varnish_stats.gperf"
      {"VBE.pipe_hdrbytes", FAM_VARNISH_BACKEND_PIPE_HDR_BYTES, NULL, NULL, "vcl", "backend", "server"},
#line 80 "varnish_stats.gperf"
      {"MAIN.n_lru_limited", FAM_VARNISH_OBJECTS_LRU_LIMITED, NULL, NULL, NULL, NULL, NULL},
#line 15 "varnish_stats.gperf"
      {"MGT.child_exit", FAM_VARNISH_MGT_CHILD_EXIT, NULL, NULL, NULL, NULL, NULL},
      {""},
#line 67 "varnish_stats.gperf"
      {"MAIN.busy_wakeup", FAM_VARNISH_REQUEST_BUSY_WAKEUP, NULL, NULL, NULL, NULL, NULL},
      {""}, {""}, {""},
#line 19 "varnish_stats.gperf"
      {"MGT.child_panic", FAM_VARNISH_MGT_CHILD_PANIC, NULL, NULL, NULL, NULL, NULL},
#line 130 "varnish_stats.gperf"
      {"MAIN.n_vcl_avail", FAM_VARNISH_VCL_AVAIL, NULL, NULL, NULL, NULL, NULL},
      {""},
#line 116 "varnish_stats.gperf"
      {"MAIN.sc_req_http20", FAM_VARNISH_SESSION_CLOSE, "reason", "REQ_HTTP20", NULL, NULL, NULL},
      {""},
#line 73 "varnish_stats.gperf"
      {"MAIN.n_vampireobject", FAM_VARNISH_OBJECTS_VAMPIRE, NULL, NULL, NULL, NULL, NULL},
      {""}, {""}, {""}, {""},
#line 25 "varnish_stats.gperf"
      {"MAIN.sess_fail_eintr", FAM_VARNISH_SESSION_FAIL_EINTR, NULL, NULL, NULL, NULL, NULL},
#line 26 "varnish_stats.gperf"
      {"MAIN.sess_fail_emfile", FAM_VARNISH_SESSION_FAIL_EMFILE, NULL, NULL, NULL, NULL, NULL},
      {""},
#line 220 "varnish_stats.gperf"
      {"VBE.unhealthy", FAM_VARNISH_BACKEND_UNHEALTHY, NULL, NULL, "vcl", "backend", "server"},
      {""}, {""},
#line 198 "varnish_stats.gperf"
      {"SMU.c_bytes", FAM_VARNISH_SMU_ALLOCATED_BYTES, NULL, NULL, "id", NULL, NULL},
      {""},
#line 132 "varnish_stats.gperf"
      {"MAIN.vcl_fail", FAM_VARNISH_VCL_FAIL, NULL, NULL, NULL, NULL, NULL    },
      {""},
#line 124 "varnish_stats.gperf"
      {"MAIN.shm_writes", FAM_VARNISH_SHM_WRITES, NULL, NULL, NULL, NULL, NULL},
      {""},
#line 163 "varnish_stats.gperf"
      {"MEMPOOL.live", FAM_VARNISH_MEMPOOL_LIVE, NULL, NULL, "id", NULL, NULL},
      {""},
#line 117 "varnish_stats.gperf"
      {"MAIN.sc_vcl_failure", FAM_VARNISH_SESSION_CLOSE, "reason", "VCL_FAILURE", NULL, NULL, NULL},
#line 140 "varnish_stats.gperf"
      {"MAIN.bans_obj_killed", FAM_VARNISH_BANS_OBJ_KILLED, NULL, NULL, NULL                    },
      {""}, {""},
#line 161 "varnish_stats.gperf"
      {"MAIN.n_gunzip", FAM_VARNISH_GUNZIP, NULL, NULL, NULL, NULL, NULL},
      {""},
#line 127 "varnish_stats.gperf"
      {"MAIN.shm_cycles", FAM_VARNISH_SHM_CYCLES, NULL, NULL, NULL, NULL, NULL},
      {""}, {""}, {""},
#line 228 "varnish_stats.gperf"
      {"VBE.fail_other", FAM_VARNISH_BACKEND_FAIL, "reason", "OTHER", "vcl", "backend", "server"},
      {""}, {""}, {""}, {""}, {""},
#line 195 "varnish_stats.gperf"
      {"SMF.g_smf_large", FAM_VARNISH_SMF_STRUCTS_LARGE_FREE, NULL, NULL, "id", NULL, NULL},
#line 191 "varnish_stats.gperf"
      {"SMF.g_bytes", FAM_VARNISH_SMF_OUTSTANDING_BYTES, NULL, NULL, "id", NULL, NULL},
      {""}, {""}, {""}, {""}, {""},
#line 43 "varnish_stats.gperf"
      {"MAIN.backend_busy", FAM_VARNISH_BACKEND_TOTAL_BUSY, NULL, NULL, NULL, NULL, NULL},
      {""}, {""}, {""},
#line 185 "varnish_stats.gperf"
      {"SMA.g_space", FAM_VARNISH_SMA_AVAILABLE_BYTES, NULL, NULL, "id", NULL, NULL},
      {""}, {""},
#line 194 "varnish_stats.gperf"
      {"SMF.g_smf_frag", FAM_VARNISH_SMF_STRUCTS_SMALL_FREE, NULL, NULL, "id", NULL, NULL},
      {""},
#line 69 "varnish_stats.gperf"
      {"MAIN.sess_queued", FAM_VARNISH_SESSION_QUEUED, NULL, NULL, NULL, NULL, NULL},
      {""}, {""},
#line 18 "varnish_stats.gperf"
      {"MGT.child_dump", FAM_VARNISH_MGT_CHILD_DUMP, NULL, NULL, NULL, NULL, NULL},
      {""}, {""},
#line 216 "varnish_stats.gperf"
      {"VBE.pipe_out", FAM_VARNISH_BACKEND_PIPE_OUT_BYTES, NULL, NULL, "vcl", "backend", "server"},
      {""}, {""},
#line 13 "varnish_stats.gperf"
      {"MGT.uptime", FAM_VARNISH_MGT_UPTIME_SECONDS, NULL, NULL, NULL, NULL, NULL},
#line 175 "varnish_stats.gperf"
      {"LCK.destroy", FAM_VARNISH_LOCK_DESTROY, NULL, NULL, "id", NULL, NULL},
#line 229 "varnish_stats.gperf"
      {"VBE.helddown", FAM_VARNISH_BACKEND_HELDDOWN, NULL, NULL, "vcl", "backend", "server"},
#line 221 "varnish_stats.gperf"
      {"VBE.busy", FAM_VARNISH_BACKEND_BUSY, NULL, NULL, "vcl", "backend", "server"},
#line 176 "varnish_stats.gperf"
      {"LCK.locks", FAM_VARNISH_LOCK_LOCKS, NULL, NULL, "id", NULL, NULL},
      {""}, {""},
#line 49 "varnish_stats.gperf"
      {"MAIN.fetch_length", FAM_VARNISH_FETCH_LENGTH, NULL, NULL, NULL, NULL, NULL},
      {""}, {""},
#line 154 "varnish_stats.gperf"
      {"MAIN.hcb_nolock", FAM_VARNISH_HCB_NOLOCK, NULL, NULL, NULL, NULL, NULL},
#line 208 "varnish_stats.gperf"
      {"MSE.g_bytes", FAM_VARNISH_MSE_OUTSTANDING_BYTES, NULL, NULL, "id", NULL, NULL},
      {""}, {""}, {""},
#line 170 "varnish_stats.gperf"
      {"MEMPOOL.timeout", FAM_VARNISH_MEMPOOL_TIMEOUT, NULL, NULL, "id", NULL, NULL},
#line 183 "varnish_stats.gperf"
      {"SMA.g_alloc", FAM_VARNISH_SMA_ALLOC_OUTSTANDING, NULL, NULL, "id", NULL, NULL},
#line 177 "varnish_stats.gperf"
      {"LCK.dbg_busy", FAM_VARNISH_LOCK_DBG_BUSY, NULL, NULL, "id", NULL, NULL},
      {""}, {""}, {""},
#line 225 "varnish_stats.gperf"
      {"VBE.fail_econnrefused", FAM_VARNISH_BACKEND_FAIL, "reason", "ECONNREFUSED", "vcl", "backend", "server"},
      {""}, {""}, {""}, {""}, {""},
#line 166 "varnish_stats.gperf"
      {"MEMPOOL.sz_actual", FAM_VARNISH_MEMPOOL_SIZE_ACTUAL_BYTES, NULL, NULL, "id", NULL, NULL},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 68 "varnish_stats.gperf"
      {"MAIN.busy_killed", FAM_VARNISH_REQUEST_BUSY_KILLED, NULL, NULL, NULL, NULL, NULL},
      {""}, {""}, {""},
#line 61 "varnish_stats.gperf"
      {"MAIN.threads_limited", FAM_VARNISH_THREADS_LIMITED, NULL, NULL, NULL, NULL, NULL},
      {""},
#line 165 "varnish_stats.gperf"
      {"MEMPOOL.sz_wanted", FAM_VARNISH_MEMPOOL_SIZE_WANTED_BYTES, NULL, NULL, "id", NULL, NULL},
      {""}, {""}, {""},
#line 181 "varnish_stats.gperf"
      {"SMA.c_bytes", FAM_VARNISH_SMA_ALLOCATED_BYTES, NULL, NULL, "id", NULL, NULL},
      {""}, {""}, {""},
#line 90 "varnish_stats.gperf"
      {"MAIN.s_req_bodybytes", FAM_VARNISH_REQUEST_HDR_BYTES, NULL, NULL, NULL, NULL, NULL},
      {""}, {""}, {""}, {""}, {""},
#line 201 "varnish_stats.gperf"
      {"SMU.g_bytes", FAM_VARNISH_SMU_OUTSTANDING_BYTES, NULL, NULL, "id", NULL, NULL},
      {""},
#line 120 "varnish_stats.gperf"
      {"MAIN.ws_client_overflow", FAM_VARNISH_WORKSPACE_CLIENT_OVERFLOW, NULL, NULL, NULL, NULL, NULL},
      {""}, {""}, {""}, {""}, {""},
#line 119 "varnish_stats.gperf"
      {"MAIN.ws_backend_overflow", FAM_VARNISH_WORKSPACE_BACKEND_OVERFLOW, NULL, NULL, NULL, NULL, NULL},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 211 "varnish_stats.gperf"
      {"VBE.bereq_hdrbytes", FAM_VARNISH_BACKEND_REQUEST_HDR_BYTES, NULL, NULL, "vcl", "backend", "server"},
      {""}, {""}, {""}, {""}, {""}, {""},
#line 169 "varnish_stats.gperf"
      {"MEMPOOL.recycle", FAM_VARNISH_MEMPOOL_RECYCLE, NULL, NULL, "id", NULL, NULL},
      {""},
#line 33 "varnish_stats.gperf"
      {"MAIN.esi_req", FAM_VARNISH_ESI_REQUESTS, NULL, NULL, NULL, NULL, NULL},
      {""},
#line 17 "varnish_stats.gperf"
      {"MGT.child_died", FAM_VARNISH_MGT_CHILD_DIED, NULL, NULL, NULL, NULL, NULL},
      {""}, {""}, {""}, {""}, {""},
#line 32 "varnish_stats.gperf"
      {"MAIN.client_req", FAM_VARNISH_CLIENT_REQUEST, NULL, NULL, NULL, NULL, NULL},
      {""}, {""}, {""},
#line 30 "varnish_stats.gperf"
      {"MAIN.client_req_400", FAM_VARNISH_CLIENT_REQUEST_400, NULL, NULL, NULL, NULL, NULL},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 31 "varnish_stats.gperf"
      {"MAIN.client_req_417", FAM_VARNISH_CLIENT_REQUEST_417, NULL, NULL, NULL, NULL, NULL},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""},
#line 227 "varnish_stats.gperf"
      {"VBE.fail_etimedout", FAM_VARNISH_BACKEND_FAIL, "reason", "ETIMEDOUT", "vcl", "backend", "server"},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 214 "varnish_stats.gperf"
      {"VBE.beresp_bodybytes", FAM_VARNISH_BACKEND_RESPONSE_BODY_BYTES, NULL, NULL, "vcl", "backend", "server"},
      {""}, {""}, {""}, {""}, {""},
#line 184 "varnish_stats.gperf"
      {"SMA.g_bytes", FAM_VARNISH_SMA_OUTSTANDING_BYTES, NULL, NULL, "id", NULL, NULL},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 178 "varnish_stats.gperf"
      {"LCK.dbg_try_fail", FAM_VARNISH_LOCK_DBG_TRY_FAIL, NULL, NULL, "id", NULL, NULL}
    };

  if (len <= VARNISH_STATS_MAX_WORD_LENGTH && len >= VARNISH_STATS_MIN_WORD_LENGTH)
    {
      register unsigned int key = varnish_stats_hash (str, len);

      if (key <= VARNISH_STATS_MAX_HASH_VALUE)
        if (len == lengthtable[key])
          {
            register const char *s = wordlist[key].key;

            if (*str == *s && !memcmp (str + 1, s + 1, len - 1))
              return &wordlist[key];
          }
    }
  return 0;
}
