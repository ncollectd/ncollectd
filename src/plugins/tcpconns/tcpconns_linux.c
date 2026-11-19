// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2007,2008 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2008 Michael Stapelberg
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Michael Stapelberg <michael+git at stapelberg.de>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <linux/netlink.h>
#ifdef HAVE_LINUX_INET_DIAG_H
#include <linux/inet_diag.h>
#endif
#include <arpa/inet.h>

#include "plugins/tcpconns/tcpconns.h"

#ifdef HAVE_STRUCT_LINUX_INET_DIAG_REQ
struct nlreq {
    struct nlmsghdr nlh;
    struct inet_diag_req r;
};
#endif

static char *path_proc_tcp;
static char *path_proc_tcp6;

static const char *tcp_state[] = {
    "",          "ESTABLISHED", "SYN_SENT",
    "SYN_RECV",  "FIN_WAIT1",   "FIN_WAIT2",
    "TIME_WAIT", "CLOSED",      "CLOSE_WAIT",
    "LAST_ACK",  "LISTEN",      "CLOSING"
};

#define TCP_STATE_LISTEN 10
#define TCP_STATE_MIN 1
#define TCP_STATE_MAX 11

/* This depends on linux inet_diag_req because if this structure is missing,
 * sequence_number is useless and we get a compilation warning.
 */
static uint32_t sequence_number;

static enum { SRC_DUNNO, SRC_NETLINK, SRC_PROC } linux_source = SRC_DUNNO;

/* Returns zero on success, less than zero on socket error and greater than
 * zero on other errors. */
static int conn_read_netlink(void)
{
#ifdef HAVE_STRUCT_LINUX_INET_DIAG_REQ
    int fd;
    struct inet_diag_msg *r;
    char buf[8192];

    /* If this fails, it's likely a permission problem. We'll fall back to
     * reading this information from files below. */
    fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_INET_DIAG);
    if (fd < 0) {
        PLUGIN_ERROR("conn_read_netlink: socket(AF_NETLINK, SOCK_RAW, "
                     "NETLINK_INET_DIAG) failed: %s", STRERRNO);
        return -1;
    }

    struct sockaddr_nl nladdr = {.nl_family = AF_NETLINK};

    struct nlreq req = {
        .nlh.nlmsg_len = sizeof(req),
        .nlh.nlmsg_type = TCPDIAG_GETSOCK,
        /* NLM_F_ROOT: return the complete table instead of a single entry.
         * NLM_F_MATCH: return all entries matching criteria (not implemented)
         * NLM_F_REQUEST: must be set on all request messages */
        .nlh.nlmsg_flags = NLM_F_ROOT | NLM_F_MATCH | NLM_F_REQUEST,
        .nlh.nlmsg_pid = 0,
        /* The sequence_number is used to track our messages. Since netlink is not
         * reliable, we don't want to end up with a corrupt or incomplete old
         * message in case the system is/was out of memory. */
        .nlh.nlmsg_seq = ++sequence_number,
        .r.idiag_family = AF_INET,
        .r.idiag_states = 0xfff,
        .r.idiag_ext = 0};

    struct iovec iov = {.iov_base = &req, .iov_len = sizeof(req)};

    struct msghdr msg = {.msg_name = (void *)&nladdr,
                         .msg_namelen = sizeof(nladdr),
                         .msg_iov = &iov,
                         .msg_iovlen = 1};

    if (sendmsg(fd, &msg, 0) < 0) {
        PLUGIN_ERROR("conn_read_netlink: sendmsg(2) failed: %s", STRERRNO);
        close(fd);
        return -1;
    }

    iov.iov_base = buf;
    iov.iov_len = sizeof(buf);

    while (1) {
        struct nlmsghdr *h;

        memset(&msg, 0, sizeof(msg));
        msg.msg_name = (void *)&nladdr;
        msg.msg_namelen = sizeof(nladdr);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        ssize_t status = recvmsg(fd, (void *)&msg, /* flags = */ 0);
        if (status < 0) {
            if ((errno == EINTR) || (errno == EAGAIN))
                continue;

            PLUGIN_ERROR("conn_read_netlink: recvmsg(2) failed: %s", STRERRNO);
            close(fd);
            return -1;
        } else if (status == 0) {
            close(fd);
            PLUGIN_DEBUG("conn_read_netlink: Unexpected zero-sized reply from netlink socket.");
            return 0;
        }

        if (msg.msg_flags & MSG_TRUNC) {
            PLUGIN_ERROR("conn_read_netlink: Truncated reply from netlink socket.");
            close(fd);
            return 1;
        }

        h = (struct nlmsghdr *)buf;
        while (NLMSG_OK(h, status)) {
            if (h->nlmsg_seq != sequence_number) {
                h = NLMSG_NEXT(h, status);
                continue;
            }

            if (h->nlmsg_type == NLMSG_DONE) {
                close(fd);
                return 0;
            } else if (h->nlmsg_type == NLMSG_ERROR) {
                if (h->nlmsg_len >= NLMSG_LENGTH(sizeof(struct nlmsgerr))) {
                    struct nlmsgerr *msg_error = NLMSG_DATA(h);
                    PLUGIN_WARNING("conn_read_netlink: Received error %i.", msg_error->error);
                } else {
                    PLUGIN_WARNING("conn_read_netlink: Received truncated error message.");
                }

                close(fd);
                return 1;
            }

            if ((h->nlmsg_type != TCPDIAG_GETSOCK) || (h->nlmsg_len < NLMSG_LENGTH(sizeof(*r)))) {
                h = NLMSG_NEXT(h, status);
                continue;
            }

            r = NLMSG_DATA(h);

            if (r->idiag_family == AF_INET) {
                struct sockaddr_in saddr = {
                    .sin_family      = AF_INET,
                    .sin_port        = ntohs(r->id.idiag_sport),
                    .sin_addr.s_addr = r->id.idiag_src[0]
                };

                struct sockaddr_in daddr = {
                    .sin_family      = AF_INET,
                    .sin_port        = ntohs(r->id.idiag_dport),
                    .sin_addr.s_addr = r->id.idiag_dst[0]
                };

                conn_handle_ports((struct sockaddr *)&saddr, (struct sockaddr *)&daddr,
                                  r->idiag_state, TCP_STATE_MIN, TCP_STATE_MAX);
            } else if (r->idiag_family == AF_INET6) {
                struct sockaddr_in6 saddr = {
                    .sin6_family       = AF_INET6,
                    .sin6_port         = ntohs(r->id.idiag_sport),
                };
                memcpy(saddr.sin6_addr.s6_addr, r->id.idiag_src, sizeof(saddr.sin6_addr.s6_addr));

                struct sockaddr_in6 daddr = {
                    .sin6_family       = AF_INET6,
                    .sin6_port         = ntohs(r->id.idiag_dport),
                };
                memcpy(daddr.sin6_addr.s6_addr, r->id.idiag_dst, sizeof(daddr.sin6_addr.s6_addr));

                conn_handle_ports((struct sockaddr *)&saddr, (struct sockaddr *)&daddr,
                                  r->idiag_state, TCP_STATE_MIN, TCP_STATE_MAX);
            }
            

            h = NLMSG_NEXT(h, status);
        }
    }

    /* Not reached because the while() loop above handles the exit condition. */
    return 0;
