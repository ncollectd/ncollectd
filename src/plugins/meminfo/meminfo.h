/* ANSI-C code produced by gperf version 3.1 */
/* Command-line: gperf meminfo.gperf  */
/* Computed positions: -k'1-2,11' */

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

#line 9 "meminfo.gperf"

#line 11 "meminfo.gperf"
struct meminfo_metric { char *key; int fam; };
/* maximum key range = 107, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
meminfo_hash (register const char *str, register size_t len)
{
  static const unsigned char asso_values[] =
    {
      113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
      113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
      113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
      113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
      113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
      113, 113, 113, 113, 113, 113, 113, 113,   0, 113,
      113, 113, 113, 113, 113,   5,  55,   5,  15, 113,
       10,  15,  15,  25, 113,  25, 113,   0, 113, 113,
       20, 113,  25,   0,   0,  10,  30,  20, 113, 113,
      113, 113, 113, 113, 113, 113, 113,  10, 113,   0,
        0,   5, 113,  25,   0,  10, 113,   5,  25,  15,
        0,  50,   5, 113,   5, 113,   0,  15, 113,   0,
      113, 113,  40, 113, 113, 113, 113, 113, 113, 113,
      113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
      113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
      113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
      113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
      113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
      113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
      113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
      113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
      113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
      113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
      113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
      113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
      113, 113, 113, 113, 113, 113
    };
  register unsigned int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[10]];
      /*FALLTHROUGH*/
      case 10:
      case 9:
      case 8:
      case 7:
      case 6:
      case 5:
      case 4:
      case 3:
      case 2:
        hval += asso_values[(unsigned char)str[1]];
      /*FALLTHROUGH*/
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval;
}

