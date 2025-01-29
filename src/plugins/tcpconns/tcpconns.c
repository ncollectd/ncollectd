// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2007,2008 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2008 Michael Stapelberg
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Michael Stapelberg <michael+git at stapelberg.de>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

/**
 * Code within `HAVE_LIBKVM_NLIST' blocks is provided under the following
 * license:
 *
 * $collectd: parts of tcpconns.c, 2008/08/08 03:48:30 Michael Stapelberg $
 * $OpenBSD: inet.c,v 1.100 2007/06/19 05:28:30 ray Exp $
 * $NetBSD: inet.c,v 1.14 1995/10/03 21:42:37 thorpej Exp $
 *
 * Copyright (c) 1983, 1988, 1993
 *          The Regents of the University of California.    All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "plugin.h"
#include "libutils/common.h"

#ifdef KERNEL_OPENBSD
#define HAVE_KVM_GETFILES 1
#endif

#ifdef KERNEL_NETBSD
#undef HAVE_SYSCTLBYNAME /* force HAVE_LIBKVM_NLIST path */
#endif

#if !defined(KERNEL_LINUX) && \
    !defined(HAVE_SYSCTLBYNAME) && \
    !defined(HAVE_KVM_GETFILES) && \
    !defined(HAVE_LIBKVM_NLIST) && \
    !defined(KERNEL_AIX)
#error "No applicable input method."
#endif

#ifdef KERNEL_LINUX
#include <linux/netlink.h>
#ifdef HAVE_LINUX_INET_DIAG_H
#include <linux/inet_diag.h>
#endif
#include <arpa/inet.h>

/* This is for NetBSD. */
#elif defined(HAVE_LIBKVM_NLIST)
#include <arpa/inet.h>
#include <net/route.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <sys/queue.h>
#ifndef HAVE_BSD_NLIST_H
#include <nlist.h>
#else
#include <bsd/nlist.h>
#endif
#include <kvm.h>

#elif defined(HAVE_SYSCTLBYNAME)
#include <sys/socketvar.h>
#include <sys/sysctl.h>

/* Some includes needed for compiling on FreeBSD */
#include <sys/time.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif

#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>

#elif defined(HAVE_KVM_GETFILES)
#include <sys/sysctl.h>
#include <sys/types.h>
#define _KERNEL /* for DTYPE_SOCKET */
#include <sys/file.h>
#undef _KERNEL

#include <netinet/in.h>

#include <kvm.h>


#elif defined(KERNEL_AIX)
#include <arpa/inet.h>
#include <sys/socketvar.h>
#endif

#pragma GCC diagnostic ignored "-Wcast-align"

#ifdef KERNEL_LINUX
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

#elif defined(HAVE_SYSCTLBYNAME)

static const char *tcp_state[] = {
    "CLOSED",    "LISTEN",      "SYN_SENT",
    "SYN_RECV",  "ESTABLISHED", "CLOSE_WAIT",
    "FIN_WAIT1", "CLOSING",     "LAST_ACK",
    "FIN_WAIT2", "TIME_WAIT"
};

#define TCP_STATE_LISTEN 1
#define TCP_STATE_MIN 0
#define TCP_STATE_MAX 10

#elif defined(HAVE_KVM_GETFILES)

static const char *tcp_state[] = {
    "CLOSED",    "LISTEN",      "SYN_SENT",
    "SYN_RECV",  "ESTABLISHED", "CLOSE_WAIT",
    "FIN_WAIT1", "CLOSING",     "LAST_ACK",
    "FIN_WAIT2", "TIME_WAIT"
};

#define TCP_STATE_LISTEN 1
#define TCP_STATE_MIN 0
#define TCP_STATE_MAX 10

static kvm_t *kvmd;

#elif defined(HAVE_LIBKVM_NLIST)

static const char *tcp_state[] = {
    "CLOSED",    "LISTEN",      "SYN_SENT",
    "SYN_RECV",  "ESTABLISHED", "CLOSE_WAIT",
    "FIN_WAIT1", "CLOSING",     "LAST_ACK",
    "FIN_WAIT2", "TIME_WAIT"
};

static kvm_t *kvmd;
static u_long inpcbtable_off;
struct inpcbtable *inpcbtable_ptr = NULL;

