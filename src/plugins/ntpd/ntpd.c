// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2006-2012  Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#define _DEFAULT_SOURCE
#define _BSD_SOURCE /* For NI_MAXHOST */

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/time.h"

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h> /* inet_ntoa */
#endif
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#ifndef STA_NANO
#define STA_NANO 0x2000
#endif

/* This option only exists for backward compatibility. If it is false and two
 * ntpd peers use the same refclock driver, the plugin will try to write
 * simultaneous measurements from both to the same type instance. */

#define NTPD_DEFAULT_HOST "localhost"
#define NTPD_DEFAULT_PORT "123"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * The following definitions were copied from the NTPd distribution  *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#define MAXFILENAME 128
#define MAXSEQ 127
#define MODE_PRIVATE 7
#define NTP_OLDVERSION ((uint8_t)1) /* oldest credible version */
#define IMPL_XNTPD 3
#define FP_FRAC 65536.0

#define REFCLOCK_ADDR 0x7f7f0000 /* 127.127.0.0 */
#define REFCLOCK_MASK 0xffff0000 /* 255.255.0.0 */

/* This structure is missing the message authentication code, since ncollectd doesn't use it. */
struct req_pkt {
    uint8_t rm_vn_mode;
    uint8_t auth_seq;
    uint8_t implementation;      /* implementation number */
    uint8_t request;             /* request number */
    uint16_t err_nitems;         /* error code/number of data items */
    uint16_t mbz_itemsize;       /* item size */
    char data[MAXFILENAME + 48]; /* data area [32 prev](176 byte max) */
                                 /* struct conf_peer must fit */
};
#define REQ_LEN_NOMAC (sizeof(struct req_pkt))

/*
 * A response packet.  The length here is variable, this is a
 * maximally sized one.  Note that this implementation doesn't
 * authenticate responses.
 */
#define RESP_HEADER_SIZE (8)
#define RESP_DATA_SIZE (500)

struct resp_pkt {
    uint8_t rm_vn_mode;        /* response, more, version, mode */
    uint8_t auth_seq;          /* key, sequence number */
    uint8_t implementation;    /* implementation number */
    uint8_t request;           /* request number */
    uint16_t err_nitems;       /* error code/number of data items */
    uint16_t mbz_itemsize;     /* item size */
    char data[RESP_DATA_SIZE]; /* data area */
};

/*
 * Bit setting macros for multifield items.
 */
#define RESP_BIT 0x80
#define MORE_BIT 0x40

#define ISRESPONSE(rm_vn_mode) (((rm_vn_mode)&RESP_BIT) != 0)
#define ISMORE(rm_vn_mode) (((rm_vn_mode)&MORE_BIT) != 0)
#define INFO_VERSION(rm_vn_mode) ((uint8_t)(((rm_vn_mode) >> 3) & 0x7))
#define INFO_MODE(rm_vn_mode) ((rm_vn_mode)&0x7)

#define RM_VN_MODE(resp, more, version)                                      \
    ((uint8_t)(((resp) ? RESP_BIT : 0) | ((more) ? MORE_BIT : 0) |           \
                         ((version ? version : (NTP_OLDVERSION + 1)) << 3) | \
                         (MODE_PRIVATE)))

#define INFO_IS_AUTH(auth_seq) (((auth_seq)&0x80) != 0)
#define INFO_SEQ(auth_seq) ((auth_seq)&0x7f)
#define AUTH_SEQ(auth, seq) ((uint8_t)((((auth) != 0) ? 0x80 : 0) | ((seq)&0x7f)))

#define INFO_ERR(err_nitems) ((uint16_t)((ntohs(err_nitems) >> 12) & 0xf))
#define INFO_NITEMS(err_nitems) ((uint16_t)(ntohs(err_nitems) & 0xfff))
#define ERR_NITEMS(err, nitems) (htons((uint16_t)((((uint16_t)(err) << 12) & 0xf000) | \
                                ((uint16_t)(nitems)&0xfff))))

#define INFO_MBZ(mbz_itemsize) ((ntohs(mbz_itemsize) >> 12) & 0xf)
#define INFO_ITEMSIZE(mbz_itemsize) ((uint16_t)(ntohs(mbz_itemsize) & 0xfff))
#define MBZ_ITEMSIZE(itemsize) (htons((uint16_t)(itemsize)))

