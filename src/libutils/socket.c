// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libutils/common.h"
#include "libutils/itoa.h"
#include "log.h"

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <sys/stat.h>
#include <sys/un.h>

int socket_connect_unix_stream(const char *path, cdtime_t timeout)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        ERROR("socket failed: %s", STRERRNO);
        return -1;
    }

    if (timeout > 0) {
        struct timeval tvout = CDTIME_T_TO_TIMEVAL(timeout);
        int status = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tvout, sizeof(tvout));
        if (status != 0) {
            ERROR("setockopt failed: %s", STRERRNO);
            close(fd);
            return -1;
        }
    }

    struct sockaddr_un sa = {0};
    sa.sun_family = AF_UNIX;
    sstrncpy(sa.sun_path, path, sizeof(sa.sun_path));

    int status = connect(fd, (const struct sockaddr *) &sa, sizeof(sa));
    if (status < 0) {
        ERROR("unix socket connect failed: %s", STRERRNO);
        close(fd);
        return -1;
    }

    return fd;
}

int socket_connect_unix_dgram(const char *localpath, const char *path, cdtime_t timeout)
{
    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) {
        ERROR("socket failed: %s", STRERRNO);
        return -1;
    }

    struct sockaddr_un lsa = {0};
    lsa.sun_family = AF_UNIX;
    sstrncpy(lsa.sun_path, localpath, sizeof(lsa.sun_path));

    int status = unlink(lsa.sun_path);
    if ((status != 0) && (errno != ENOENT)) {
        ERROR("Socket '%s' unlink failed: %s", lsa.sun_path, STRERRNO);
        close(fd);
        return -1;
    }

    /* We need to bind to a specific path, because this is a datagram socket
     * and otherwise the daemon cannot answer. */
    status = bind(fd, (struct sockaddr *)&lsa, sizeof(lsa));
    if (status != 0) {
        ERROR("Socket '%s' bind failed: %s", lsa.sun_path, STRERRNO);
        close(fd);
        return -1;
    }

    /* Make the socket writeable by the daemon.. */
    status = chmod(lsa.sun_path, 0666);
    if (status != 0) {
        ERROR("Socket '%s' chmod failed: %s", lsa.sun_path, STRERRNO);
        close(fd);
        return -1;
    }

    if (timeout > 0) {
        struct timeval tvout =  CDTIME_T_TO_TIMEVAL(timeout);
        status = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tvout, sizeof(struct timeval));
        if (status != 0) {
            ERROR("Socket '%s' setsockopt failed: %s", lsa.sun_path, STRERRNO);
            close(fd);
            return -1;
        }
    }

    struct sockaddr_un sa = {0};
    sa.sun_family = AF_UNIX;
    sstrncpy(sa.sun_path, path, sizeof(sa.sun_path));

    status = connect(fd, (struct sockaddr *)&sa, sizeof(sa));
    if (status != 0) {
        ERROR("Socket '%s' connect failed: %s", sa.sun_path, STRERRNO);
        close(fd);
        return -1;
    }

    unlink(lsa.sun_path);

    return fd;
}

int socket_connect_udp(const char *host, short port, int ttl)
{
    char service[ITOA_MAX];

    uitoa(port, service);

    struct addrinfo ai_hints = {.ai_family = AF_UNSPEC,
                                .ai_flags = AI_ADDRCONFIG,
                                .ai_socktype = SOCK_DGRAM};
    struct addrinfo *ai_list;
    int status = getaddrinfo(host, service, &ai_hints, &ai_list);
    if (status != 0) {
        ERROR("getaddrinfo (%s, %s) failed: %s",
                     host, service, gai_strerror(status));
        return -1;
    }

    if (ai_list == NULL)
        return -1;

    int fd = -1;

    for (struct addrinfo *ai_ptr = ai_list; ai_ptr != NULL; ai_ptr = ai_ptr->ai_next) {
        fd = socket(ai_ptr->ai_family, ai_ptr->ai_socktype, ai_ptr->ai_protocol);
        if (fd < 0) {
            ERROR("failed to open socket: %s", STRERRNO);
            continue;
        }

        status = connect(fd, ai_ptr->ai_addr, ai_ptr->ai_addrlen);
        if (status != 0) {
            ERROR("failed to connect to remote host: %s", STRERRNO);
            close(fd);
            fd = -1;
            continue;
        }

        if (ttl > 0) {
            if (ai_ptr->ai_family == AF_INET) {
                struct sockaddr_in *addr = (struct sockaddr_in *)ai_ptr->ai_addr;
                int optname = IN_MULTICAST(ntohl(addr->sin_addr.s_addr)) ? IP_MULTICAST_TTL
                                                                         : IP_TTL;
                if (setsockopt(fd, IPPROTO_IP, optname, &ttl, sizeof(ttl)) != 0)
                    WARNING("setsockopt(ipv4-ttl): %s", STRERRNO);
            } else if (ai_ptr->ai_family == AF_INET6) {
                struct sockaddr_in6 *addr = (struct sockaddr_in6 *)ai_ptr->ai_addr;
                int optname = IN6_IS_ADDR_MULTICAST(&addr->sin6_addr) ? IPV6_MULTICAST_HOPS
                                                                      : IPV6_UNICAST_HOPS;
                if (setsockopt(fd, IPPROTO_IPV6, optname, &ttl, sizeof(ttl)) != 0)
                    WARNING("setsockopt(ipv6-ttl): %s", STRERRNO);
            }
        }
        break;
    }

    freeaddrinfo(ai_list);

    return fd;
}

int socket_connect_tcp(const char *host, short port, int keep_idle, int keep_int)
{
    char service[ITOA_MAX];

    uitoa(port, service);

    struct addrinfo ai_hints = {.ai_family = AF_UNSPEC,
                                .ai_flags = AI_ADDRCONFIG,
                                .ai_socktype = SOCK_STREAM};
    struct addrinfo *ai_list;
    int status = getaddrinfo(host, service, &ai_hints, &ai_list);
    if (status != 0) {
        ERROR("getaddrinfo (%s, %s) failed: %s",
                     host, service, gai_strerror(status));
        return -1;
    }

    if (ai_list == NULL)
        return -1;

    int fd = -1;

    for (struct addrinfo *ai_ptr = ai_list; ai_ptr != NULL; ai_ptr = ai_ptr->ai_next) {
        fd = socket(ai_ptr->ai_family, ai_ptr->ai_socktype, ai_ptr->ai_protocol);
        if (fd < 0) {
            ERROR("failed to open socket: %s", STRERRNO);
            continue;
        }

        if ((keep_idle > 0) || (keep_int > 0)) {
            status = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &(int){1}, sizeof(int));
            if (status != 0)
                WARNING("failed to set socket keepalive flag");

#ifdef TCP_KEEPIDLE
            if (keep_idle > 0) {
                status = setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keep_idle, sizeof(keep_idle));
                if (status != 0)
                    WARNING("failed to set socket tcp keepalive time");
            }
#endif

#ifdef TCP_KEEPINTVL
            if (keep_intvl > 0) {
                status = setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &keep_int, sizeof(keep_int));
                if (status != 0)
                    WARNING("failed to set socket tcp keepalive interval");
            }
#endif
        }

        status = connect(fd, ai_ptr->ai_addr, ai_ptr->ai_addrlen);
        if (status != 0) {
            ERROR("failed to connect to remote host: %s", STRERRNO);
            close(fd);
            fd = -1;
            continue;
        }

        break;
    }

    freeaddrinfo(ai_list);

    return fd;
}
