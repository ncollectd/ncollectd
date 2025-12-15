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

#include <sys/sysctl.h>
#include <sys/types.h>
#define _KERNEL /* for DTYPE_SOCKET */
#include <sys/file.h>
#undef _KERNEL

#include <netinet/in.h>

#include <kvm.h>

#pragma GCC diagnostic ignored "-Wcast-align"

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

int conn_read(void)
{
    int fcnt = 0;
    struct kinfo_file *kf = kvm_getfiles(kvmd, KERN_FILE_BYFILE, DTYPE_SOCKET, sizeof(*kf), &fcnt);
    if (kf == NULL) {
        PLUGIN_ERROR("kvm_getfiles failed.");
        return -1;
    }

    for (int i = 0; i < fcnt; i++) {
        if (kf[i].so_protocol != IPPROTO_TCP)
            continue;
        if (kf[i].inp_fport == 0)
            continue;

        if (ki[i].so_family == AF_INET) {
            struct sockaddr_in saddr = {
                .sin_family      = AF_INET,
                .sin_port        = ntohs(ki[i].inp_lport),
                .sin_addr.s_addr = ki[i].inp_laddru[0]
            };

            struct sockaddr_in daddr = {
                .sin_family      = AF_INET,
                .sin_port        = ntohs(ki[i].inp_fport),
                .sin_addr.s_addr = ki[i].inp_faddru[0]
            };

            conn_handle_ports((struct sockaddr *)&saddr, (struct sockaddr *)&daddr,
                              r->idiag_state, TCP_STATE_MIN, TCP_STATE_MAX);
        } else if (ki[i].so_family == AF_INET6) {
            struct sockaddr_in6 saddr = {
                .sin6_family       = AF_INET6,
                .sin6_port         = ntohs(ki[i].inp_lport),
            };
            memcpy(saddr.sin6_addr.s6_addr, ki[i].inp_laddru, sizeof(saddr.sin6_addr.s6_addr));

            struct sockaddr_in6 daddr = {
                .sin6_family       = AF_INET6,
                .sin6_port         = ntohs(ki[i].inp_fport),
            };
            memcpy(daddr.sin6_addr.s6_addr, ki[i].inp_faddru, sizeof(daddr.sin6_addr.s6_addr));

            conn_handle_ports((struct sockaddr *)&saddr, (struct sockaddr *)&daddr,
                              kf[i].t_state, TCP_STATE_MIN, TCP_STATE_MAX);
        }
    }

    conn_submit_all(tcp_state, TCP_STATE_MIN, TCP_STATE_MAX);

    return 0;
}

int conn_init(void)
{
    char buf[_POSIX2_LINE_MAX];

    kvmd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, buf);
    if (kvmd == NULL) {
        PLUGIN_ERROR("kvm_openfiles failed: %s", buf);
        return -1;
    }

    return 0;
}