#define TCP_STATE_LISTEN 1
#define TCP_STATE_MIN 1
#define TCP_STATE_MAX 10

#elif defined(KERNEL_AIX)

static const char *tcp_state[] = {
    "CLOSED",    "LISTEN",      "SYN_SENT",
    "SYN_RECV",  "ESTABLISHED", "CLOSE_WAIT",
    "FIN_WAIT1", "CLOSING",     "LAST_ACK",
    "FIN_WAIT2", "TIME_WAIT"};

#define TCP_STATE_LISTEN 1
#define TCP_STATE_MIN 0
#define TCP_STATE_MAX 10

struct netinfo_conn {
    uint32_t unknow1[2];
    uint16_t dstport;
    uint16_t unknow2;
    struct in6_addr dstaddr;
    uint16_t srcport;
    uint16_t unknow3;
    struct in6_addr srcaddr;
    uint32_t unknow4[36];
    uint16_t tcp_state;
    uint16_t unknow5[7];
};

struct netinfo_header {
    unsigned int proto;
    unsigned int size;
};

#define NETINFO_TCP 3
extern int netinfo(int proto, void *data, int *size, int n);
#endif

#define PORT_COLLECT_LOCAL 0x01
#define PORT_COLLECT_REMOTE 0x02
#define PORT_IS_LISTENING 0x04

typedef struct port_entry_s {
    uint16_t port;
    uint16_t flags;
    uint32_t count_local[TCP_STATE_MAX + 1];
    uint32_t count_remote[TCP_STATE_MAX + 1];
    struct port_entry_s *next;
} port_entry_t;

static bool port_collect_listening;
static bool port_collect_total;
static port_entry_t *port_list_head;
static uint32_t count_total[TCP_STATE_MAX + 1];

#ifdef KERNEL_LINUX
#ifdef HAVE_STRUCT_LINUX_INET_DIAG_REQ
/* This depends on linux inet_diag_req because if this structure is missing,
 * sequence_number is useless and we get a compilation warning.
 */
static uint32_t sequence_number;
#endif

static enum { SRC_DUNNO, SRC_NETLINK, SRC_PROC } linux_source = SRC_DUNNO;
#endif

static void conn_submit_all(void)
{
    metric_family_t fam = {
            .name = "system_tcp_connections",
            .type = METRIC_TYPE_GAUGE,
            .help = "Number of TCP connections",
    };

    if (port_collect_total) {
        for (int i = 1; i <= TCP_STATE_MAX; i++) {
            metric_family_append(&fam, VALUE_GAUGE(count_total[i]), NULL,
                                 &(label_pair_const_t){.name="port", .value="all"},
                                 &(label_pair_const_t){.name="state", .value=tcp_state[i]},
                                 NULL);
        }
    }

    for (port_entry_t *pe = port_list_head; pe != NULL; pe = pe->next) {
        if ((port_collect_listening && (pe->flags & PORT_IS_LISTENING)) ||
                (pe->flags & PORT_COLLECT_LOCAL)) {
            char port[64];
            snprintf(port, sizeof(port), "%" PRIu16 "-local", pe->port);

            for (int i = 1; i <= TCP_STATE_MAX; i++) {
                metric_family_append(&fam, VALUE_GAUGE(pe->count_local[i]), NULL,
                                     &(label_pair_const_t){.name="port", .value=port},
                                     &(label_pair_const_t){.name="state", .value=tcp_state[i]},
                                     NULL);
            }
        }

        if (pe->flags & PORT_COLLECT_REMOTE) {
            char port[64];
            snprintf(port, sizeof(port), "%" PRIu16 "-remote", pe->port);

            for (int i = 1; i <= TCP_STATE_MAX; i++) {
                metric_family_append(&fam, VALUE_GAUGE(pe->count_remote[i]), NULL,
                                     &(label_pair_const_t){.name="port", .value=port},
                                     &(label_pair_const_t){.name="state", .value=tcp_state[i]},
                                     NULL);
            }
        }
    }

    plugin_dispatch_metric_family(&fam, 0);
}

