/* ANSI-C code produced by gperf version 3.1 */
/* Command-line: gperf haproxy_info.gperf  */
/* Computed positions: -k'1,6' */

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

#line 9 "haproxy_info.gperf"

#line 11 "haproxy_info.gperf"
struct hainfo_metric { char *key; int fam; };
/* maximum key range = 85, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
hainfo_hash (register const char *str, register size_t len)
{
  static const unsigned char asso_values[] =
    {
      89, 89, 89, 89, 89, 89, 89, 89, 89, 89,
      89, 89, 89, 89, 89, 89, 89, 89, 89, 89,
      89, 89, 89, 89, 89, 89, 89, 89, 89, 89,
      89, 89, 10, 89, 89, 89, 89, 89, 89, 89,
      89, 89, 89, 89, 89, 89, 89, 89, 89, 89,
      89, 89, 89, 89, 89, 89, 89, 89, 89, 89,
      89, 89, 89, 89, 89, 55, 45, 10, 50, 89,
       0, 89, 25, 25,  0, 89,  0,  0, 40,  0,
      30, 89,  0,  0, 30, 10, 89, 89, 89, 89,
      10, 89, 89, 89, 89,  5, 89,  0, 89,  0,
      10, 10, 89, 89, 89, 40, 89, 89, 15, 25,
      45, 10, 35,  0, 89,  0, 35,  0, 89, 89,
       0, 89, 89, 89, 89, 89, 89, 89, 89, 89,
      89, 89, 89, 89, 89, 89, 89, 89, 89, 89,
      89, 89, 89, 89, 89, 89, 89, 89, 89, 89,
      89, 89, 89, 89, 89, 89, 89, 89, 89, 89,
      89, 89, 89, 89, 89, 89, 89, 89, 89, 89,
      89, 89, 89, 89, 89, 89, 89, 89, 89, 89,
      89, 89, 89, 89, 89, 89, 89, 89, 89, 89,
      89, 89, 89, 89, 89, 89, 89, 89, 89, 89,
      89, 89, 89, 89, 89, 89, 89, 89, 89, 89,
      89, 89, 89, 89, 89, 89, 89, 89, 89, 89,
      89, 89, 89, 89, 89, 89, 89, 89, 89, 89,
      89, 89, 89, 89, 89, 89, 89, 89, 89, 89,
      89, 89, 89, 89, 89, 89, 89, 89, 89, 89,
      89, 89, 89, 89, 89, 89
    };
  register unsigned int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[5]];
      /*FALLTHROUGH*/
      case 5:
      case 4:
      case 3:
      case 2:
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval;
}

