/* SPDX-License-Identifier: LGPL-2.1-or-later                           */
/* SPDX-FileCopyrightText: Copyright (C) 2006-2017 Florian octo Forster */
/* SPDX-FileContributor: Florian octo Forster <ff at octo.it>           */

#pragma once

#define PING_ERRMSG_LEN 256
#define PING_TABLE_LEN 5381

struct pinghost {
    /* username: name passed in by the user */
    char *username;
    /* hostname: name returned by the reverse lookup */
    char *hostname;
    struct sockaddr_storage *addr;
    socklen_t addrlen;
    int addrfamily;
    int ident;
    int sequence;
    struct timeval *timer;
    double latency;
    uint32_t dropped;
    int recv_ttl;
    uint8_t recv_qos;
    char *data;
    void *context;
    struct pinghost *next;
    struct pinghost *table_next;
};
typedef struct pinghost pinghost_t;

struct pingobj {
    double timeout;
    int ttl;
    int addrfamily;
    uint8_t qos;
    char *data;
    int fd4;
    int fd6;
    struct sockaddr *srcaddr;
//    struct sockaddr_storage *srcaddr;
    socklen_t srcaddrlen;
    char *device;
    char set_mark;
    int mark;
    char errmsg[PING_ERRMSG_LEN];
    pinghost_t *head;
    pinghost_t *table[PING_TABLE_LEN];
};
typedef struct pingobj pingobj_t;

typedef pinghost_t pingobj_iter_t;

#define PING_OPT_TIMEOUT 0x01
#define PING_OPT_TTL     0x02
#define PING_OPT_AF      0x04
#define PING_OPT_DATA    0x08
#define PING_OPT_SOURCE  0x10
#define PING_OPT_DEVICE  0x20
#define PING_OPT_QOS     0x40
#define PING_OPT_MARK    0x80

#define PING_DEF_TIMEOUT 1.0
#define PING_DEF_TTL     255
#define PING_DEF_AF      AF_UNSPEC
#define PING_DEF_DATA    "liboping -- ICMP ping library <http://octo.it/liboping/>"

pingobj_t *ping_construct (void);

void ping_destroy (pingobj_t *obj);

int ping_setopt (pingobj_t *obj, int option, void *value);

int ping_send (pingobj_t *obj);

int ping_host_add (pingobj_t *obj, const char *host);

int ping_host_remove (pingobj_t *obj, const char *host);

int ping_host_resolve (pingobj_t *obj, pinghost_t *ph, const char *host);

const char *ping_get_error (pingobj_t *obj);
