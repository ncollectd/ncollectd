/* ANSI-C code produced by gperf version 3.1 */
/* Command-line: gperf fscache.gperf  */
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

#line 9 "fscache.gperf"

#line 11 "fscache.gperf"
struct fscache_metric { char *key; int fam;};
/* maximum key range = 233, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
fscache_hash (register const char *str, register size_t len)
{
  static const unsigned char asso_values[] =
    {
      241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
      241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
      241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
      241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
      241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
      241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
      241, 241, 241, 241, 241, 241, 241,  70, 241, 241,
      241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
      241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
      241, 241, 241, 241, 241, 241, 241,   5,  50,  10,
       72,   5,  90,  20,  55,  52,   2,  15,  20,  27,
        0,   0,  80,  90,  27,  85,  20,  55,  30, 241,
       40,   0, 241, 241, 241, 241, 241, 241, 241, 241,
      241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
      241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
      241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
      241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
      241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
      241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
      241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
      241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
      241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
      241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
      241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
      241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
      241, 241, 241, 241, 241, 241, 241
    };
  register unsigned int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[7]+1];
      /*FALLTHROUGH*/
      case 7:
      case 6:
      case 5:
        hval += asso_values[(unsigned char)str[4]];
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
}

const struct fscache_metric *
fscache_get_key (register const char *str, register size_t len)
{
  enum
    {
      FSCACHE_TOTAL_KEYWORDS = 101,
      FSCACHE_MIN_WORD_LENGTH = 5,
      FSCACHE_MAX_WORD_LENGTH = 10,
      FSCACHE_MIN_HASH_VALUE = 8,
      FSCACHE_MAX_HASH_VALUE = 240
    };

  static const unsigned char lengthtable[] =
    {
       0,  0,  0,  0,  0,  0,  0,  0,  8,  0,  0,  6,  7,  6,
       0,  0,  0,  7,  0,  0, 10,  0,  0,  0,  0,  5,  0,  7,
       8,  0, 10,  6,  0,  0,  9, 10,  0,  0,  8,  9, 10,  0,
      10,  0,  9, 10,  0, 10,  8,  9, 10,  0, 10,  8,  0, 10,
       0,  0,  6,  9,  8,  6, 10,  8,  9, 10,  0,  0,  9,  9,
      10,  0, 10,  0,  0, 10,  0, 10,  8,  9, 10,  0, 10,  0,
       7, 10,  9, 10,  8,  0, 10,  0, 10,  8,  9, 10,  6, 10,
       8,  9, 10,  0,  0,  0, 10, 10,  0, 10,  0,  0, 10,  0,
      10,  0,  9, 10,  0,  0,  0,  9, 10,  9, 10,  6,  0, 10,
       0, 10,  8,  0, 10,  0,  0,  0,  9, 10,  0,  0,  0,  0,
      10,  0,  0,  0,  9, 10,  0, 10,  0,  0, 10,  0, 10,  0,
       9,  0,  9, 10,  0,  9,  0,  0,  0,  0,  0, 10,  0,  0,
       0, 10, 10,  0,  0,  0,  9,  8,  0,  0,  0,  9, 10,  0,
       0,  0,  0, 10,  0, 10,  0,  9,  0,  0,  0,  0,  9,  0,
       0,  0,  0,  0,  0,  0,  0,  0,  0, 10,  0,  0,  0,  0,
       0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
       0,  0,  9,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
       0,  0, 10
    };
  static const struct fscache_metric wordlist[] =
    {
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 42 "fscache.gperf"
      {"Relinqsn", FAM_FSCACHE_RELINQUISHES},
      {""}, {""},
#line 87 "fscache.gperf"
      {"Opscan", FAM_FSCACHE_OP_CANCELLED},
#line 69 "fscache.gperf"
      {"Storesn", FAM_FSCACHE_STORE},
#line 88 "fscache.gperf"
      {"Opsrej", FAM_FSCACHE_OP_REJECTED},
      {""}, {""}, {""},
#line 51 "fscache.gperf"
      {"Allocsn", FAM_FSCACHE_ALLOCS},
      {""}, {""},
#line 99 "fscache.gperf"
      {"CacheOpdro", FAM_FSCACHE_CACHEOP_DROP_OBJECT},
      {""}, {""}, {""}, {""},
#line 92 "fscache.gperf"
      {"Opsgc", FAM_FSCACHE_OP_GC},
      {""},
#line 37 "fscache.gperf"
      {"Invalsn", FAM_FSCACHE_INVALIDATES},
#line 39 "fscache.gperf"
      {"Updatesn", FAM_FSCACHE_UPDATES},
      {""},
#line 43 "fscache.gperf"
      {"Relinqsnul", FAM_FSCACHE_RELINQUISHES_NULL},
#line 91 "fscache.gperf"
      {"Opsrel", FAM_FSCACHE_OP_RELEASE},
      {""}, {""},
#line 81 "fscache.gperf"
      {"VmScanbsy", FAM_FSCACHE_STORE_VMSCAN_BUSY},
#line 101 "fscache.gperf"
      {"CacheOpsyn", FAM_FSCACHE_CACHEOP_SYNC_CACHE},
      {""}, {""},
#line 59 "fscache.gperf"
      {"Retrvlsn", FAM_FSCACHE_RETRIEVALS},
#line 55 "fscache.gperf"
      {"Allocsint", FAM_FSCACHE_ALLOCS_INTR},
#line 17 "fscache.gperf"
      {"Objectsnal", FAM_FSCACHE_OBJECT_NO_ALLOC},
      {""},
#line 94 "fscache.gperf"
      {"CacheOpluo", FAM_FSCACHE_CACHEOP_LOOKUP_OBJECT},
      {""},
#line 75 "fscache.gperf"
      {"Storesrun", FAM_FSCACHE_STORE_CALLS},
#line 98 "fscache.gperf"
      {"CacheOpupo", FAM_FSCACHE_CACHEOP_UPDATE_OBJECT},
      {""},
#line 97 "fscache.gperf"
      {"CacheOpinv", FAM_FSCACHE_CACHEOP_INVALIDATE_OBJECT},
#line 70 "fscache.gperf"
      {"Storesok", FAM_FSCACHE_STORE_OK},
#line 58 "fscache.gperf"
      {"Allocsabt", FAM_FSCACHE_ALLOCS_OBJECT_DEAD},
#line 40 "fscache.gperf"
      {"Updatesnul", FAM_FSCACHE_UPDATES_NULL},
      {""},
#line 95 "fscache.gperf"
      {"CacheOpluc", FAM_FSCACHE_CACHEOP_LOOKUP_COMPLETE},
#line 52 "fscache.gperf"
      {"Allocsok", FAM_FSCACHE_ALLOCS_OK},
      {""},
#line 111 "fscache.gperf"
      {"CacheEvstl", FAM_FSCACHE_CACHE_STALE_OBJECTS},
      {""}, {""},
#line 89 "fscache.gperf"
      {"Opsini", FAM_FSCACHE_OP_INITIALISED},
#line 38 "fscache.gperf"
      {"Invalsrun", FAM_FSCACHE_INVALIDATES_RUN},
#line 26 "fscache.gperf"
      {"Acquiren", FAM_FSCACHE_ACQUIRES},
#line 85 "fscache.gperf"
      {"Opsrun", FAM_FSCACHE_OP_RUN},
#line 64 "fscache.gperf"
      {"Retrvlsint", FAM_FSCACHE_RETRIEVALS_INTR},
#line 32 "fscache.gperf"
      {"Lookupsn", FAM_FSCACHE_OBJECT_LOOKUPS},
#line 82 "fscache.gperf"
      {"VmScancan", FAM_FSCACHE_STORE_VMSCAN_CANCELLED},
#line 93 "fscache.gperf"
      {"CacheOpalo", FAM_FSCACHE_CACHEOP_ALLOC_OBJECT},
      {""}, {""},
#line 78 "fscache.gperf"
      {"Storesolm", FAM_FSCACHE_STORE_PAGES_OVER_LIMIT},
#line 71 "fscache.gperf"
      {"Storesagn", FAM_FSCACHE_STORE_AGAIN},
#line 96 "fscache.gperf"
      {"CacheOpgro", FAM_FSCACHE_CACHEOP_GRAB_OBJECT},
      {""},
#line 28 "fscache.gperf"
      {"Acquirenoc", FAM_FSCACHE_ACQUIRES_NO_CACHE},
      {""}, {""},
#line 102 "fscache.gperf"
      {"CacheOpatc", FAM_FSCACHE_CACHEOP_ATTR_CHANGED},
      {""},
#line 44 "fscache.gperf"
      {"Relinqswcr", FAM_FSCACHE_RELINQUISHES_WAITCRT},
#line 46 "fscache.gperf"
      {"AttrChgn", FAM_FSCACHE_ATTR_CHANGED},
#line 57 "fscache.gperf"
      {"Allocsowt", FAM_FSCACHE_ALLOC_OP_WAITS},
#line 16 "fscache.gperf"
      {"Objectsalc", FAM_FSCACHE_OBJECT_ALLOC},
      {""},
#line 27 "fscache.gperf"
      {"Acquirenul", FAM_FSCACHE_ACQUIRES_NULL},
      {""},
#line 84 "fscache.gperf"
      {"Opspend", FAM_FSCACHE_OP_PENDING},
#line 33 "fscache.gperf"
      {"Lookupsneg", FAM_FSCACHE_OBJECT_LOOKUPS_NEGATIVE},
#line 77 "fscache.gperf"
      {"Storesrxd", FAM_FSCACHE_STORE_RADIX_DELETES},
#line 14 "fscache.gperf"
      {"Cookiesdat", FAM_FSCACHE_COOKIE_DATA},
#line 83 "fscache.gperf"
      {"VmScanwt", FAM_FSCACHE_STORE_VMSCAN_WAIT},
      {""},
#line 18 "fscache.gperf"
      {"Objectsavl", FAM_FSCACHE_OBJECT_AVAIL},
      {""},
#line 15 "fscache.gperf"
      {"Cookiesspc", FAM_FSCACHE_COOKIE_SPECIAL},
#line 53 "fscache.gperf"
      {"Allocswt", FAM_FSCACHE_ALLOCS_WAIT},
#line 80 "fscache.gperf"
      {"VmScangon", FAM_FSCACHE_STORE_VMSCAN_GONE},
#line 110 "fscache.gperf"
      {"CacheEvnsp", FAM_FSCACHE_CACHE_NO_SPACE_REJECT},
#line 86 "fscache.gperf"
      {"Opsenq", FAM_FSCACHE_OP_ENQUEUE},
#line 19 "fscache.gperf"
      {"Objectsded", FAM_FSCACHE_OBJECT_DEAD},
#line 21 "fscache.gperf"
      {"ChkAuxok", FAM_FSCACHE_CHECKAUX_OKAY},
#line 61 "fscache.gperf"
      {"Retrvlswt", FAM_FSCACHE_RETRIEVALS_WAIT},
#line 109 "fscache.gperf"
      {"CacheOpdsp", FAM_FSCACHE_CACHEOP_DISSOCIATE_PAGES},
      {""}, {""}, {""},
#line 13 "fscache.gperf"
      {"Cookiesidx", FAM_FSCACHE_COOKIE_INDEX},
#line 100 "fscache.gperf"
      {"CacheOppto", FAM_FSCACHE_CACHEOP_PUT_OBJECT},
      {""},
#line 113 "fscache.gperf"
      {"CacheEvcul", FAM_FSCACHE_CACHE_CULLED_OBJECTS},
      {""}, {""},
#line 68 "fscache.gperf"
      {"Retrvlsabt", FAM_FSCACHE_RETRIEVALS_OBJECT_DEAD},
      {""},
#line 62 "fscache.gperf"
      {"Retrvlsnod", FAM_FSCACHE_RETRIEVALS_NODATA},
      {""},
#line 72 "fscache.gperf"
      {"Storesnbf", FAM_FSCACHE_STORE_NOBUFS},
#line 41 "fscache.gperf"
      {"Updatesrun", FAM_FSCACHE_UPDATES_RUN},
      {""}, {""}, {""},
#line 54 "fscache.gperf"
      {"Allocsnbf", FAM_FSCACHE_ALLOCS_NOBUFS},
#line 36 "fscache.gperf"
      {"Lookupstmo", FAM_FSCACHE_OBJECT_LOOKUPS_TIMED_OUT},
#line 73 "fscache.gperf"
      {"Storesoom", FAM_FSCACHE_STORE_OOM},
#line 45 "fscache.gperf"
      {"Relinqsrtr", FAM_FSCACHE_RELINQUISHES_RETIRE},
#line 90 "fscache.gperf"
      {"Opsdfr", FAM_FSCACHE_OP_DEFERRED_RELEASE},
      {""},
#line 108 "fscache.gperf"
      {"CacheOpucp", FAM_FSCACHE_CACHEOP_UNCACHE_PAGE},
      {""},
#line 112 "fscache.gperf"
      {"CacheEvrtr", FAM_FSCACHE_CACHE_RETIRED_OBJECTS},
#line 24 "fscache.gperf"
      {"Pagesmrk", FAM_FSCACHE_MARKS},
      {""},
#line 63 "fscache.gperf"
      {"Retrvlsnbf", FAM_FSCACHE_RETRIEVALS_NOBUFS},
      {""}, {""}, {""},
#line 60 "fscache.gperf"
      {"Retrvlsok", FAM_FSCACHE_RETRIEVALS_OK},
#line 107 "fscache.gperf"
      {"CacheOpwrp", FAM_FSCACHE_CACHEOP_WRITE_PAGE},
      {""}, {""}, {""}, {""},
#line 67 "fscache.gperf"
      {"Retrvlsowt", FAM_FSCACHE_RETRIEVAL_OP_WAITS},
      {""}, {""}, {""},
#line 20 "fscache.gperf"
      {"ChkAuxnon", FAM_FSCACHE_CHECKAUX_NONE},
#line 105 "fscache.gperf"
      {"CacheOpalp", FAM_FSCACHE_CACHEOP_ALLOCATE_PAGE},
      {""},
#line 65 "fscache.gperf"
      {"Retrvlsoom", FAM_FSCACHE_RETRIEVALS_NOMEM},
      {""}, {""},
#line 106 "fscache.gperf"
      {"CacheOpals", FAM_FSCACHE_CACHEOP_ALLOCATE_PAGES},
      {""},
#line 30 "fscache.gperf"
      {"Acquirenbf", FAM_FSCACHE_ACQUIRES_NOBUFS},
      {""},
#line 76 "fscache.gperf"
      {"Storespgs", FAM_FSCACHE_STORE_PAGES},
      {""},
#line 29 "fscache.gperf"
      {"Acquireok", FAM_FSCACHE_ACQUIRES_OK},
#line 35 "fscache.gperf"
      {"Lookupscrt", FAM_FSCACHE_OBJECT_CREATED},
      {""},
#line 23 "fscache.gperf"
      {"ChkAuxobs", FAM_FSCACHE_CHECKAUX_OBSOLETE},
      {""}, {""}, {""}, {""}, {""},
#line 50 "fscache.gperf"
      {"AttrChgrun", FAM_FSCACHE_ATTR_CHANGED_CALLS},
      {""}, {""}, {""},
#line 31 "fscache.gperf"
      {"Acquireoom", FAM_FSCACHE_ACQUIRES_OOM},
#line 48 "fscache.gperf"
      {"AttrChgnbf", FAM_FSCACHE_ATTR_CHANGED_NOBUFS},
      {""}, {""}, {""},
#line 47 "fscache.gperf"
      {"AttrChgok", FAM_FSCACHE_ATTR_CHANGED_OK},
#line 25 "fscache.gperf"
      {"Pagesunc", FAM_FSCACHE_UNCACHES},
      {""}, {""}, {""},
#line 79 "fscache.gperf"
      {"VmScannos", FAM_FSCACHE_STORE_VMSCAN_NOT_STORING},
#line 103 "fscache.gperf"
      {"CacheOprap", FAM_FSCACHE_CACHEOP_READ_OR_ALLOC_PAGE},
      {""}, {""}, {""}, {""},
#line 104 "fscache.gperf"
      {"CacheOpras", FAM_FSCACHE_CACHEOP_READ_OR_ALLOC_PAGES},
      {""},
#line 49 "fscache.gperf"
      {"AttrChgoom", FAM_FSCACHE_ATTR_CHANGED_NOMEM},
      {""},
#line 74 "fscache.gperf"
      {"Storesops", FAM_FSCACHE_STORE_OPS},
      {""}, {""}, {""}, {""},
#line 56 "fscache.gperf"
      {"Allocsops", FAM_FSCACHE_ALLOC_OPS},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""},
#line 66 "fscache.gperf"
      {"Retrvlsops", FAM_FSCACHE_RETRIEVAL_OPS},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""},
#line 22 "fscache.gperf"
      {"ChkAuxupd", FAM_FSCACHE_CHECKAUX_UPDATE},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""},
#line 34 "fscache.gperf"
      {"Lookupspos", FAM_FSCACHE_OBJECT_LOOKUPS_POSITIVE}
    };

  if (len <= FSCACHE_MAX_WORD_LENGTH && len >= FSCACHE_MIN_WORD_LENGTH)
    {
      register unsigned int key = fscache_hash (str, len);

      if (key <= FSCACHE_MAX_HASH_VALUE)
        if (len == lengthtable[key])
          {
            register const char *s = wordlist[key].key;

            if (*str == *s && !memcmp (str + 1, s + 1, len - 1))
              return &wordlist[key];
          }
    }
  return 0;
}