/* negate a long float type */
#define M_NEG(v_i, v_f)                                        \
    do {                                                       \
        if ((v_f) == 0)                                        \
            (v_i) = -((uint32_t)(v_i));                        \
        else {                                                 \
            (v_f) = -((uint32_t)(v_f));                        \
            (v_i) = ~(v_i);                                    \
        }                                                      \
    } while (0)
/* l_fp to double */
#define M_LFPTOD(r_i, r_uf, d)                                 \
    do {                                                       \
        register int32_t ri;                                   \
        register uint32_t rf;                                  \
                                                               \
        ri = (r_i);                                            \
        rf = (r_uf);                                           \
        if (ri < 0) {                                          \
            M_NEG(ri, rf);                                     \
            (d) = -((double)ri + ((double)rf) / 4294967296.0); \
        } else {                                               \
            (d) = (double)ri + ((double)rf) / 4294967296.0;    \
        }                                                      \
    } while (0)

#define REQ_PEER_LIST_SUM 1
struct info_peer_summary {
    uint32_t dstadr;         /* local address (zero for undetermined) */
    uint32_t srcadr;         /* source address */
    uint16_t srcport;        /* source port */
    uint8_t stratum;         /* stratum of peer */
    int8_t hpoll;            /* host polling interval */
    int8_t ppoll;            /* peer polling interval */
    uint8_t reach;           /* reachability register */
    uint8_t flags;           /* flags, from above */
    uint8_t hmode;           /* peer mode */
    int32_t delay;           /* peer.estdelay; s_fp */
    int32_t offset_int;      /* peer.estoffset; integral part */
    int32_t offset_frc;      /* peer.estoffset; fractional part */
    uint32_t dispersion;     /* peer.estdisp; u_fp */
    uint32_t v6_flag;        /* is this v6 or not */
    uint32_t unused1;        /* (unused) padding for dstadr6 */
    struct in6_addr dstadr6; /* local address (v6) */
    struct in6_addr srcadr6; /* source address (v6) */
};

#define REQ_SYS_INFO 4
struct info_sys {
    uint32_t peer;           /* system peer address (v4) */
    uint8_t peer_mode;       /* mode we are syncing to peer in */
    uint8_t leap;            /* system leap bits */
    uint8_t stratum;         /* our stratum */
    int8_t precision;        /* local clock precision */
    int32_t rootdelay;       /* distance from sync source */
    uint32_t rootdispersion; /* dispersion from sync source */
    uint32_t refid;          /* reference ID of sync source */
    uint64_t reftime;        /* system reference time */
    uint32_t poll;           /* system poll interval */
    uint8_t flags;           /* system flags */
    uint8_t unused1;         /* unused */
    uint8_t unused2;         /* unused */
    uint8_t unused3;         /* unused */
    int32_t bdelay;          /* default broadcast offset */
    int32_t frequency;       /* frequency residual (scaled ppm)  */
    uint64_t authdelay;      /* default authentication delay */
    uint32_t stability;      /* clock stability (scaled ppm) */
    int32_t v6_flag;         /* is this v6 or not */
    int32_t unused4;         /* unused, padding for peer6 */
    struct in6_addr peer6;   /* system peer address (v6) */
};

#define REQ_GET_KERNEL 38
struct info_kernel {
    int32_t offset;
    int32_t freq;
    int32_t maxerror;
    int32_t esterror;
    uint16_t status;
    uint16_t shift;
    int32_t constant;
    int32_t precision;
    int32_t tolerance;
    /* pps stuff */
    int32_t ppsfreq;
    int32_t jitter;
    int32_t stabil;
    int32_t jitcnt;
    int32_t calcnt;
    int32_t errcnt;
    int32_t stbcnt;
};

/* List of reference clock names */
static const char *refclock_names[] = {
        "UNKNOWN",    "LOCAL",        "GPS_TRAK",   "WWV_PST",     /*  0- 3 */
        "SPECTRACOM", "TRUETIME",     "IRIG_AUDIO", "CHU_AUDIO",   /*  4- 7 */
        "GENERIC",    "GPS_MX4200",   "GPS_AS2201", "GPS_ARBITER", /*  8-11 */
        "IRIG_TPRO",  "ATOM_LEITCH",  "MSF_EES",    "GPSTM_TRUE",  /* 12-15 */
        "GPS_BANC",   "GPS_DATUM",    "ACTS_NIST",  "WWV_HEATH",   /* 16-19 */
        "GPS_NMEA",   "GPS_VME",      "PPS",        "ACTS_PTB",    /* 20-23 */
        "ACTS_USNO",  "TRUETIME",     "GPS_HP",     "MSF_ARCRON",  /* 24-27 */
        "SHM",        "GPS_PALISADE", "GPS_ONCORE", "GPS_JUPITER", /* 28-31 */
        "CHRONOLOG",  "DUMBCLOCK",    "ULINK_M320", "PCF",         /* 32-35 */
        "WWV_AUDIO",  "GPS_FG",       "HOPF_S",     "HOPF_P",      /* 36-39 */
        "JJY",        "TT_IRIG",      "GPS_ZYFER",  "GPS_RIPENCC", /* 40-43 */
        "NEOCLK4X",   "PCI_TSYNC",    "GPSD_JSON"                  /* 44-46 */
};
static size_t refclock_names_num = STATIC_ARRAY_SIZE(refclock_names);
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * End of the copied stuff..                                                                                 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