static port_entry_t *conn_get_port_entry(uint16_t port, int create)
{
    port_entry_t *ret = port_list_head;
    while (ret != NULL) {
        if (ret->port == port)
            break;
        ret = ret->next;
    }

    if ((ret == NULL) && (create != 0)) {
        ret = calloc(1, sizeof(*ret));
        if (ret == NULL)
            return NULL;

        ret->port = port;
        ret->next = port_list_head;
        port_list_head = ret;
    }

    return ret;
}

/* Removes ports that were added automatically due to the `ListeningPorts'
 * setting but which are no longer listening. */
static void conn_reset_port_entry(void)
{
    port_entry_t *prev = NULL;
    port_entry_t *pe = port_list_head;

    memset(&count_total, '\0', sizeof(count_total));

    while (pe != NULL) {
        /* If this entry was created while reading the files (ant not when handling
         * the configuration) remove it now. */
        if ((pe->flags & (PORT_COLLECT_LOCAL | PORT_COLLECT_REMOTE | PORT_IS_LISTENING)) == 0) {
            port_entry_t *next = pe->next;

            PLUGIN_DEBUG("Removing temporary entry " "for listening port %" PRIu16, pe->port);

            if (prev == NULL)
                port_list_head = next;
            else
                prev->next = next;

            free(pe);
            pe = next;

            continue;
        }

        memset(pe->count_local, '\0', sizeof(pe->count_local));
        memset(pe->count_remote, '\0', sizeof(pe->count_remote));
        pe->flags &= ~PORT_IS_LISTENING;

        prev = pe;
        pe = pe->next;
    }
}

static int conn_handle_ports(uint16_t port_local, uint16_t port_remote, uint8_t state)
{
    port_entry_t *pe = NULL;

    if ((state > TCP_STATE_MAX)
#if TCP_STATE_MIN > 0
        || (state < TCP_STATE_MIN)
#endif
    ) {
        PLUGIN_NOTICE("Ignoring connection with unknown state 0x%02" PRIx8 ".", state);
        return -1;
    }

    count_total[state]++;

    /* Listening sockets */
    if ((state == TCP_STATE_LISTEN) && port_collect_listening) {
        pe = conn_get_port_entry(port_local, 1 /* create */);
        if (pe != NULL)
            pe->flags |= PORT_IS_LISTENING;
    }

    PLUGIN_DEBUG("Connection %" PRIu16 " <-> %" PRIu16 " (%s)",
                 port_local, port_remote, tcp_state[state]);

    pe = conn_get_port_entry(port_local, 0 /* no create */);
    if (pe != NULL)
        pe->count_local[state]++;

    pe = conn_get_port_entry(port_remote, 0 /* no create */);
    if (pe != NULL)
        pe->count_remote[state]++;

    return 0;
}

#ifdef KERNEL_LINUX
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
                struct nlmsgerr *msg_error;

                msg_error = NLMSG_DATA(h);
                PLUGIN_WARNING("conn_read_netlink: Received error %i.", msg_error->error);

                close(fd);
                return 1;
            }

            r = NLMSG_DATA(h);

            /* This code does not (need to) distinguish between IPv4 and IPv6. */
            conn_handle_ports(ntohs(r->id.idiag_sport), ntohs(r->id.idiag_dport), r->idiag_state);

            h = NLMSG_NEXT(h, status);
        }
    }

    /* Not reached because the while() loop above handles the exit condition. */
    return 0;
#else
    return 1;
#endif
}

static int conn_handle_line(char *buffer)
{
    char *fields[32];
    int fields_len;

    char *endptr;

    char *port_local_str;
    char *port_remote_str;
    uint16_t port_local;
    uint16_t port_remote;

    uint8_t state;

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

    port_local_str = strchr(fields[1], ':');
    port_remote_str = strchr(fields[2], ':');

    if ((port_local_str == NULL) || (port_remote_str == NULL))
        return -1;
    port_local_str++;
    port_remote_str++;
    if ((*port_local_str == '\0') || (*port_remote_str == '\0'))
        return -1;

    endptr = NULL;
    port_local = (uint16_t)strtol(port_local_str, &endptr, 16);
    if ((endptr == NULL) || (*endptr != '\0'))
        return -1;

    endptr = NULL;
    port_remote = (uint16_t)strtol(port_remote_str, &endptr, 16);
    if ((endptr == NULL) || (*endptr != '\0'))
        return -1;

    endptr = NULL;
    state = (uint8_t)strtol(fields[3], &endptr, 16);
    if ((endptr == NULL) || (*endptr != '\0'))
        return -1;

    return conn_handle_ports(port_local, port_remote, state);
}

