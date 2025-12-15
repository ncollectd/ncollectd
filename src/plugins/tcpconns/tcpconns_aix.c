// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2007,2008 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2008 Michael Stapelberg
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Michael Stapelberg <michael+git at stapelberg.de>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"


#include <arpa/inet.h>
#include <sys/socketvar.h>

#pragma GCC diagnostic ignored "-Wcast-align"

static const char *tcp_state[] = {
    "CLOSED",    "LISTEN",      "SYN_SENT",
    "SYN_RECV",  "ESTABLISHED", "CLOSE_WAIT",
    "FIN_WAIT1", "CLOSING",     "LAST_ACK",
    "FIN_WAIT2", "TIME_WAIT"};

#define TCP_STATE_LISTEN 1
#define TCP_STATE_MIN 0
#define TCP_STATE_MAX 10

struct netinfo_conn {
    uint32_t unknown1[2];
    uint16_t dstport;
    uint16_t unknown2;
    struct in6_addr dstaddr;
    uint16_t srcport;
    uint16_t unknown3;
    struct in6_addr srcaddr;
    uint32_t unknown4[36];
    uint16_t tcp_state;
    uint16_t unknown5[7];
};

struct netinfo_header {
    unsigned int proto;
    unsigned int size;
};

#define NETINFO_TCP 3
extern int netinfo(int proto, void *data, int *size, int n);

int conn_read(void)
{
    int size = netinfo(NETINFO_TCP, 0, 0, 0);
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

    void *data = malloc(size);
    if (data == NULL) {
        PLUGIN_ERROR("malloc failed");
        return -1;
    }

    if (netinfo(NETINFO_TCP, data, &size, 0) < 0) {
        PLUGIN_ERROR("netinfo failed");
        free(data);
        return -1;
    }

    struct netinfo_header *header = (struct netinfo_header *)data;
    int nconn = header->size;
    struct netinfo_conn *conn = (struct netinfo_conn *)((unsigned char *)data +
                                                        sizeof(struct netinfo_header));

    for (int i = 0; i < nconn; conn++, i++) {

        struct sockaddr_in saddr = {
            .sin_family      = AF_INET,
            .sin_port        = conn->srcport,
            .sin_addr.s_addr = conn->srcaddr.s_addr[0]
        };

        struct sockaddr_in daddr = {
            .sin_family      = AF_INET,
            .sin_port        = conn->dstport,
            .sin_addr.s_addr = conn->dstaddr.s_addr[0]
        };

        conn_handle_ports((struct sockaddr *)&saddr, (struct sockaddr *)&daddr,
                          conn->tcp_state, TCP_STATE_MIN, TCP_STATE_MAX);

    }

    free(data);

    conn_submit_all(tcp_state, TCP_STATE_MIN, TCP_STATE_MAX); 

    return 0;
}