enum {
    FAM_NTPD_KERNEL_FREQUENCY_OFFSET,
    FAM_NTPD_KERNEL_OFFSET_LOOP_SECONDS,
    FAM_NTPD_KERNEL_OFFSET_ERROR_SECONDS,
    FAM_NTPD_PEER_STRATUM,
    FAM_NTPD_PEER_DISPERSION_SECONDS,
    FAM_NTPD_PEER_OFFSET_SECONDS,
    FAM_NTPD_PEER_DELAY_SECONDS,
    FAM_NTPD_MAX,
};

static metric_family_t fams[FAM_NTPD_MAX] = {
    [FAM_NTPD_KERNEL_FREQUENCY_OFFSET] = {
        .name = "ntpd_kernel_frequency_offset",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_NTPD_KERNEL_OFFSET_LOOP_SECONDS] = {
        .name = "ntpd_kernel_offset_loop_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_NTPD_KERNEL_OFFSET_ERROR_SECONDS] = {
        .name = "ntpd_kernel_offset_error_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_NTPD_PEER_STRATUM] = {
        .name = "ntpd_peer_stratum",
        .type = METRIC_TYPE_GAUGE,
        .help = "NTPD stratum",
    },
    [FAM_NTPD_PEER_DISPERSION_SECONDS] = {
        .name = "ntpd_peer_dispersion_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "NTPD dispersion",
    },
    [FAM_NTPD_PEER_OFFSET_SECONDS] = {
        .name = "ntpd_peer_offset_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "ClockOffset between NTP and local clock",
    },
    [FAM_NTPD_PEER_DELAY_SECONDS] = {
        .name = "ntpd_peer_delay_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "NTPD delay",
    },
};

typedef struct {
    char *name;
    char *host;
    char *port;
    bool do_reverse_lookups;
    bool include_unit_id;
    label_set_t labels;
    plugin_filter_t *filter;
    metric_family_t fams[FAM_NTPD_MAX];
    int sd;
} ntpd_ctx_t;

static int ntpd_connect(ntpd_ctx_t *ctx)
{
    if (ctx->sd >= 0)
        return 0;

    PLUGIN_DEBUG("Opening a new socket");

    struct addrinfo ai_hints = {.ai_family = AF_UNSPEC,
                                .ai_flags = AI_ADDRCONFIG,
                                .ai_protocol = IPPROTO_UDP,
                                .ai_socktype = SOCK_DGRAM};
    struct addrinfo *ai_list;
    int status = getaddrinfo(ctx->host, ctx->port, &ai_hints, &ai_list);
    if (status != 0) {
        PLUGIN_ERROR("getaddrinfo (%s, %s): %s", ctx->host,ctx-> port,
                     (status == EAI_SYSTEM) ? STRERRNO : gai_strerror(status));
        return -1;
    }

    for (struct addrinfo *ai_ptr = ai_list; ai_ptr != NULL; ai_ptr = ai_ptr->ai_next) {
        /* create our socket descriptor */
        ctx->sd = socket(ai_ptr->ai_family, ai_ptr->ai_socktype, ai_ptr->ai_protocol);
        if (ctx->sd < 0)
            continue;

        /* connect to the ntpd */
        if (connect(ctx->sd, ai_ptr->ai_addr, ai_ptr->ai_addrlen) < 0) {
            close(ctx->sd);
            ctx->sd = -1;
            continue;
        }

        break;
    }

    freeaddrinfo(ai_list);

    if (ctx->sd < 0) {
        PLUGIN_ERROR("Unable to connect to server.");
        return -1;
    }

    return 0;
}