#else
    return 1;
#endif
}

static int hex2int(char c)
{
    if ((c >= '0') && (c <= '9'))
        return c - '0';
    if ((c >= 'a') && (c <='f'))
        return c - 'a' + 10;
    if ((c >= 'A') && (c <='F'))
        return c - 'A' + 10;
    return -1;
}

/* The kernel prints the address as 32-bit words in host byte order ("%08X"),
 * so each word is stored back as is to get the address in network byte order. */
static int conn_parse_addr(const char *str, void *addr, size_t addr_size)
{
    if (strlen(str) != (2 * addr_size))
        return -1;

    for (size_t i = 0; i < (addr_size / sizeof(uint32_t)); i++) {
        uint32_t word = 0;
        for (size_t j = 0; j < (2 * sizeof(uint32_t)); j++) {
            int digit = hex2int(str[i * 2 * sizeof(uint32_t) + j]);
            if (digit < 0)
                return -1;
            word = (word << 4) | (uint32_t)digit;
        }
        memcpy((uint8_t *)addr + i * sizeof(uint32_t), &word, sizeof(uint32_t));
    }

    return 0;
}

static int conn_handle_line(char *buffer, int family)
{
    char *fields[32];
    int fields_len;

    size_t buffer_len = strlen(buffer);

    while ((buffer_len > 0) && (buffer[buffer_len - 1] < 32))
        buffer[--buffer_len] = '\0';
    if (buffer_len == 0)
        return -1;

    fields_len = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
    if (fields_len < 12) {
        PLUGIN_DEBUG("Got %i fields, expected at least 12.", fields_len);
        return -1;
    }

    char *ip_local_str = fields[1];
    char *ip_remote_str = fields[2];

    char *port_local_str = strchr(ip_local_str, ':');
    char *port_remote_str = strchr(ip_remote_str, ':');

    if ((port_local_str == NULL) || (port_remote_str == NULL))
        return -1;
    *port_local_str = '\0';
    port_local_str++;
    *port_remote_str = '\0';
    port_remote_str++;
    if ((*ip_local_str == '\0') || (*ip_remote_str == '\0'))
        return -1;
    if ((*port_local_str == '\0') || (*port_remote_str == '\0'))
        return -1;

    char *endptr = NULL;
    uint16_t port_local = (uint16_t)strtol(port_local_str, &endptr, 16);
    if ((endptr == NULL) || (*endptr != '\0'))
        return -1;

    endptr = NULL;
    uint16_t port_remote = (uint16_t)strtol(port_remote_str, &endptr, 16);
    if ((endptr == NULL) || (*endptr != '\0'))
        return -1;

    endptr = NULL;
    uint8_t state = (uint8_t)strtol(fields[3], &endptr, 16);
    if ((endptr == NULL) || (*endptr != '\0'))
        return -1;

    if (family == AF_INET) {
        struct sockaddr_in saddr = {
            .sin_family      = AF_INET,
            .sin_port        = port_local,
        };
        if (conn_parse_addr(ip_local_str, &saddr.sin_addr, sizeof(saddr.sin_addr)) != 0)
            return -1;

        struct sockaddr_in daddr = {
            .sin_family      = AF_INET,
            .sin_port        = port_remote,
        };
        if (conn_parse_addr(ip_remote_str, &daddr.sin_addr, sizeof(daddr.sin_addr)) != 0)
            return -1;

        return conn_handle_ports((struct sockaddr *)&saddr, (struct sockaddr *)&daddr, state,
                                 TCP_STATE_MIN, TCP_STATE_MAX);
    } else if (family == AF_INET6) {
        struct sockaddr_in6 saddr = {
            .sin6_family       = AF_INET6,
            .sin6_port         = port_local,
        };
        if (conn_parse_addr(ip_local_str, &saddr.sin6_addr, sizeof(saddr.sin6_addr)) != 0)
            return -1;

        struct sockaddr_in6 daddr = {
            .sin6_family       = AF_INET6,
            .sin6_port         = port_remote,
        };
        if (conn_parse_addr(ip_remote_str, &daddr.sin6_addr, sizeof(daddr.sin6_addr)) != 0)
            return -1;

        return conn_handle_ports((struct sockaddr *)&saddr, (struct sockaddr *)&daddr, state,
                                  TCP_STATE_MIN, TCP_STATE_MAX);
    }

    return 0;
}