static int conn_read_file(const char *file)
{
    FILE *fh;
    char buffer[1024];

    fh = fopen(file, "r");
    if (fh == NULL)
        return -1;

    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        conn_handle_line(buffer);
    }

    fclose(fh);

    return 0;
}

#elif defined(HAVE_SYSCTLBYNAME)

#elif defined(HAVE_LIBKVM_NLIST)
#endif

#ifdef KERNEL_LINUX
static int conn_init(void)
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

    if (port_collect_total == 0 && port_list_head == NULL)
        port_collect_listening = 1;

    return 0;
}

static int conn_read(void)
{
    int status;

    conn_reset_port_entry();

    if (linux_source == SRC_NETLINK) {
        status = conn_read_netlink();
    } else if (linux_source == SRC_PROC) {
        int errors_num = 0;

        if (conn_read_file(path_proc_tcp) != 0)
            errors_num++;
        if (conn_read_file(path_proc_tcp6) != 0)
            errors_num++;

        if (errors_num < 2)
            status = 0;
        else
            status = ENOENT;
    } else /* if (linux_source == SRC_DUNNO) */
    {
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
        conn_submit_all();
    else
        return status;

    return 0;
}
#elif defined(HAVE_SYSCTLBYNAME)

static int conn_read(void)
{
    int status;
    char *buffer;
    size_t buffer_len;
    ;

    struct xinpgen *in_orig;
    struct xinpgen *in_ptr;

    conn_reset_port_entry();

    buffer_len = 0;
    status = sysctlbyname("net.inet.tcp.pcblist", NULL, &buffer_len, 0, 0);
    if (status < 0) {
        PLUGIN_ERROR("sysctlbyname failed.");
        return -1;
    }

    buffer = malloc(buffer_len);
    if (buffer == NULL) {
        PLUGIN_ERROR("malloc failed.");
        return -1;
    }

    status = sysctlbyname("net.inet.tcp.pcblist", buffer, &buffer_len, 0, 0);
    if (status < 0) {
        PLUGIN_ERROR("sysctlbyname failed.");
        free(buffer);
        return -1;
    }

    if (buffer_len <= sizeof(struct xinpgen)) {
        PLUGIN_ERROR("(buffer_len <= sizeof (struct xinpgen))");
        free(buffer);
        return -1;
    }

    in_orig = (struct xinpgen *)buffer;
    for (in_ptr = (struct xinpgen *)(((char *)in_orig) + in_orig->xig_len);
             in_ptr->xig_len > sizeof(struct xinpgen);
             in_ptr = (struct xinpgen *)(((char *)in_ptr) + in_ptr->xig_len)) {
#if __FreeBSD_version >= 1200026
        struct xtcpcb *tp = (struct xtcpcb *)in_ptr;
        struct xinpcb *inp = &tp->xt_inp;
        struct xsocket *so = &inp->xi_socket;
#else
        struct tcpcb *tp = &((struct xtcpcb *)in_ptr)->xt_tp;
        struct inpcb *inp = &((struct xtcpcb *)in_ptr)->xt_inp;
        struct xsocket *so = &((struct xtcpcb *)in_ptr)->xt_socket;
#endif

        /* Ignore non-TCP sockets */
        if (so->xso_protocol != IPPROTO_TCP)
            continue;

        /* Ignore PCBs which were freed during copyout. */
        if (inp->inp_gencnt > in_orig->xig_gen)
            continue;

        if (((inp->inp_vflag & INP_IPV4) == 0) &&
                ((inp->inp_vflag & INP_IPV6) == 0))
            continue;

        conn_handle_ports(ntohs(inp->inp_lport), ntohs(inp->inp_fport), tp->t_state);
    }

    in_orig = NULL;
    in_ptr = NULL;
    free(buffer);

    conn_submit_all();

    return 0;
}
#elif defined(HAVE_KVM_GETFILES)

static int conn_init(void)
{
    char buf[_POSIX2_LINE_MAX];

    kvmd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, buf);
    if (kvmd == NULL) {
        PLUGIN_ERROR("kvm_openfiles failed: %s", buf);
        return -1;
    }

    return 0;
}