/* For a description of the arguments see 'ntpd_do_query' below. */
static int ntpd_receive_response(ntpd_ctx_t *ctx, int *res_items, int *res_size,
                                                  char **res_data, int res_item_size)
{
    char pkt_recvd[MAXSEQ + 1] = {0}; /* sequence numbers that have been received */
    int pkt_recvd_num = 0;            /* number of packets that have been received */
    int pkt_lastseq = -1;             /* the last sequence number */
    ssize_t pkt_padding;              /* Padding in this packet */

    int status = ntpd_connect(ctx);
    if (status != 0)
        return -1;

    char *items = NULL;
    size_t items_num = 0;

    *res_items = 0;
    *res_size = 0;
    *res_data = NULL;

    cdtime_t end = cdtime() + TIME_T_TO_CDTIME_T(1); /* wait for a most one second */

    int done = 0;
    while (done == 0) {
        cdtime_t now = cdtime();
        /* timeout reached */
        if (now > end)
            break;

        struct pollfd poll_s;
        poll_s.fd = ctx->sd;
        poll_s.events = POLLIN | POLLPRI;
        poll_s.revents = 0;

        status = poll(&poll_s, 1, CDTIME_T_TO_MS(end-now));

        if ((status < 0) && ((errno == EAGAIN) || (errno == EINTR)))
            continue;

        if (status < 0) {
            PLUGIN_ERROR("poll failed: %s", STRERRNO);
            return -1;
        }

        if (status == 0) { /* timeout */
            PLUGIN_DEBUG("timeout reached.");
            break;
        }

        struct resp_pkt res;
        memset(&res, '\0', sizeof(res));
        status = recv(ctx->sd, (void *)&res, sizeof(res), 0 /* no flags */);

        if ((status < 0) && ((errno == EAGAIN) || (errno == EINTR)))
            continue;

        if (status < 0) {
            PLUGIN_INFO("recv(2) failed: %s", STRERRNO);
            PLUGIN_DEBUG("Closing socket #%i", ctx->sd);
            close(ctx->sd);
            ctx->sd = -1;
            return -1;
        }

        PLUGIN_DEBUG("recv'd %i bytes", status);

        /*
         * Do some sanity checks first
         */
        if (status < RESP_HEADER_SIZE) {
            PLUGIN_WARNING("Short (%i bytes) packet received", (int)status);
            continue;
        }
        if (INFO_MODE(res.rm_vn_mode) != MODE_PRIVATE) {
            PLUGIN_NOTICE("Packet received with mode %i", INFO_MODE(res.rm_vn_mode));
            continue;
        }
        if (INFO_IS_AUTH(res.auth_seq)) {
            PLUGIN_NOTICE("Encrypted packet received");
            continue;
        }
        if (!ISRESPONSE(res.rm_vn_mode)) {
            PLUGIN_NOTICE("Received request packet, wanted response");
            continue;
        }
        if (INFO_MBZ(res.mbz_itemsize)) {
            PLUGIN_WARNING("Received packet with nonzero MBZ field!");
            continue;
        }
        if (res.implementation != IMPL_XNTPD) {
            PLUGIN_WARNING("Asked for request of type %i, got %i",
                           (int)IMPL_XNTPD, (int)res.implementation);
            continue;
        }

        /* Check for error code */
        if (INFO_ERR(res.err_nitems) != 0) {
            PLUGIN_ERROR("Received error code %i", (int)INFO_ERR(res.err_nitems));
            return (int)INFO_ERR(res.err_nitems);
        }

        /* extract number of items in this packet and the size of these items */
        int pkt_item_num = INFO_NITEMS(res.err_nitems);
        int pkt_item_len = INFO_ITEMSIZE(res.mbz_itemsize);
        PLUGIN_DEBUG("pkt_item_num = %i; pkt_item_len = %i;", pkt_item_num, pkt_item_len);

        /* Check if the reported items fit in the packet */
        if ((pkt_item_num * pkt_item_len) > (status - RESP_HEADER_SIZE)) {
            PLUGIN_ERROR("%i items * %i bytes > %i bytes - %i bytes header",
                         (int)pkt_item_num, (int)pkt_item_len, (int)status, (int)RESP_HEADER_SIZE);
            continue;
        }

        if (pkt_item_len > res_item_size) {
            PLUGIN_ERROR("(pkt_item_len = %i) >= (res_item_size = %i)",
                         pkt_item_len, res_item_size);
            continue;
        }

        /* If this is the first packet (time wise, not sequence wise),
         * set 'res_size'. If it's not the first packet check if the
         * items have the same size. Discard invalid packets. */
        if (items_num == 0) { /* first packet */
            PLUGIN_DEBUG("*res_size = %i", pkt_item_len);
            *res_size = pkt_item_len;
        } else if (*res_size != pkt_item_len) {
            PLUGIN_DEBUG("Error: *res_size = %i; pkt_item_len = %i;", *res_size, pkt_item_len);
            PLUGIN_ERROR("Item sizes differ.");
            continue;
        }

        /*
         * Because the items in the packet may be smaller than the
         * items requested, the following holds true:
         */
        assert((*res_size == pkt_item_len) && (pkt_item_len <= res_item_size));

        /* Calculate the padding. No idea why there might be any padding.. */
        pkt_padding = 0;
        if (pkt_item_len < res_item_size)
            pkt_padding = res_item_size - pkt_item_len;
        PLUGIN_DEBUG("res_item_size = %i; pkt_padding = %zi;", res_item_size, pkt_padding);

        /* Extract the sequence number */
        int pkt_sequence = INFO_SEQ(res.auth_seq);
        if ((pkt_sequence < 0) || (pkt_sequence > MAXSEQ)) {
            PLUGIN_ERROR("Received packet with sequence %i", pkt_sequence);
            continue;
        }

        /* Check if this sequence has been received before. If so, discard it. */
        if (pkt_recvd[pkt_sequence] != '\0') {
            PLUGIN_NOTICE("Sequence %i received twice", pkt_sequence);
            continue;
        }

        /* If 'pkt_lastseq != -1' another packet without 'more bit' has
         * been received. */
        if (!ISMORE(res.rm_vn_mode)) {
            if (pkt_lastseq != -1) {
                PLUGIN_ERROR("Two packets which both claim to be the last one in the "
                             "sequence have been received.");
                continue;
            }
            pkt_lastseq = pkt_sequence;
            PLUGIN_DEBUG("Last sequence = %i;", pkt_lastseq);
        }

        /*
         * Enough with the checks. Copy the data now.
         * We start by allocating some more memory.
         */
        PLUGIN_DEBUG("realloc (%p, %" PRIsz ")", (void *)*res_data,
                     (items_num + pkt_item_num) * res_item_size);
        items = realloc(*res_data, (items_num + pkt_item_num) * res_item_size);
        if (items == NULL) {
            PLUGIN_ERROR("realloc failed.");
            continue;
        }
        items_num += pkt_item_num;
        *res_data = items;

        for (int i = 0; i < pkt_item_num; i++) {
            /* dst: There are already '*res_items' items with
             *          res_item_size bytes each in in '*res_data'. Set
             *          dst to the first byte after that. */
            void *dst = (void *)(*res_data + ((*res_items) * res_item_size));
            /* src: We use 'pkt_item_len' to calculate the offset
             *          from the beginning of the packet, because the
             *          items in the packet may be smaller than the
             *          items that were requested. We skip 'i' such
             *          items. */
            void *src = (void *)(((char *)res.data) + (i * pkt_item_len));

            /* Set the padding to zeros */
            if (pkt_padding != 0)
                memset(dst, '\0', res_item_size);
            memcpy(dst, src, (size_t)pkt_item_len);

            /* Increment '*res_items' by one, so 'dst' will end up
             * one further in the next round. */
            (*res_items)++;
        }

        pkt_recvd[pkt_sequence] = (char)1;
        pkt_recvd_num++;

        if ((pkt_recvd_num - 1) == pkt_lastseq)
            done = 1;
    }

    return 0;
}

