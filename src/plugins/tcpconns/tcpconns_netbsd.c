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

#ifdef KERNEL_NETBSD
#undef HAVE_SYSCTLBYNAME /* force HAVE_LIBKVM_NLIST path */
#endif

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


#pragma GCC diagnostic ignored "-Wcast-align"

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

static int kread(u_long addr, void *buf, int size)
{
    int status = kvm_read(kvmd, addr, buf, size);
    if (status != size) {
        PLUGIN_ERROR("kvm_read failed (got %i, expected %i): %s\n", status, size, kvm_geterr(kvmd));
        return -1;
    }

    return 0;
}

int conn_init(void)
{
    char buf[_POSIX2_LINE_MAX];
#define N_TCBTABLE 0
    struct nlist nl[] = {{"_tcbtable"}, {""}};

    kvmd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, buf);
    if (kvmd == NULL) {
        PLUGIN_ERROR("kvm_openfiles failed: %s", buf);
        return -1;
    }

    int status = kvm_nlist(kvmd, nl);
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

int conn_read(void)
{
    struct inpcbtable table;
#if (defined(__NetBSD_Version__) && __NetBSD_Version__ <= 699002700)
    struct inpcb *head;
#endif
    struct inpcb *next;
    struct inpcb inpcb;
    struct tcpcb tcpcb;

    /* Read the pcbtable from the kernel */
    int status = kread(inpcbtable_off, &table, sizeof(table));
    if (status != 0)
        return -1;

#if (defined(__NetBSD_Version__) && __NetBSD_Version__ > 699002700)
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
#if (defined(__NetBSD_Version__) && __NetBSD_Version__ > 699002700)
        /* inpt_queue is a TAILQ on OpenBSD */
        next = (struct inpcb *)TAILQ_NEXT(&inpcb, inp_queue);
#else
        next = (struct inpcb *)CIRCLEQ_NEXT(&inpcb, inp_queue);
#endif

/* Ignore sockets, that are not connected. */
        if (inpcb.inp_af == AF_INET6)
            continue; /* XXX see netbsd/src/usr.bin/netstat/inet6.c */

        status = kread((u_long)inpcb.inp_ppcb, &tcpcb, sizeof(tcpcb));
        if (status != 0)
            return -1;

        if (inpcb.inp_flags & INP_IPV4) {
            if (inet_lnaof(inpcb.inp_laddr) == INADDR_ANY)
                continue;

            struct sockaddr_in saddr = {
                .sin_family      = AF_INET,
                .sin_port        = ntohs(inpcb.inp_lport),
                .sin_addr.s_addr = inpcb.inp_laddr.s_addr
            };

            struct sockaddr_in daddr = {
                .sin_family      = AF_INET,
                .sin_port        = ntohs(inpcb.inp_fport),
                .sin_addr.s_addr = inpcb.inp_faddr.s_addr
            };

            conn_handle_ports((struct sockaddr *)&saddr, (struct sockaddr *)&daddr,
                              tp->t_state, TCP_STATE_MIN, TCP_STATE_MAX);
        } else if (inpcb.inp_flags & INP_IPV6) {
            if (IN6_IS_ADDR_UNSPECIFIED(&inpcb.inp_laddr6))
                continue;

            struct sockaddr_in6 saddr = {
                .sin6_family       = AF_INET6,
                .sin6_port         = ntohs(inpcb.inp_lport)
            };
            memcpy(saddr.sin6_addr.s6_addr, inpcb.in6p_laddr.s6_addr,
                   sizeof(saddr.sin6_addr.s6_addr));

            struct sockaddr_in6 daddr = {
                .sin6_family       = AF_INET6,
                .sin6_port         = ntohs(inpcb.inp_fport)
            };
            memcpy(daddr.sin6_addr.s6_addr, inpcb.in6p_faddr.s6_addr,
                   sizeof(daddr.sin6_addr.s6_addr));

            conn_handle_ports((struct sockaddr *)&saddr, (struct sockaddr *)&daddr,
                              tcpcb.t_state, TCP_STATE_MIN, TCP_STATE_MAX);
        }
    }

    conn_submit_all(tcp_state, TCP_STATE_MIN, TCP_STATE_MAX);

    return 0;
}