static int conn_read(void)
{
    struct kinfo_file *kf;
    int i, fcnt;

    conn_reset_port_entry();

    kf = kvm_getfiles(kvmd, KERN_FILE_BYFILE, DTYPE_SOCKET, sizeof(*kf), &fcnt);
    if (kf == NULL) {
        PLUGIN_ERROR("kvm_getfiles failed.");
        return -1;
    }

    for (i = 0; i < fcnt; i++) {
        if (kf[i].so_protocol != IPPROTO_TCP)
            continue;
        if (kf[i].inp_fport == 0)
            continue;
        conn_handle_ports(ntohs(kf[i].inp_lport), ntohs(kf[i].inp_fport), kf[i].t_state);
    }

    conn_submit_all();

    return 0;
}
#elif defined(HAVE_LIBKVM_NLIST)

static int kread(u_long addr, void *buf, int size)
{
    int status;

    status = kvm_read(kvmd, addr, buf, size);
    if (status != size) {
        PLUGIN_ERROR("kvm_read failed (got %i, expected %i): %s\n", status, size, kvm_geterr(kvmd));
        return -1;
    }
    return 0;
}

static int conn_init(void)
{
    char buf[_POSIX2_LINE_MAX];
    struct nlist nl[] = {
#define N_TCBTABLE 0
            {"_tcbtable"}, {""}};
    int status;

    kvmd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, buf);
    if (kvmd == NULL) {
        PLUGIN_ERROR("kvm_openfiles failed: %s", buf);
        return -1;
    }

    status = kvm_nlist(kvmd, nl);
    if (status < 0) {
        PLUGIN_ERROR("kvm_nlist failed with status %i.", status);
        return -1;
    }

    if (nl[N_TCBTABLE].n_type == 0) {
        PLUGIN_ERROR("Error looking up kernel's namelist: N_TCBTABLE is invalid.");
        return -1;
    }

    inpcbtable_off = (u_long)nl[N_TCBTABLE].n_value;
    inpcbtable_ptr = (struct inpcbtable *)nl[N_TCBTABLE].n_value;

    return 0;
}

static int conn_read(void)
{
    struct inpcbtable table;
#if !defined(__OpenBSD__) && (defined(__NetBSD_Version__) && __NetBSD_Version__ <= 699002700)
    struct inpcb *head;
#endif
    struct inpcb *next;
    struct inpcb inpcb;
    struct tcpcb tcpcb;
    int status;

    conn_reset_port_entry();

    /* Read the pcbtable from the kernel */
    status = kread(inpcbtable_off, &table, sizeof(table));
    if (status != 0)
        return -1;

#if defined(__OpenBSD__) || (defined(__NetBSD_Version__) && __NetBSD_Version__ > 699002700)
    /* inpt_queue is a TAILQ on OpenBSD */
    /* Get the first pcb */
    next = (struct inpcb *)TAILQ_FIRST(&table.inpt_queue);
    while (next)
#else
    /* Get the `head' pcb */
    head = (struct inpcb *)&(inpcbtable_ptr->inpt_queue);
    /* Get the first pcb */
    next = (struct inpcb *)CIRCLEQ_FIRST(&table.inpt_queue);

    while (next != head)
#endif
    {
        /* Read the pcb pointed to by `next' into `inpcb' */
        status = kread((u_long)next, &inpcb, sizeof(inpcb));
        if (status != 0)
            return -1;

/* Advance `next' */
#if defined(__OpenBSD__) || (defined(__NetBSD_Version__) && __NetBSD_Version__ > 699002700)
        /* inpt_queue is a TAILQ on OpenBSD */
        next = (struct inpcb *)TAILQ_NEXT(&inpcb, inp_queue);
#else
        next = (struct inpcb *)CIRCLEQ_NEXT(&inpcb, inp_queue);
#endif

/* Ignore sockets, that are not connected. */
#ifdef __NetBSD__
        if (inpcb.inp_af == AF_INET6)
            continue; /* XXX see netbsd/src/usr.bin/netstat/inet6.c */
#else
        if (!(inpcb.inp_flags & INP_IPV6) &&
                (inet_lnaof(inpcb.inp_laddr) == INADDR_ANY))
            continue;
        if ((inpcb.inp_flags & INP_IPV6) &&
                IN6_IS_ADDR_UNSPECIFIED(&inpcb.inp_laddr6))
            continue;
#endif

        status = kread((u_long)inpcb.inp_ppcb, &tcpcb, sizeof(tcpcb));
        if (status != 0)
            return -1;
        conn_handle_ports(ntohs(inpcb.inp_lport), ntohs(inpcb.inp_fport), tcpcb.t_state);
    }

    conn_submit_all();

    return 0;
}
#elif defined(KERNEL_AIX)