/* For a description of the arguments see 'ntpd_do_query' below. */
static int ntpd_send_request(ntpd_ctx_t *ctx, int req_code, int req_items,
                                              int req_size, char *req_data)
{
    struct req_pkt req = {0};
    size_t req_data_len;

    assert(req_items >= 0);
    assert(req_size >= 0);

    int status  = ntpd_connect(ctx);
    if (status != 0)
        return -1;

    req.rm_vn_mode = RM_VN_MODE(0, 0, 0);
    req.auth_seq = AUTH_SEQ(0, 0);
    req.implementation = IMPL_XNTPD;
    req.request = (unsigned char)req_code;

    req_data_len = (size_t)(req_items * req_size);

    assert(((req_data != NULL) && (req_data_len > 0)) ||
            ((req_data == NULL) && (req_items == 0) && (req_size == 0)));

    req.err_nitems = ERR_NITEMS(0, req_items);
    req.mbz_itemsize = MBZ_ITEMSIZE(req_size);

    if (req_data != NULL)
        memcpy((void *)req.data, (const void *)req_data, req_data_len);

    PLUGIN_DEBUG("req_items = %i; req_size = %i; req_data = %p;",
                 req_items, req_size, (void *)req_data);

    status = swrite(ctx->sd, (const char *)&req, REQ_LEN_NOMAC);
    if (status != 0) {
        PLUGIN_DEBUG("'swrite' failed. Closing socket #%i", ctx->sd);
        close(ctx->sd);
        ctx->sd = -1;
        return status;
    }

    return 0;
}

