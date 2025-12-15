// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2007,2008 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2008 Michael Stapelberg
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Michael Stapelberg <michael+git at stapelberg.de>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>


#include "plugin.h"
#include "libutils/common.h"

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

static const char *tcp_state[] = {
    "CLOSED",    "LISTEN",      "SYN_SENT",
    "SYN_RECV",  "ESTABLISHED", "CLOSE_WAIT",
    "FIN_WAIT1", "CLOSING",     "LAST_ACK",
    "FIN_WAIT2", "TIME_WAIT"
};

#define TCP_STATE_LISTEN 1
#define TCP_STATE_MIN 0
#define TCP_STATE_MAX 10

int conn_read(void)
{
    size_t buffer_len = 0;
    int status = sysctlbyname("net.inet.tcp.pcblist", NULL, &buffer_len, 0, 0);
    if (status < 0) {
        PLUGIN_ERROR("sysctlbyname failed.");
        return -1;
    }

    char *buffer = malloc(buffer_len);
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

    struct xinpgen *in_ptr;
    struct xinpgen *in_orig = (struct xinpgen *)buffer;
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

        if (inp->inp_vflag & INP_IPV4) {
            struct sockaddr_in saddr = {
                .sin_family      = AF_INET,
                .sin_port        = ntohs(inp->inp_lport),
                .sin_addr.s_addr = inp->inp_laddr.s_addr
            };

            struct sockaddr_in daddr = {
                .sin_family      = AF_INET,
                .sin_port        = ntohs(inp->inp_fport),
                .sin_addr.s_addr = inp->inp_faddr.s_addr
            };

            conn_handle_ports((struct sockaddr *)&saddr, (struct sockaddr *)&daddr,
                              tp->t_state, TCP_STATE_MIN, TCP_STATE_MAX);
        } else if (inp->inp_vflag & INP_IPV6) {
            struct sockaddr_in6 saddr = {
                .sin6_family       = AF_INET6,
                .sin6_port         = ntohs(inp->inp_lport)
            };
            memcpy(saddr.sin6_addr.s6_addr, inp->in6p_laddr.s6_addr,
                   sizeof(saddr.sin6_addr.s6_addr));

            struct sockaddr_in6 daddr = {
                .sin6_family       = AF_INET6,
                .sin6_port         = ntohs(inp->inp_fport)
            };
            memcpy(daddr.sin6_addr.s6_addr, inp->in6p_faddr.s6_addr,
                   sizeof(daddr.sin6_addr.s6_addr));

            conn_handle_ports((struct sockaddr *)&saddr, (struct sockaddr *)&daddr,
                              tp->t_state, TCP_STATE_MIN, TCP_STATE_MAX);
        }
    }

    free(buffer);

    conn_submit_all(tcp_state, TCP_STATE_MIN, TCP_STATE_MAX);

    return 0;
}