const struct hainfo_metric *
hainfo_get_key (register const char *str, register size_t len)
{
  enum
    {
      HAINFO_TOTAL_KEYWORDS = 60,
      HAINFO_MIN_WORD_LENGTH = 4,
      HAINFO_MAX_WORD_LENGTH = 27,
      HAINFO_MIN_HASH_VALUE = 4,
      HAINFO_MAX_HASH_VALUE = 88
    };

  static const unsigned char lengthtable[] =
    {
       0,  0,  0,  0,  4,  0,  0,  7,  8,  9,  0, 11, 12, 13,
      14, 15,  6, 17,  8, 14, 20, 11, 12, 13, 14, 10, 11, 17,
      18,  9, 10, 21, 12, 13, 14,  5, 11, 27, 18,  9, 10, 11,
       7,  8, 14,  0,  6, 12,  8,  9,  0, 21,  7,  8,  9, 15,
      11, 12,  8,  0, 15, 16, 12,  8,  0, 10, 11,  0,  8,  0,
       0, 11,  0,  0,  0,  0, 11,  0,  0,  0,  0,  0,  0,  0,
       0,  0,  0,  0, 13
    };
  static const struct hainfo_metric wordlist[] =
    {
      {""}, {""}, {""}, {""},
#line 60 "haproxy_info.gperf"
      {"Jobs", FAM_HAPROXY_PROCESS_JOBS},
      {""}, {""},
#line 23 "haproxy_info.gperf"
      {"Maxsock", FAM_HAPROXY_PROCESS_MAX_SOCKETS},
#line 38 "haproxy_info.gperf"
      {"SessRate", FAM_HAPROXY_PROCESS_CURRENT_SESSION_RATE},
#line 57 "haproxy_info.gperf"
      {"Run_queue", FAM_HAPROXY_PROCESS_CURRENT_RUN_QUEUE},
      {""},
#line 40 "haproxy_info.gperf"
      {"MaxSessRate", FAM_HAPROXY_PROCESS_MAX_SESSION_RATE},
#line 18 "haproxy_info.gperf"
      {"Memmax_bytes", FAM_HAPROXY_PROCESS_MAX_MEMORY_BYTES},
#line 39 "haproxy_info.gperf"
      {"SessRateLimit", FAM_HAPROXY_PROCESS_LIMIT_SESSION_RATE},
#line 50 "haproxy_info.gperf"
      {"SslCacheMisses", FAM_HAPROXY_PROCESS_SSL_CACHE_MISSES_TOTAL},
#line 49 "haproxy_info.gperf"
      {"SslCacheLookups", FAM_HAPROXY_PROCESS_SSL_CACHE_LOOKUPS_TOTAL},
#line 28 "haproxy_info.gperf"
      {"CumReq", FAM_HAPROXY_PROCESS_REQUESTS_TOTAL},
#line 47 "haproxy_info.gperf"
      {"SslBackendKeyRate", FAM_HAPROXY_PROCESS_CURRENT_BACKEND_SSL_KEY_RATE},
#line 35 "haproxy_info.gperf"
      {"ConnRate", FAM_HAPROXY_PROCESS_CURRENT_CONNECTION_RATE},
#line 17 "haproxy_info.gperf"
      {"Start_time_sec", FAM_HAPROXY_PROCESS_START_TIME_SECONDS},
#line 48 "haproxy_info.gperf"
      {"SslBackendMaxKeyRate", FAM_HAPROXY_PROCESS_MAX_BACKEND_SSL_KEY_RATE},
#line 71 "haproxy_info.gperf"
      {"CumRecvLogs", FAM_HAPROXY_PROCESS_RECV_LOGS_TOTAL},
#line 30 "haproxy_info.gperf"
      {"CurrSslConns", FAM_HAPROXY_PROCESS_CURRENT_SSL_CONNECTIONS},
#line 36 "haproxy_info.gperf"
      {"ConnRateLimit", FAM_HAPROXY_PROCESS_LIMIT_CONNECTION_RATE},
#line 64 "haproxy_info.gperf"
      {"ConnectedPeers", FAM_HAPROXY_PROCESS_CONNECTED_PEERS},
#line 43 "haproxy_info.gperf"
      {"MaxSslRate", FAM_HAPROXY_PROCESS_MAX_SSL_RATE},
#line 29 "haproxy_info.gperf"
      {"MaxSslConns", FAM_HAPROXY_PROCESS_MAX_SSL_CONNECTIONS},
#line 67 "haproxy_info.gperf"
      {"FailedResolutions", FAM_HAPROXY_PROCESS_FAILED_RESOLUTIONS},
#line 44 "haproxy_info.gperf"
      {"SslFrontendKeyRate", FAM_HAPROXY_PROCESS_CURRENT_FRONTEND_SSL_KEY_RATE},
#line 26 "haproxy_info.gperf"
      {"CurrConns", FAM_HAPROXY_PROCESS_CURRENT_CONNECTIONS},
#line 16 "haproxy_info.gperf"
      {"Uptime_sec", FAM_HAPROXY_PROCESS_UPTIME_SECONDS},
#line 45 "haproxy_info.gperf"
      {"SslFrontendMaxKeyRate", FAM_HAPROXY_PROCESS_MAX_FRONTEND_SSL_KEY_RATE},
#line 54 "haproxy_info.gperf"
      {"ZlibMemUsage", FAM_HAPROXY_PROCESS_CURRENT_ZLIB_MEMORY},
#line 51 "haproxy_info.gperf"
      {"CompressBpsIn", FAM_HAPROXY_PROCESS_HTTP_COMP_BYTES_IN_TOTAL},
#line 52 "haproxy_info.gperf"
      {"CompressBpsOut", FAM_HAPROXY_PROCESS_HTTP_COMP_BYTES_OUT_TOTAL},
#line 56 "haproxy_info.gperf"
      {"Tasks", FAM_HAPROXY_PROCESS_CURRENT_TASKS},
#line 31 "haproxy_info.gperf"
      {"CumSslConns", FAM_HAPROXY_PROCESS_SSL_CONNECTIONS_TOTAL},
#line 46 "haproxy_info.gperf"
      {"SslFrontendSessionReuse_pct", FAM_HAPROXY_PROCESS_FRONTEND_SSL_REUSE},
#line 53 "haproxy_info.gperf"
      {"CompressBpsRateLim", FAM_HAPROXY_PROCESS_LIMIT_HTTP_COMP},
#line 34 "haproxy_info.gperf"
      {"PipesFree", FAM_HAPROXY_PROCESS_PIPES_FREE_TOTAL},
#line 21 "haproxy_info.gperf"
      {"PoolFailed", FAM_HAPROXY_PROCESS_POOL_FAILURES_TOTAL},
#line 15 "haproxy_info.gperf"
      {"Process_num", FAM_HAPROXY_PROCESS_RELATIVE_PROCESS_ID},
#line 41 "haproxy_info.gperf"
      {"SslRate", FAM_HAPROXY_PROCESS_CURRENT_SSL_RATE},
#line 32 "haproxy_info.gperf"
      {"Maxpipes", FAM_HAPROXY_PROCESS_MAX_PIPES},
#line 20 "haproxy_info.gperf"
      {"PoolUsed_bytes", FAM_HAPROXY_PROCESS_POOL_USED_BYTES},
      {""},
#line 14 "haproxy_info.gperf"
      {"Nbproc", FAM_HAPROXY_PROCESS_NBPROC},
#line 42 "haproxy_info.gperf"
      {"SslRateLimit", FAM_HAPROXY_PROCESS_LIMIT_SSL_RATE},
#line 59 "haproxy_info.gperf"
      {"Stopping", FAM_HAPROXY_PROCESS_STOPPING},
#line 33 "haproxy_info.gperf"
      {"PipesUsed", FAM_HAPROXY_PROCESS_PIPES_USED_TOTAL},
      {""},
#line 69 "haproxy_info.gperf"
      {"TotalSplicdedBytesOut", FAM_HAPROXY_PROCESS_SPLICED_BYTES_OUT_TOTAL},
#line 24 "haproxy_info.gperf"
      {"Maxconn", FAM_HAPROXY_PROCESS_MAX_CONNECTIONS},
#line 22 "haproxy_info.gperf"
      {"Ulimit-n", FAM_HAPROXY_PROCESS_MAX_FDS},
#line 62 "haproxy_info.gperf"
      {"Listeners", FAM_HAPROXY_PROCESS_LISTENERS},
#line 55 "haproxy_info.gperf"
      {"MaxZlibMemUsage", FAM_HAPROXY_PROCESS_MAX_ZLIB_MEMORY},
#line 37 "haproxy_info.gperf"
      {"MaxConnRate", FAM_HAPROXY_PROCESS_MAX_CONNECTION_RATE},
#line 70 "haproxy_info.gperf"
      {"BytesOutRate", FAM_HAPROXY_PROCESS_BYTES_OUT_RATE},
#line 13 "haproxy_info.gperf"
      {"Nbthread", FAM_HAPROXY_PROCESS_NBTHREAD},
      {""},
#line 19 "haproxy_info.gperf"
      {"PoolAlloc_bytes", FAM_HAPROXY_PROCESS_POOL_ALLOCATED_BYTES},
#line 61 "haproxy_info.gperf"
      {"Unstoppable Jobs", FAM_HAPROXY_PROCESS_UNSTOPPABLE_JOBS},
#line 25 "haproxy_info.gperf"
      {"Hard_maxconn", FAM_HAPROXY_PROCESS_HARD_MAX_CONNECTIONS},
#line 27 "haproxy_info.gperf"
      {"CumConns", FAM_HAPROXY_PROCESS_CONNECTIONS_TOTAL},
      {""},
#line 72 "haproxy_info.gperf"
      {"Build info", FAM_HAPROXY_PROCESS_BUILD_INFO                 },
#line 66 "haproxy_info.gperf"
      {"BusyPolling", FAM_HAPROXY_PROCESS_BUSY_POLLING_ENABLED},
      {""},
#line 58 "haproxy_info.gperf"
      {"Idle_pct", FAM_HAPROXY_PROCESS_IDLE_TIME_PERCENT},
      {""}, {""},
#line 65 "haproxy_info.gperf"
      {"DroppedLogs", FAM_HAPROXY_PROCESS_DROPPED_LOGS_TOTAL},
      {""}, {""}, {""}, {""},
#line 63 "haproxy_info.gperf"
      {"ActivePeers", FAM_HAPROXY_PROCESS_ACTIVE_PEERS},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""},
#line 68 "haproxy_info.gperf"
      {"TotalBytesOut", FAM_HAPROXY_PROCESS_BYTES_OUT_TOTAL}
    };

  if (len <= HAINFO_MAX_WORD_LENGTH && len >= HAINFO_MIN_WORD_LENGTH)
    {
      register unsigned int key = hainfo_hash (str, len);

      if (key <= HAINFO_MAX_HASH_VALUE)
        if (len == lengthtable[key])
          {
            register const char *s = wordlist[key].key;

            if (*str == *s && !memcmp (str + 1, s + 1, len - 1))
              return &wordlist[key];
          }
    }
  return 0;
}