/*
 * ntpd_do_query:
 *
 * req_code:            Type of request packet
 * req_items:           Numver of items in the request
 * req_size:            Size of one item in the request
 * req_data:            Data of the request packet
 * res_items:           Pointer to where the number returned items will be stored.
 * res_size:            Pointer to where the size of one returned item will be stored.
 * res_data:            This is where a pointer to the (allocated) data will be
 * stored.
 * res_item_size: Size of one returned item. (used to calculate padding)
 *
 * returns:             zero upon success, non-zero otherwise.
 */
static int ntpd_do_query(ntpd_ctx_t *ctx, int req_code, int req_items, int req_size,
                         char *req_data, int *res_items, int *res_size,
                         char **res_data, int res_item_size)
{
    int status = ntpd_send_request(ctx, req_code, req_items, req_size, req_data);
    if (status != 0)
        return status;

    status = ntpd_receive_response(ctx, res_items, res_size, res_data, res_item_size);
    return status;
}

static double ntpd_read_fp(int32_t val_int)
{
    val_int = ntohl(val_int);
    double val_double = ((double)val_int) / FP_FRAC;

    return val_double;
}

static uint32_t ntpd_get_refclock_id(struct info_peer_summary const *peer_info)
{
    uint32_t addr = ntohl(peer_info->srcadr);
    uint32_t refclock_id = (addr >> 8) & 0x00FF;

    return refclock_id;
}

static int ntpd_get_name_from_address(char *buffer, size_t buffer_size,
                                      struct info_peer_summary const *peer_info,
                                      bool do_reverse_lookup)
{
    struct sockaddr_storage sa = {0};
    socklen_t sa_len;

    if (peer_info->v6_flag) {
        struct sockaddr_in6 sa6 = {0};

        assert(sizeof(sa) >= sizeof(sa6));

        sa6.sin6_family = AF_INET6;
        sa6.sin6_port = htons(123);
        memcpy(&sa6.sin6_addr, &peer_info->srcadr6, sizeof(struct in6_addr));
        sa_len = sizeof(sa6);

        memcpy(&sa, &sa6, sizeof(sa6));
    } else {
        struct sockaddr_in sa4 = {0};

        assert(sizeof(sa) >= sizeof(sa4));

        sa4.sin_family = AF_INET;
        sa4.sin_port = htons(123);
        memcpy(&sa4.sin_addr, &peer_info->srcadr, sizeof(struct in_addr));
        sa_len = sizeof(sa4);

        memcpy(&sa, &sa4, sizeof(sa4));
    }

    int flags = 0;
    if (!do_reverse_lookup)
        flags |= NI_NUMERICHOST;

    int status = getnameinfo((struct sockaddr const *)&sa, sa_len, buffer,
                         buffer_size, NULL, 0, /* No port name */ flags);
    if (status != 0) {
        PLUGIN_ERROR("getnameinfo failed: %s",
                     (status == EAI_SYSTEM) ? STRERRNO : gai_strerror(status));
        return -1;
    }

    return 0;
}

static int ntpd_get_name_refclock(char *buffer, size_t buffer_size,
                                  struct info_peer_summary const *peer_info, bool include_unit_id)
{
    uint32_t refclock_id = ntpd_get_refclock_id(peer_info);
    uint32_t unit_id = ntohl(peer_info->srcadr) & 0x00FF;

    if (((size_t)refclock_id) >= refclock_names_num)
        return ntpd_get_name_from_address(buffer, buffer_size, peer_info, 0);

    if (include_unit_id) {
        /* coverity[OVERRUN] */
        snprintf(buffer, buffer_size, "%s-%" PRIu32, refclock_names[refclock_id], unit_id);
    } else {
        /* coverity[OVERRUN] */
        sstrncpy(buffer, refclock_names[refclock_id], buffer_size);
    }

    return 0;
}