const struct meminfo_metric *
meminfo_get_key (register const char *str, register size_t len)
{
  enum
    {
      MEMINFO_TOTAL_KEYWORDS = 52,
      MEMINFO_MIN_WORD_LENGTH = 5,
      MEMINFO_MAX_WORD_LENGTH = 18,
      MEMINFO_MIN_HASH_VALUE = 6,
      MEMINFO_MAX_HASH_VALUE = 112
    };

  static const unsigned char lengthtable[] =
    {
       0,  0,  0,  0,  0,  0,  6,  0,  0,  9, 10, 11,  7,  8,
       9, 10,  0,  7, 13,  0, 15, 11,  7, 13,  0, 15,  0, 12,
       8,  9,  5,  6,  7,  8,  9, 10,  0, 12,  8, 14, 15, 11,
      12, 13, 14, 15, 16, 12, 18,  0, 15,  0, 12, 13,  0, 15,
       0, 12, 13, 14,  0,  0,  0, 13,  0,  0,  0, 12, 13,  0,
      15,  0,  0, 13,  0,  0,  0,  0,  8,  0,  0,  0,  0, 13,
       0,  0,  0,  0, 13,  0,  0,  0,  0,  0,  0,  0,  0,  0,
       0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
       7
    };
  static const struct meminfo_metric wordlist[] =
    {
      {""}, {""}, {""}, {""}, {""}, {""},
#line 33 "meminfo.gperf"
      {"Shmem:", FAM_MEMINFO_SHMEM_BYTES},
      {""}, {""},
#line 28 "meminfo.gperf"
      {"SwapFree:", FAM_MEMINFO_SWAP_FREE_BYTES},
#line 27 "meminfo.gperf"
      {"SwapTotal:", FAM_MEMINFO_SWAP_TOTAL_BYTES},
#line 18 "meminfo.gperf"
      {"SwapCached:", FAM_MEMINFO_SWAP_CACHED_BYTES},
#line 19 "meminfo.gperf"
      {"Active:", FAM_MEMINFO_ACTIVE_BYTES},
#line 14 "meminfo.gperf"
      {"MemFree:", FAM_MEMINFO_MEMORY_FREE_BYTES},
#line 13 "meminfo.gperf"
      {"MemTotal:", FAM_MEMINFO_MEMORY_TOTAL_BYTES    },
#line 31 "meminfo.gperf"
      {"AnonPages:", FAM_MEMINFO_ANONYMOUS_BYTES},
      {""},
#line 32 "meminfo.gperf"
      {"Mapped:", FAM_MEMINFO_MAPPED_BYTES},
#line 21 "meminfo.gperf"
      {"Active(anon):", FAM_MEMINFO_ACTIVE_ANONYMOUS_BYTES},
      {""},
#line 51 "meminfo.gperf"
      {"ShmemPmdMapped:", FAM_MEMINFO_HUGEPAGES_SHMEM_PMDMAPPED_BYTES},
#line 37 "meminfo.gperf"
      {"SUnreclaim:", FAM_MEMINFO_SLAB_UNRECLAIMABLE_BYTES},
#line 17 "meminfo.gperf"
      {"Cached:", FAM_MEMINFO_CACHED_BYTES},
#line 23 "meminfo.gperf"
      {"Active(file):", FAM_MEMINFO_ACTIVE_PAGE_CACHE_BYTES},
      {""},
#line 50 "meminfo.gperf"
      {"ShmemHugePages:", FAM_MEMINFO_HUGEPAGES_SHMEM_BYTES},
      {""},
#line 25 "meminfo.gperf"
      {"Unevictable:", FAM_MEMINFO_UNEVICTABLE_BYTES},
#line 55 "meminfo.gperf"
      {"CmaFree:", FAM_MEMINFO_CMA_FREE_BYTES},
#line 54 "meminfo.gperf"
      {"CmaTotal:", FAM_MEMINFO_CMA_TOTAL_BYTES},
#line 35 "meminfo.gperf"
      {"Slab:", FAM_MEMINFO_SLAB_BYTES},
#line 29 "meminfo.gperf"
      {"Dirty:", FAM_MEMINFO_DIRTY_BYTES},
#line 47 "meminfo.gperf"
      {"Percpu:", FAM_MEMINFO_PERCPU_BYTES},
#line 26 "meminfo.gperf"
      {"Mlocked:", FAM_MEMINFO_MLOCKED_BYTES},
#line 20 "meminfo.gperf"
      {"Inactive:", FAM_MEMINFO_INACTIVE_BYTES},
#line 30 "meminfo.gperf"
      {"Writeback:", FAM_MEMINFO_WRITEBACK_BYTES},
      {""},
#line 63 "meminfo.gperf"
      {"DirectMap2M:", FAM_MEMINFO_DIRECTMAP_2M_BYTES},
#line 61 "meminfo.gperf"
      {"Hugetlb:", FAM_MEMINFO_HUGEPAGES_BYTES},
#line 53 "meminfo.gperf"
      {"FilePmdMapped:", FAM_MEMINFO_HUGEPAGES_FILE_PMDMAPPED_BYTES},
#line 22 "meminfo.gperf"
      {"Inactive(anon):", FAM_MEMINFO_INACTIVE_ANONYMOUS_BYTES},
#line 39 "meminfo.gperf"
      {"PageTables:", FAM_MEMINFO_PAGE_TABLES_BYTES},
#line 62 "meminfo.gperf"
      {"DirectMap4k:", FAM_MEMINFO_DIRECTMAP_4K_BYTES},
#line 15 "meminfo.gperf"
      {"MemAvailable:", FAM_MEMINFO_MEMORY_AVAILABLE_BYTES},
#line 49 "meminfo.gperf"
      {"AnonHugePages:", FAM_MEMINFO_HUGEPAGES_ANONYMOUS_BYTES},
#line 59 "meminfo.gperf"
      {"HugePages_Surp:", FAM_MEMINFO_HUGEPAGES_SURPASSED},
#line 56 "meminfo.gperf"
      {"HugePages_Total:", FAM_MEMINFO_HUGEPAGES_TOTAL},
#line 38 "meminfo.gperf"
      {"KernelStack:", FAM_MEMINFO_KERNEL_STACK_BYTES},
#line 48 "meminfo.gperf"
      {"HardwareCorrupted:", FAM_MEMINFO_HARDWARE_CORRUPTED_BYTES},
      {""},
#line 24 "meminfo.gperf"
      {"Inactive(file):", FAM_MEMINFO_INACTIVE_PAGE_CACHE_BYTES},
      {""},
#line 64 "meminfo.gperf"
      {"DirectMap1G:", FAM_MEMINFO_DIRECTMAP_2G_BYTES},
#line 41 "meminfo.gperf"
      {"WritebackTmp:", FAM_MEMINFO_WRITE_BACK_TMP_BYTES},
      {""},
#line 57 "meminfo.gperf"
      {"HugePages_Free:", FAM_MEMINFO_HUGEPAGES_FREE},
      {""},
#line 45 "meminfo.gperf"
      {"VmallocUsed:", FAM_MEMINFO_VMALLOC_USED_BYTES},
#line 46 "meminfo.gperf"
      {"VmallocChunk:", FAM_MEMINFO_VMALLOC_CHUNCK_BYTES},
#line 52 "meminfo.gperf"
      {"FileHugePages:", FAM_MEMINFO_HUGEPAGES_FILE_BYTES},
      {""}, {""}, {""},
#line 36 "meminfo.gperf"
      {"SReclaimable:", FAM_MEMINFO_SLAB_RECLAIMABLE_BYTES},
      {""}, {""}, {""},
#line 42 "meminfo.gperf"
      {"CommitLimit:", FAM_MEMINFO_COMMIT_LIMIT_BYTES},
#line 44 "meminfo.gperf"
      {"VmallocTotal:", FAM_MEMINFO_VMALLOC_TOTAL_BYTES},
      {""},
#line 58 "meminfo.gperf"
      {"HugePages_Rsvd:", FAM_MEMINFO_HUGEPAGES_RESERVED},
      {""}, {""},
#line 43 "meminfo.gperf"
      {"Committed_AS:", FAM_MEMINFO_COMMITTED_BYTES},
      {""}, {""}, {""}, {""},
#line 16 "meminfo.gperf"
      {"Buffers:", FAM_MEMINFO_BUFFERS_BYTES},
      {""}, {""}, {""}, {""},
#line 60 "meminfo.gperf"
      {"Hugepagesize:", FAM_MEMINFO_HUGEPAGE_SIZE_BYTES},
      {""}, {""}, {""}, {""},
#line 34 "meminfo.gperf"
      {"KReclaimable:", FAM_MEMINFO_KERNEL_RECLAIMABLE_BYTES},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""},
#line 40 "meminfo.gperf"
      {"Bounce:", FAM_MEMINFO_BOUNCE_BYTES}
    };

  if (len <= MEMINFO_MAX_WORD_LENGTH && len >= MEMINFO_MIN_WORD_LENGTH)
    {
      register unsigned int key = meminfo_hash (str, len);

      if (key <= MEMINFO_MAX_HASH_VALUE)
        if (len == lengthtable[key])
          {
            register const char *s = wordlist[key].key;

            if (*str == *s && !memcmp (str + 1, s + 1, len - 1))
              return &wordlist[key];
          }
    }
  return 0;
}
