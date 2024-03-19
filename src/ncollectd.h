/* SPDX-License-Identifier: GPL-2.0-only OR MIT                         */
/* SPDX-FileCopyrightText: Copyright (C) 2005,2006 Florian octo Forster */
/* SPDX-FileContributor: Florian octo Forster <octo at collectd.org>    */

#pragma once

#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS_TYPES_H
#    include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#    include <sys/stat.h>
#endif
#ifdef HAVE_STRINGS_H
#    include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#    include <sys/wait.h>
#endif

#ifndef WEXITSTATUS
#    define WEXITSTATUS(stat_val) ((unsigned int)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
#    define WIFEXITED(stat_val) (((stat_val)&255) == 0)
#endif

#ifdef HAVE_FCNTL_H
#    include <fcntl.h>
#endif

#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#else
#    include <time.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#    include <sys/socket.h>
#endif

#ifdef NAN_STATIC_DEFAULT
#    include <math.h>
#elif NAN_STATIC_ISOC
#    ifndef __USE_ISOC99
#        define DISABLE_ISOC99 1
#        define __USE_ISOC99 1
#    endif
#    include <math.h>
#    if DISABLE_ISOC99
#        undef DISABLE_ISOC99
#        undef __USE_ISOC99
#    endif
#elif NAN_ZERO_ZERO
#    include <math.h>
#    ifdef NAN
#        undef NAN
#    endif
#    define NAN (0.0 / 0.0)
#    ifndef isnan
#        define isnan(f) ((f) != (f))
#    endif
#    ifndef isfinite
#        define isfinite(f) (((f) - (f)) == 0.0)
#    endif
#    ifndef isinf
#        define isinf(f) (!isfinite(f) && !isnan(f))
#    endif
#endif

/* Try really, really hard to determine endianess. Under NexentaStor 1.0.2 this
 * information is in <sys/isa_defs.h>, possibly some other Solaris versions do
 * this too.. */
#ifdef HAVE_ENDIAN_H
#    include <endian.h>
#elif defined(HAVE_SYS_ISA_DEFS_H)
#    include <sys/isa_defs.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#    include <sys/param.h>
#endif

#ifndef BYTE_ORDER
#    if defined(_BYTE_ORDER)
#        define BYTE_ORDER _BYTE_ORDER
#    elif defined(__BYTE_ORDER)
#        define BYTE_ORDER __BYTE_ORDER
#    elif defined(__DARWIN_BYTE_ORDER)
#        define BYTE_ORDER __DARWIN_BYTE_ORDER
#    endif
#endif

#ifndef BIG_ENDIAN
#    if defined(_BIG_ENDIAN)
#        define BIG_ENDIAN _BIG_ENDIAN
#    elif defined(__BIG_ENDIAN)
#        define BIG_ENDIAN __BIG_ENDIAN
#    elif defined(__DARWIN_BIG_ENDIAN)
#       define BIG_ENDIAN __DARWIN_BIG_ENDIAN
#    endif
#endif

#ifndef LITTLE_ENDIAN
#    if defined(_LITTLE_ENDIAN)
#        define LITTLE_ENDIAN _LITTLE_ENDIAN
#    elif defined(__LITTLE_ENDIAN)
#        define LITTLE_ENDIAN __LITTLE_ENDIAN
#    elif defined(__DARWIN_LITTLE_ENDIAN)
#        define LITTLE_ENDIAN __DARWIN_LITTLE_ENDIAN
#    endif
#endif

#ifndef BYTE_ORDER
#    if defined(BIG_ENDIAN) && !defined(LITTLE_ENDIAN)
#       undef BIG_ENDIAN
#       define BIG_ENDIAN 4321
#       define LITTLE_ENDIAN 1234
#       define BYTE_ORDER BIG_ENDIAN
#    elif !defined(BIG_ENDIAN) && defined(LITTLE_ENDIAN)
#       undef LITTLE_ENDIAN
#       define BIG_ENDIAN 4321
#       define LITTLE_ENDIAN 1234
#       define BYTE_ORDER LITTLE_ENDIAN
#    endif
#endif

#if !defined(BYTE_ORDER) || !defined(BIG_ENDIAN)
#    error "Cannot determine byte order"
#endif

#ifdef HAVE_DIRENT_H
#    include <dirent.h>
#    define NAMLEN(dirent) strlen((dirent)->d_name)
#else
#    define dirent direct
#    define NAMLEN(dirent) (dirent)->d_namlen
#    ifdef HAVE_SYS_NDIR_H
#        include <sys/ndir.h>
#    endif
#    ifdef HAVE_SYS_DIR_H
#        include <sys/dir.h>
#    endif
#    ifdef HAVE_NDIR_H
#        include <ndir.h>
#    endif
#endif

#ifndef PACKAGE_NAME
#    define PACKAGE_NAME "ncollectd"
#endif

#ifndef PREFIX
#    define PREFIX "/opt/" PACKAGE_NAME
#endif

#ifndef SYSCONFDIR
#    define SYSCONFDIR PREFIX "/etc"
#endif

#ifndef CONFIGFILE
#    define CONFIGFILE SYSCONFDIR "/ncollectd.conf"
#endif

#ifndef LOCALSTATEDIR
#    define LOCALSTATEDIR PREFIX "/var"
#endif

#ifndef PKGLOCALSTATEDIR
#    define PKGLOCALSTATEDIR PREFIX "/var/lib/" PACKAGE_NAME
#endif

#ifndef PIDFILE
#    define PIDFILE PREFIX "/var/run/" PACKAGE_NAME ".pid"
#endif

#ifndef PLUGINDIR
#    define PLUGINDIR PREFIX "/lib/" PACKAGE_NAME
#endif

#ifndef PKGDATADIR
#    define PKGDATADIR PREFIX "/share/" PACKAGE_NAME
#endif

#ifndef NCOLLECTD_GRP_NAME
#    define NCOLLECTD_GRP_NAME "ncollectd"
#endif

#ifndef UNIXSOCKETPATH
#    define UNIXSOCKETPATH LOCALSTATEDIR "/run/" PACKAGE_NAME "-unixsock"
#endif

#ifndef NCOLLECTD_DEFAULT_INTERVAL
#    define NCOLLECTD_DEFAULT_INTERVAL 10.0
#endif

#ifndef NCOLLECTD_USERAGENT
#    define NCOLLECTD_USERAGENT PACKAGE_NAME "/" PACKAGE_VERSION
#endif

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

#ifndef PRIsz
#    define PRIsz "zu"
#endif
