include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckCSourceCompiles)
include(CheckFunctionExists)
include(CheckStructHasMember)
include(CheckSymbolExists)
include(CMakePushCheckState)

function(check_common_deps)

    check_include_file(unistd.h       HAVE_UNISTD_H)
    check_include_file(strings.h      HAVE_STRINGS_H)
    check_include_file(arpa/inet.h    HAVE_ARPA_INET_H)
    check_include_file(dirent.h       HAVE_DIRENT_H)
    check_include_file(endian.h       HAVE_ENDIAN_H)
    check_include_file(fcntl.h        HAVE_FCNTL_H)
    check_include_file(fnmatch.h      HAVE_FNMATCH_H)
    check_include_file(fs_info.h      HAVE_FS_INFO_H)
    check_include_file(fshelp.h       HAVE_FSHELP_H)
    check_include_file(kstat.h        HAVE_KSTAT_H)
    check_include_file(kvm.h          HAVE_KVM_H)
    check_include_file(libgen.h       HAVE_LIBGEN_H)
    check_include_file(locale.h       HAVE_LOCALE_H)
    check_include_file(mntent.h       HAVE_MNTENT_H)
    check_include_file(mnttab.h       HAVE_MNTTAB_H)
    check_include_file(ndir.h         HAVE_NDIR_H)
    check_include_file(netdb.h        HAVE_NETDB_H)
    check_include_file(paths.h        HAVE_PATHS_H)
    check_include_file(poll.h         HAVE_POLL_H)
    check_include_file(pthread_np.h   HAVE_PTHREAD_NP_H)
    check_include_file(pwd.h          HAVE_PWD_H)
    check_include_file(regex.h        HAVE_REGEX_H)
    check_include_file(sys/dir.h      HAVE_SYS_DIR_H)
    check_include_file(sys/ndir.h     HAVE_SYS_NDIR_H)
    check_include_file(sys/fs_types.h HAVE_SYS_FS_TYPES_H)
    check_include_file(sys/fstyp.h    HAVE_SYS_FSTYP_H)
    check_include_file(sys/ioctl.h    HAVE_SYS_IOCTL_H)
    check_include_file(sys/isa_defs.h HAVE_SYS_ISA_DEFS_H)
    check_include_file(sys/mntent.h   HAVE_SYS_MNTENT_H)
    check_include_files("stdio.h;sys/mnttab.h"   HAVE_SYS_MNTTAB_H)
    check_include_file(sys/param.h    HAVE_SYS_PARAM_H)
    check_include_file(sys/quota.h    HAVE_SYS_QUOTA_H)
    check_include_file(sys/socket.h   HAVE_SYS_SOCKET_H)
    check_include_file(sys/statfs.h   HAVE_SYS_STATFS_H)
    check_include_file(sys/statvfs.h  HAVE_SYS_STATVFS_H)
    check_include_file(sys/time.h     HAVE_SYS_TIME_H)
    check_include_file(sys/types.h    HAVE_SYS_TYPES_H)
    check_include_file(sys/un.h       HAVE_SYS_UN_H)
    check_include_file(sys/ucred.h    HAVE_SYS_UCRED_H)
    check_include_file(sys/vfs.h      HAVE_SYS_VFS_H)
    check_include_file(sys/vfstab.h   HAVE_SYS_VFSTAB_H)
    check_include_file(sys/wait.h     HAVE_SYS_WAIT_H)
    check_include_file(wordexp.h      HAVE_WORDEXP_H)
    check_include_file(xfs/xqm.h      HAVE_XFS_XQM_H)
    check_include_file(sys/stat.h     HAVE_SYS_STAT_H)

    set(_INC_NET_INET "stdint.h" "stddef.h")
    if(HAVE_SYS_TYPES_H)
        list(APPEND _INC_NET_INET sys/types.h)
    endif()
    check_include_files("${_INC_NET_INET};netinet/in_systm.h"  HAVE_NETINET_IN_SYSTM_H)
    if(HAVE_NETINET_IN_SYSTM_H)
        list(APPEND _INC_NET_INET netinet/in_systm.h)
    endif()
    check_include_files("${_INC_NET_INET};netinet/in.h"       HAVE_NETINET_IN_H)
    if(HAVE_NETINET_IN_H)
        list(APPEND _INC_NET_INET netinet/in.h)
    endif()
    check_include_files("${_INC_NET_INET};netinet/ip.h"       HAVE_NETINET_IP_H)
    if(HAVE_NETINET_IP_H)
        list(APPEND _INC_NET_INET netinet/ip.h)
    endif()
    check_include_files("${_INC_NET_INET};netinet/ip_icmp.h"  HAVE_NETINET_IP_ICMP_H)
    check_include_files("${_INC_NET_INET};netinet/ip_var.h"   HAVE_NETINET_IP_VAR_H)
    check_include_files("${_INC_NET_INET};netinet/ip6.h"      HAVE_NETINET_IP6_H)
    check_include_files("${_INC_NET_INET};netinet/icmp6.h"    HAVE_NETINET_ICMP6_H)
    check_include_files("${_INC_NET_INET};netinet/tcp.h"      HAVE_NETINET_TCP_H)
    check_include_files("${_INC_NET_INET};netinet/udp.h"      HAVE_NETINET_UDP_H)
    if(HAVE_SYS_SOCKET_H)
        list(APPEND _INC_NET_INET sys/socket.h)
    endif()
    check_include_files("${_INC_NET_INET};net/if_arp.h"       HAVE_NET_IF_ARP_H)
    if(HAVE_NET_IF_ARP_H)
        list(APPEND _INC_NET_INET net/if_arp.h)
    endif()
    check_include_file(net/if.h            HAVE_NET_IF_H)
    if(HAVE_NET_IF_H)
        list(APPEND _INC_NET_INET net/if.h)
    endif()
    check_include_files("${_INC_NET_INET};netinet/if_ether.h" HAVE_NETINET_IF_ETHER_H)

    check_include_files("time.h;net/ppp_defs.h"               HAVE_NET_PPP_DEFS_H)

    check_include_file(net/if_ppp.h        HAVE_NET_IF_PPP_H)

    check_include_file(sys/sysmacros.h     HAVE_SYS_SYSMACROS_H)

    if(HAVE_SYS_TYPES_H)
        list(APPEND _INC_SYS_MOUNT sys/types.h)
    endif()
    list(APPEND _INC_SYS_MOUNT sys/mount.h)
    check_include_files("${_INC_SYS_MOUNT}"  HAVE_SYS_MOUNT_H)

    if(HAVE_SYS_TYPES_H)
        list(APPEND _INC_SYS_SYSCTL "sys/types.h")
    endif()
    list(APPEND _INC_SYS_SYSCTL "sys/sysctl.h")
    check_include_files("${_INC_SYS_SYSCTL}" HAVE_SYS_SYSCTL_H)


    check_include_file(getopt.h     HAVE_GETOPT_H)
    check_function_exists(getopt_long HAVE_GETOPT_LONG)

    cmake_push_check_state(RESET)
    SET(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -Werror -Werror=implicit-function-declaration")
    check_c_source_compiles("
#define _GNU_SOURCE
#include <unistd.h>
int main(void) {
    return execvpe(\"none\", NULL, NULL);
}" HAVE_EXECVPE)
    cmake_pop_check_state()

    cmake_push_check_state(RESET)
    SET(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -Werror -Werror=implicit-function-declaration")
    check_c_source_compiles("
#include <arpa/inet.h>
int main(void) {
    if (htonll(65536))
        return 0;
    return 1;
}" HAVE_HTONLL)
    cmake_pop_check_state()

    check_function_exists(getpwnam         HAVE_GETPWNAM)
    check_function_exists(getpwnam_r       HAVE_GETPWNAM_R)
    check_function_exists(setgroups        HAVE_SETGROUPS)
    check_function_exists(setlocale        HAVE_SETLOCALE)
    check_function_exists(getfsstat        HAVE_GETFSSTAT)
    check_function_exists(getvfsstat       HAVE_GETVFSSTAT)
    check_function_exists(listmntent       HAVE_LISTMNTENT)
    check_function_exists(getmntent_r      HAVE_GETMNTENT_R)
    check_function_exists(statfs           HAVE_STATFS)
    check_function_exists(statvfs          HAVE_STATVFS)
    check_function_exists(strnlen          HAVE_STRNLEN)
    check_function_exists(sysctl           HAVE_SYSCTL)
    check_function_exists(sysctlbyname     HAVE_SYSCTLBYNAME)
    check_function_exists(close_range      HAVE_CLOSE_RANGE)
    check_function_exists(closefrom        HAVE_CLOSEFROM)

    check_symbol_exists(F_CLOSEM "fcntl.h" HAVE_FCNTL_CLOSEM)

    check_symbol_exists(strerror_r "stdlib.h;string.h" HAVE_STRERROR_R)
    if(HAVE_STRERROR_R)
        check_c_source_runs("
#include <string.h>
int main(void)
{
      char buf[100];
      char x = *strerror_r (0, buf, sizeof buf);
      char *p = strerror_r (0, buf, sizeof buf);
      return !p || x;
}" HAVE_STRERROR_R_CHAR_P)
    endif()

    check_function_exists(getmntent      HAVE_GETMNTENT)
    if(HAVE_GETMNTENT)
       if(HAVE_MNTENT_H)
            set(_MNTENT_H "#include <mntent.h>")
       endif()
        if(HAVE_SYS_MNTENT_H)
            set(_MNTENT_H "#include <sys/mntent.h>")
       endif()
        if(HAVE_MNTTAB_H)
            set(_MNTTAB_H "#include <mnttab.h>")
       endif()
        if(HAVE_SYS_MNTTAB_H)
            set(_MNTTAB_H "#include <sys/mnttab.h>")
       endif()
        check_c_source_compiles("
#include <stdio.h>
${_MNTENT_H}
${_MNTTAB_H}
int main(void)
{
    FILE *fh;
    struct mntent *me;
    fh = setmntent (\"/etc/mtab\", \"r\");
    me = getmntent (fh);
    return me->mnt_passno;
}" HAVE_ONE_GETMNTENT)

        check_c_source_compiles("
#include <stdio.h>
${_MNTENT_H}
${_MNTTAB_H}
int main(void)
{
    FILE *fh;
    struct mnttab mt;
    int status;
    fh = fopen (\"/etc/mnttab\", \"r\");
    status = getmntent (fh, &mt);
    return status;
}" HAVE_TWO_GETMNTENT)
    else()
        check_library_exists(sun getmntent  "" HAVE_SUN_GETMNTENT)
        check_library_exists(gen getmntent  "" HAVE_GEN_GETMNTENT)
    endif()

    check_function_exists(clock_gettime HAVE_CLOCK_GETTIME)

    check_function_exists(strptime HAVE_STRPTIME)
    if(HAVE_STRPTIME)
        cmake_push_check_state(RESET)
        SET(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -Werror=implicit-function-declaration")
        check_c_source_compiles("
#include <time.h>
int main(void) {
    struct tm stm;
    (void)strptime (\"2010-12-30%13:42:42\", \"%Y-%m-%dT%T\", &stm);
}
" HAVE_STRPTIME_DEFAULT)

        if (NOT HAVE_STRPTIME_DEFAULT)
            check_c_source_compiles("
#ifndef _ISOC99_SOURCE
# define _ISOC99_SOURCE 1
#endif
#ifndef _POSIX_C_SOURCE
# define _POSIX_C_SOURCE 200112L
#endif
#ifndef _XOPEN_SOURCE
# define _XOPEN_SOURCE 500
#endif
#include <time.h>
int main(void) {
    struct tm stm;
    (void)strptime (\"2010-12-30%13:42:42\", \"%Y-%m-%dT%T\", &stm);
} " HAVE_STRPTIME_STANDARDS)
            if(NOT HAVE_STRPTIME_STANDARDS)
                unset(HAVE_STRPTIME CACHE)
            endif()
        endif()
        cmake_pop_check_state()
    endif()

    check_function_exists(timegm HAVE_TIMEGM)
    if(HAVE_TIMEGM)
        cmake_push_check_state(RESET)
        SET(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -Werror -Werror=implicit-function-declaration")
        if (HAVE_STRPTIME_STANDARDS)
            SET(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -DHAVE_STRPTIME_STANDARDS")
        endif()
        if (STRPTIME_NEEDS_STANDARDS)
            SET(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -DSTRPTIME_NEEDS_STANDARDS")
        endif()
        check_c_source_compiles("
#if defined(STRPTIME_NEEDS_STANDARDS) || defined(HAVE_STRPTIME_STANDARDS)
# ifndef _ISOC99_SOURCE
#  define _ISOC99_SOURCE 1
# endif
# ifndef _POSIX_C_SOURCE
#  define _POSIX_C_SOURCE 200809L
# endif
# ifndef _XOPEN_SOURCE
#  define _XOPEN_SOURCE 500
# endif
#endif
#ifndef _DEFAULT_SOURCE
# define _DEFAULT_SOURCE 1
#endif

#include <time.h>
int main(void) {
    time_t t = timegm(&(struct tm){0});
    if (t == ((time_t) -1)) {
        return 1;
    }
    return 0;
}" TIMEGM_NEEDS_DEFAULT)
        if(NOT TIMEGM_NEEDS_DEFAULT)
            check_c_source_compiles("
#if defined(STRPTIME_NEEDS_STANDARDS) || defined(HAVE_STRPTIME_STANDARDS)
# ifndef _ISOC99_SOURCE
#  define _ISOC99_SOURCE 1
# endif
# ifndef _POSIX_C_SOURCE
#  define _POSIX_C_SOURCE 200809L
# endif
# ifndef _XOPEN_SOURCE
#  define _XOPEN_SOURCE 500
# endif
#endif
#ifndef _BSD_SOURCE
# define _BSD_SOURCE 1
#endif
#include <time.h>
int main(void) {
    time_t t = timegm(&(struct tm){0});
    if (t == ((time_t) -1)) {
        return 1;
    }
    return 0;
}" TIMEGM_NEEDS_BSD)
            if(NOT TIMEGM_NEEDS_BSD)
                unset(HAVE_TIMEGM CACHE)
            endif()
        endif()
        cmake_pop_check_state()
    endif()


    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
    check_include_file("pthread.h"    HAVE_PTHREAD_H)
    if (HAVE_PTHREAD_H)
        check_function_exists(pthread_setname_np HAVE_PTHREAD_SETNAME_NP)
        if(HAVE_PTHREAD_SETNAME_NP)
            check_c_source_compiles("
#define _GNU_SOURCE
#include <pthread.h>
int main(void) {
    pthread_setname_np((pthread_t) {0}, \"conftest\");
    return 0;
}
" HAVE_PTHREAD_SETNAME_NP_TWO_ARGS)
            if(NOT HAVE_PTHREAD_SETNAME_NP_TWO_ARGS)
                check_c_source_compiles("
#define _GNU_SOURCE
#include <pthread.h>
int main(void) {
pthread_setname_np((pthread_t) {0}, \"conftest\", (void *)0);
    return 0;
}
" HAVE_PTHREAD_SETNAME_NP_THREE_ARGS)
               if(NOT HAVE_PTHREAD_SETNAME_NP_THREE_ARGS)
                    unset(HAVE_PTHREAD_SETNAME_NP CACHE)
               endif()
           endif()
        else()
            check_include_file("pthread_np.h" HAVE_PTHREAD_NP_H)
            if (HAVE_PTHREAD_NP_H)
                check_c_source_compiles("
#include <pthread.h>
#include <pthread_np.h>
int main(void) {
    pthread_set_name_np((pthread_t) {0}, \"conftest\");
    return 0;
}" HAVE_PTHREAD_SET_NAME_NP)
            endif()
        endif()

        check_function_exists(pthread_attr_setaffinity_np HAVE_PTHREAD_ATTR_SETAFFINITY_NP)
    endif()
    cmake_pop_check_state()

endfunction()