static int conn_read(void)
{
    int size;
    int nconn;
    void *data;
    struct netinfo_header *header;
    struct netinfo_conn *conn;

    conn_reset_port_entry();

    size = netinfo(NETINFO_TCP, 0, 0, 0);
    if (size < 0) {
        PLUGIN_ERROR("netinfo failed return: %i", size);
        return -1;
    }

    if (size == 0)
        return 0;

    if ((size - sizeof(struct netinfo_header)) % sizeof(struct netinfo_conn)) {
        PLUGIN_ERROR("invalid buffer size");
        return -1;
    }

    data = malloc(size);
    if (data == NULL) {
        PLUGIN_ERROR("malloc failed");
        return -1;
    }

    if (netinfo(NETINFO_TCP, data, &size, 0) < 0) {
        PLUGIN_ERROR("netinfo failed");
        free(data);
        return -1;
    }

    header = (struct netinfo_header *)data;
    nconn = header->size;
    conn = (struct netinfo_conn *)((unsigned char *)data + sizeof(struct netinfo_header));

    for (int i = 0; i < nconn; conn++, i++) {
        conn_handle_ports(conn->srcport, conn->dstport, conn->tcp_state);
    }

    free(data);

    conn_submit_all();

    return 0;
}
#endif

static int conn_config_port(config_item_t *ci, uint16_t flags)
{
    int port = 0;

    int status = cf_util_get_int(ci, &port);
    if (status != 0)
        return -1;

    if ((port < 1) || (port > 65535)) {
        PLUGIN_ERROR("Invalid port: %i in %s:%d.", port, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    port_entry_t *pe = conn_get_port_entry((uint16_t)port, 1 /* create */);
    if (pe == NULL) {
        PLUGIN_ERROR("conn_get_port_entry failed.");
        return -1;
    }

    pe->flags |= flags;

    return 0;
}

static int conn_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "listening-ports") == 0) {
            status = cf_util_get_boolean(child, &port_collect_listening);
        } else if (strcasecmp(child->key, "local-port") == 0) {
            status = conn_config_port(child, PORT_COLLECT_LOCAL);
        } else if (strcasecmp(child->key, "remote-port") == 0) {
            status = conn_config_port(child, PORT_COLLECT_REMOTE);
        } else if (strcasecmp(child->key, "all-port-summary") == 0) {
            status = cf_util_get_boolean(child, &port_collect_total);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int conn_shutdown(void)
{
#ifdef KERNEL_LINUX
    free(path_proc_tcp);
    free(path_proc_tcp6);
#endif

    port_entry_t *entry = port_list_head;
    while (entry != NULL) {
        port_entry_t *next = entry->next;
        free(entry);
        entry = next;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("tcpconns", conn_config);
#ifdef KERNEL_LINUX
    plugin_register_init("tcpconns", conn_init);
#elif defined(HAVE_SYSCTLBYNAME)
    /* no initialization */
#elif defined(HAVE_LIBKVM_NLIST) || defined(HAVE_KVM_GETFILES)
    plugin_register_init("tcpconns", conn_init);
#elif defined(KERNEL_AIX)
/* no initialization */
#endif
    plugin_register_read("tcpconns", conn_read);
    plugin_register_shutdown("tcpconns", conn_shutdown);
}