static int ntp_read_kernel(ntpd_ctx_t *ctx)
{
    struct info_kernel *ik = NULL;
    int ik_num = 0;
    int ik_size = 0;

    int status = ntpd_do_query(ctx, REQ_GET_KERNEL, 0, 0, NULL, /* request data */
                               &ik_num, &ik_size,
                               (char **)((void *)&ik), /* response data */
                               sizeof(struct info_kernel));
    if (status != 0) {
        PLUGIN_ERROR("ntpd_do_query (REQ_GET_KERNEL) failed with status %i", status);
        free(ik);
        return status;
    } else if ((ik == NULL) || (ik_num == 0) || (ik_size == 0)) {
        PLUGIN_ERROR("ntpd_do_query returned unexpected data. "
                     "(ik = %p; ik_num = %i; ik_size = %i)", (void *)ik, ik_num, ik_size);
        free(ik);
        return -1;
    }

    /* On Linux, if the STA_NANO bit is set in ik->status, then ik->offset
     * is is nanoseconds, otherwise it's microseconds. */
    double scale_loop = 1e-6;
    double scale_error = 1e-6;

    if (ntohs(ik->status) & STA_NANO) {
        scale_loop = 1e-9;
        scale_error = 1e-9;
    }

    /* kerninfo -> estimated error */
    double offset_loop = (double)((int32_t)ntohl(ik->offset) * scale_loop);
    double freq_loop = ntpd_read_fp(ik->freq);
    double offset_error = (double)((int32_t)ntohl(ik->esterror) * scale_error);

    PLUGIN_DEBUG("info_kernel:\n"
                 "  pll offset        = %.8g\n"
                 "  pll frequency = %.8g\n" /* drift compensation */
                 "  est error         = %.8g\n",
                 offset_loop, freq_loop, offset_error);

    metric_family_append(&ctx->fams[FAM_NTPD_KERNEL_FREQUENCY_OFFSET],
                         VALUE_GAUGE(freq_loop), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_NTPD_KERNEL_OFFSET_LOOP_SECONDS],
                         VALUE_GAUGE(offset_loop), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_NTPD_KERNEL_OFFSET_ERROR_SECONDS],
                         VALUE_GAUGE(offset_error), &ctx->labels, NULL);

    free(ik);
    return 0;
}

static int ntp_read_peer_summary(ntpd_ctx_t *ctx)
{
    struct info_peer_summary *ps = NULL;
    int ps_num = 0;
    int ps_size = 0;

    int status = ntpd_do_query(ctx, REQ_PEER_LIST_SUM, 0, 0, NULL, /* request data */
                               &ps_num, &ps_size,
                               (char **)((void *)&ps), /* response data */
                               sizeof(struct info_peer_summary));
    if (status != 0) {
        PLUGIN_ERROR("ntpd_do_query (REQ_PEER_LIST_SUM) failed with status %i", status);
        free(ps);
        return status;
    } else if ((ps == NULL) || (ps_num == 0) || (ps_size == 0)) {
        PLUGIN_ERROR("ntpd_do_query returned unexpected data. "
                    "(ps = %p; ps_num = %i; ps_size = %i)", (void *)ps, ps_num, ps_size);
        free(ps);
        return -1;
    }

    for (int i = 0; i < ps_num; i++) {
        struct info_peer_summary *ptr = ps + i;

        int is_refclock = !ptr->v6_flag && ((ntohl(ptr->srcadr) & REFCLOCK_MASK) == REFCLOCK_ADDR);

        char peername[NI_MAXHOST];
        if (is_refclock)
            status = ntpd_get_name_refclock(peername, sizeof(peername), ptr,
                                            ctx->include_unit_id);
        else
            status = ntpd_get_name_from_address(peername, sizeof(peername), ptr,
                                                ctx->do_reverse_lookups);

        if (status != 0) {
            PLUGIN_ERROR("Determining name of peer failed.");
            continue;
        }

        // '0.0.0.0' hosts are caused by POOL servers
        // see https://github.com/collectd/collectd/issues/2358
        if (strcmp(peername, "0.0.0.0") == 0)
            continue;

        uint32_t refclock_id = ntpd_get_refclock_id(ptr);

        /* Convert the 'long floating point' offset value to double */
        double offset = 0.0;
        M_LFPTOD(ntohl(ptr->offset_int), ntohl(ptr->offset_frc), offset);

        PLUGIN_DEBUG("peer %d:\n"
                     "  is_refclock= %d\n"
                     "  refclock_id= %u\n"
                     "  peername   = %s\n"
                     "  srcadr     = 0x%08x\n"
                     "  reach      = 0%03o\n"
                     "  delay      = %f\n"
                     "  offset_int = %u\n"
                     "  offset_frc = %u\n"
                     "  offset     = %f\n"
                     "  dispersion = %f\n",
                     i, is_refclock, (is_refclock > 0) ? refclock_id : 0, peername,
                     ntohl(ptr->srcadr), ptr->reach, ntpd_read_fp(ptr->delay),
                     ntohl(ptr->offset_int), ntohl(ptr->offset_frc), offset,
                     ntpd_read_fp(ptr->dispersion));

        metric_family_append(&ctx->fams[FAM_NTPD_PEER_STRATUM],
                            VALUE_GAUGE(ptr->stratum), &ctx->labels,
                             &(label_pair_const_t){.name="peer", .value=peername}, NULL);

        metric_family_append(&ctx->fams[FAM_NTPD_PEER_DISPERSION_SECONDS],
                             VALUE_GAUGE(ptr->reach & 1 ? ntpd_read_fp(ptr->dispersion) : NAN),
                             &ctx->labels,
                             &(label_pair_const_t){.name="peer", .value=peername}, NULL);

        /* not the system clock (offset will always be zero) */
        if (!(is_refclock && refclock_id == 1)) {
            metric_family_append(&ctx->fams[FAM_NTPD_PEER_OFFSET_SECONDS],
                                 VALUE_GAUGE(ptr->reach & 1 ? offset : NAN), &ctx->labels,
                                 &(label_pair_const_t){.name="peer", .value=peername}, NULL);
        }
        /* not a reference clock */
        if (!is_refclock) {
            metric_family_append(&ctx->fams[FAM_NTPD_PEER_DELAY_SECONDS],
                                 VALUE_GAUGE(ptr->reach & 1 ? ntpd_read_fp(ptr->delay) : NAN),
                                 &ctx->labels,
                                 &(label_pair_const_t){.name="peer", .value=peername}, NULL);
        }
    }

    free(ps);
    return 0;
}