static int conn_read_file(const char *file, int family)
{
    FILE *fh;
    char buffer[1024];

    fh = fopen(file, "r");
    if (fh == NULL)
        return -1;

    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        conn_handle_line(buffer, family);
    }

    fclose(fh);

    return 0;
}

int conn_read(void)
{
    int status;

    if (linux_source == SRC_NETLINK) {
        status = conn_read_netlink();
    } else if (linux_source == SRC_PROC) {
        int errors_num = 0;

        if (conn_read_file(path_proc_tcp, AF_INET) != 0)
            errors_num++;
        if (conn_read_file(path_proc_tcp6, AF_INET6) != 0)
            errors_num++;

        if (errors_num < 2)
            status = 0;
        else
            status = ENOENT;
    } else { /* if (linux_source == SRC_DUNNO) */
        /* Try to use netlink for getting this data, it is _much_ faster on systems
         * with a large amount of connections. */
        status = conn_read_netlink();
        if (status == 0) {
            PLUGIN_INFO("Reading from netlink succeeded. Will use the netlink method from now on.");
            linux_source = SRC_NETLINK;
        } else {
            PLUGIN_INFO("Reading from netlink failed. Will read from /proc from now on.");
            linux_source = SRC_PROC;

            /* return success here to avoid the "plugin failed" message. */
            return 0;
        }
    }

    if (status == 0)
        conn_submit_all(tcp_state, TCP_STATE_MIN, TCP_STATE_MAX);
    else
        return status;

    return 0;
}

int conn_init(void)
{
    path_proc_tcp = plugin_procpath("net/tcp");
    if (path_proc_tcp == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    path_proc_tcp6 = plugin_procpath("net/tcp6");
    if (path_proc_tcp6 == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }


    return 0;
}

int conn_shutdown(void)
{
    free(path_proc_tcp);
    free(path_proc_tcp6);

    return 0;
}