static int ntpd_read(user_data_t *user_data)
{
    ntpd_ctx_t *ctx = user_data->data;

    int status = ntp_read_kernel(ctx);
    if (status == 0)
        ntp_read_peer_summary(ctx);

    plugin_dispatch_metric_family_array_filtered(ctx->fams, FAM_NTPD_MAX, ctx->filter, 0);
    return 0;
}

static void ntpd_free(void *arg)
{
    ntpd_ctx_t *ctx = arg;

    if (ctx == NULL)
        return;

    free(ctx->name);
    free(ctx->host);
    free(ctx->port);

    label_set_reset(&ctx->labels);
    plugin_filter_free(ctx->filter);

    if (ctx->sd >= 0)
        close(ctx->sd);

    free(ctx);
}

static int ntpd_config_instance(config_item_t *ci)
{
    ntpd_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    ctx->sd = -1;
    ctx->do_reverse_lookups = true;

    memcpy(ctx->fams, fams, sizeof(ctx->fams[0])*FAM_NTPD_MAX);

    int status = cf_util_get_string(ci, &ctx->name);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        free(ctx);
        return status;
    }

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("host", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->host);
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->port);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &ctx->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("reverse-lookups", child->key) == 0) {
            status = cf_util_get_boolean(child, &ctx->do_reverse_lookups);
        } else if (strcasecmp("include-unit-id", child->key) == 0) {
            status = cf_util_get_boolean(child, &ctx->include_unit_id);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &ctx->filter);
        } else {
            PLUGIN_ERROR("Option `%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        ntpd_free(ctx);
        return -1;
    }

    if (ctx->host == NULL) {
        ctx->host = strdup(NTPD_DEFAULT_HOST);
        if (ctx->host == NULL) {
            ntpd_free(ctx);
            return -1;
        }
    }

    if (ctx->port == NULL) {
        ctx->port = strdup(NTPD_DEFAULT_PORT);
        if (ctx->port == NULL) {
            ntpd_free(ctx);
            return -1;
        }
    }

    label_set_add(&ctx->labels, true, "instance", ctx->name);

    return plugin_register_complex_read("ntpd", ctx->name, ntpd_read, interval,
                                        &(user_data_t){.data = ctx, .free_func = ntpd_free});
}

static int ntpd_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = ntpd_config_instance(child);
        } else {
            PLUGIN_ERROR("The configuration option '%s' is not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("ntpd", ntpd_config);
}
