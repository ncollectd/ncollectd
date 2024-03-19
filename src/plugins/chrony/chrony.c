// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: Copyright (C) Claudius M Zingerli, ZSeng, 2015-2016
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

/*
 * TODO:
 *  - More robust udp parsing (using offsets instead of structs?)
 *      -> Currently chrony parses its data the same way as we do (using
 *structs)
 *  - Plausibility checks on values received
 *      -> Done at higher levels
 */

#include "plugin.h"
#include "libutils/common.h"

#ifdef HAVE_NETDB_H
#include <netdb.h> /* struct addrinfo */
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h> /* ntohs/ntohl */
#endif

/* AIX doesn't have MSG_DONTWAIT */
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT MSG_NONBLOCK
#endif

/* Used to initialize seq nr generator */
#define URAND_DEVICE_PATH "/dev/urandom"
/* Used to initialize seq nr generator (fall back) */
#define RAND_DEVICE_PATH "/dev/random"

#define CHRONY_DEFAULT_HOST "localhost"
#define CHRONY_DEFAULT_PORT "323"
#define CHRONY_DEFAULT_TIMEOUT 2

/* Chronyd command packet variables adapted from chrony/candm.h (GPL2) */
#define PROTO_VERSION_NUMBER 6
#define IPADDR_UNSPEC 0
#define IPADDR_INET4 1
#define IPADDR_INET6 2
#define IPV6_STR_MAX_SIZE (8 * 4 + 7 + 1)
#define MODE_REFCLOCK 2

typedef enum {
    PKT_TYPE_CMD_REQUEST = 1,
    PKT_TYPE_CMD_REPLY   = 2
} packet_type_e;

typedef enum {
    REQ_N_SOURCES    = 14,
    REQ_SOURCE_DATA  = 15,
    REQ_TRACKING     = 33,
    REQ_SOURCE_STATS = 34
} daemon_requests_t;

typedef enum {
    RPY_NULL             = 1,
    RPY_N_SOURCES        = 2,
    RPY_SOURCE_DATA      = 3,
    RPY_MANUAL_TIMESTAMP = 4,
    RPY_TRACKING         = 5,
    RPY_SOURCE_STATS     = 6,
    RPY_RTC              = 7
} daemon_replies_t;

typedef struct __attribute__((packed)) {
    int32_t value;
} value_float_t;

typedef struct __attribute__((packed)) {
    uint32_t tv_sec_high;
    uint32_t tv_sec_low;
    uint32_t tv_nsec;
} timeval_t;

typedef enum {
    STT_SUCCESS            = 0,
    STT_FAILED             = 1,
    STT_UNAUTH             = 2,
    STT_INVALID            = 3,
    STT_NOSUCHSOURCE       = 4,
    STT_INVALIDTS          = 5,
    STT_NOTENABLED         = 6,
    STT_BADSUBNET          = 7,
    STT_ACCESSALLOWED      = 8,
    STT_ACCESSDENIED       = 9,
    STT_NOHOSTACCESS       = 10,
    STT_SOURCEALREADYKNOWN = 11,
    STT_TOOMANYSOURCES     = 12,
    STT_NORTC              = 13,
    STT_BADRTCFILE         = 14,
    STT_INACTIVE           = 15,
    STT_BADSAMPLE          = 16,
    STT_INVALIDAF          = 17,
    STT_BADPKTVERSION      = 18,
    STT_BADPKTLENGTH       = 19
} eChrony_Status;

/* Chrony client request packets */
typedef struct __attribute__((packed)) {
    uint8_t f_dummy0[80]; /* Chrony expects 80bytes dummy data (Avoiding UDP Amplification) */
} chrony_req_tracking_t;

typedef struct __attribute__((packed)) {
    uint32_t f_n_sources;
} chrony_req_n_sources_t;

typedef struct __attribute__((packed)) {
    int32_t f_index;
    uint8_t f_dummy0[44];
} chrony_req_source_data_t;

typedef struct __attribute__((packed)) {
    int32_t f_index;
    uint8_t f_dummy0[56];
} chrony_req_source_stats;

typedef struct __attribute__((packed)) {
    struct {
        uint8_t f_version;
        uint8_t f_type;
        uint8_t f_dummy0;
        uint8_t f_dummy1;
        uint16_t f_cmd;
        uint16_t f_cmd_try;
        uint32_t f_seq;

        uint32_t f_dummy2;
        uint32_t f_dummy3;
    } header; /* Packed: 20Bytes */
    union {
        chrony_req_n_sources_t n_sources;
        chrony_req_source_data_t source_data;
        chrony_req_source_stats source_stats;
        chrony_req_tracking_t tracking;
    } body;
    uint8_t padding[4 + 16]; /* Padding to match minimal response size */
} chrony_request_t;

/* Chrony daemon response packets */
typedef struct __attribute__((packed)) {
    uint32_t f_n_sources;
} chrony_resp_n_sources_t;

typedef struct __attribute__((packed)) {
    union {
        uint32_t ip4;
        uint8_t ip6[16];
    } addr;
    uint16_t f_family;
    uint16_t padding;
} chrony_ipaddr_t;

typedef struct __attribute__((packed)) {
    chrony_ipaddr_t addr;
    int16_t f_poll;     /* 2^f_poll = Time between polls (s) */
    uint16_t f_stratum; /* Remote clock stratum */
    uint16_t f_state;   /* 0 = RPY_SD_ST_SYNC, 1 = RPY_SD_ST_UNREACH, 2 = RPY_SD_ST_FALSETICKER */
                        /* 3 = RPY_SD_ST_JITTERY, 4 = RPY_SD_ST_CANDIDATE, 5 = RPY_SD_ST_OUTLIER */
    uint16_t f_mode;    /* 0 = RPY_SD_MD_CLIENT, 1 = RPY_SD_MD_PEER, 2 = RPY_SD_MD_REF */
    uint16_t f_flags;   /* unused */
    uint16_t f_reachability; /* Bit mask of successfull tries to reach the source */

    uint32_t f_since_sample;     /* Time since last sample (s) */
    value_float_t f_origin_latest_meas; /*  */
    value_float_t f_latest_meas;        /*  */
    value_float_t f_latest_meas_err;    /*  */
} chrony_resp_source_data_t;

typedef struct __attribute__((packed)) {
    uint32_t f_ref_id;
    chrony_ipaddr_t addr;
    uint32_t f_n_samples;       /* Number of measurements done   */
    uint32_t f_n_runs;          /* How many measurements to come */
    uint32_t f_span_seconds;    /* For how long we're measuring  */
    value_float_t f_rtc_seconds_fast;  /* ??? */
    value_float_t f_rtc_gain_rate_ppm; /* Estimated relative frequency error */
    value_float_t f_skew_ppm;          /* Clock skew (ppm) (worst case freq est error (skew: peak2peak)) */
    value_float_t f_est_offset;        /* Estimated offset of source */
    value_float_t f_est_offset_err;    /* Error of estimation */
} chrony_resp_source_stats_t;

typedef struct __attribute__((packed)) {
    uint32_t f_ref_id;
    chrony_ipaddr_t addr;
    uint16_t f_stratum;
    uint16_t f_leap_status;
    timeval_t f_ref_time;
    value_float_t f_current_correction;
    value_float_t f_last_offset;
    value_float_t f_rms_offset;
    value_float_t f_freq_ppm;
    value_float_t f_resid_freq_ppm;
    value_float_t f_skew_ppm;
    value_float_t f_root_delay;
    value_float_t f_root_dispersion;
    value_float_t f_last_update_interval;
} chrony_resp_tracking_t;

typedef struct __attribute__((packed)) {
    struct {
        uint8_t f_version;
        uint8_t f_type;
        uint8_t f_dummy0;
        uint8_t f_dummy1;
        uint16_t f_cmd;
        uint16_t f_reply;
        uint16_t f_status;
        uint16_t f_dummy2;
        uint16_t f_dummy3;
        uint16_t f_dummy4;
        uint32_t f_seq;
        uint32_t f_dummy5;
        uint32_t f_dummy6;
    } header; /* Packed: 28 Bytes */

    union {
        chrony_resp_n_sources_t n_sources;
        chrony_resp_source_data_t source_data;
        chrony_resp_source_stats_t source_stats;
        chrony_resp_tracking_t tracking;
    } body;

    uint8_t padding[1024];
} chrony_response_t;

enum {
    FAM_CHRONY_CLOCK_MODE = 0,
    FAM_CHRONY_CLOCK_LAST_MEAS,
    FAM_CHRONY_CLOCK_LAST_UPDATE,
    FAM_CHRONY_CLOCK_REACHABILITY,
    FAM_CHRONY_CLOCK_SKEW,
    FAM_CHRONY_CLOCK_STATE,
    FAM_CHRONY_CLOCK_STRATUM,
    FAM_CHRONY_FREQUENCY_ERROR,
    FAM_CHRONY_ROOT_DELAY,
    FAM_CHRONY_ROOT_DISPERSION,
    FAM_CHRONY_TIME_OFFSET_NTP,
    FAM_CHRONY_TIME_OFFSET_RMS,
    FAM_CHRONY_TIME_OFFSET,
    FAM_CHRONY_TIME_REF,
    FAM_CHRONY_MAX,
};

static metric_family_t fams[FAM_CHRONY_MAX] = {
    [FAM_CHRONY_CLOCK_MODE] = {
        .name = "chrony_clock_mode",
        .type = METRIC_TYPE_GAUGE,
    },
    [FAM_CHRONY_CLOCK_LAST_MEAS] = {
        .name = "chrony_clock_last_measurement_seconds",
        .type = METRIC_TYPE_GAUGE,
    },
    [FAM_CHRONY_CLOCK_LAST_UPDATE] = {
        .name = "chrony_clock_last_update_seconds",
        .type = METRIC_TYPE_GAUGE,
    },
    [FAM_CHRONY_CLOCK_REACHABILITY] = {
        .name = "chrony_clock_reachability",
        .type = METRIC_TYPE_GAUGE,
    },
    [FAM_CHRONY_CLOCK_SKEW] = {
        .name = "chrony_clock_skew_ppm",
        .type = METRIC_TYPE_GAUGE,
    },
    [FAM_CHRONY_CLOCK_STATE] = {
        .name = "chrony_clock_state",
        .type = METRIC_TYPE_GAUGE,
    },
    [FAM_CHRONY_CLOCK_STRATUM] = {
        .name = "chrony_clock_stratum",
        .type = METRIC_TYPE_GAUGE,
    },
    [FAM_CHRONY_FREQUENCY_ERROR] = {
        .name = "chrony_frequency_error_ppm",
        .type = METRIC_TYPE_GAUGE,
    },
    [FAM_CHRONY_ROOT_DELAY] = {
        .name = "chrony_root_delay_seconds",
        .type = METRIC_TYPE_GAUGE,
    },
    [FAM_CHRONY_ROOT_DISPERSION] = {
        .name = "chrony_root_dispersion_seconds",
        .type = METRIC_TYPE_GAUGE,
    },
    [FAM_CHRONY_TIME_OFFSET_NTP] = {
        .name = "chrony_time_offset_ntp_seconds",
        .type = METRIC_TYPE_GAUGE,
    },
    [FAM_CHRONY_TIME_OFFSET_RMS] = {
        .name = "chrony_time_offset_rms_seconds",
        .type = METRIC_TYPE_GAUGE,
    },
    [FAM_CHRONY_TIME_OFFSET] = {
        .name = "chrony_time_offset_seconds",
        .type = METRIC_TYPE_GAUGE,
    },
    [FAM_CHRONY_TIME_REF] = {
        .name = "chrony_time_ref_seconds",
        .type = METRIC_TYPE_GAUGE,
    },
};

typedef struct {
    char *name;
    char *host;
    char *port;
    cdtime_t timeout;
    label_set_t labels;
    bool is_connected;
    bool seq_is_initialized;
    uint32_t rand;
    int socket;
    metric_family_t fams[FAM_CHRONY_MAX];
} chrony_ctx_t;

static int connect_client(const char *hostname, const char *service)
{
    struct addrinfo ai_hints = {.ai_family = AF_UNSPEC,
                                .ai_flags = AI_ADDRCONFIG,
                                .ai_protocol = IPPROTO_UDP,
                                .ai_socktype = SOCK_DGRAM};
    struct addrinfo *ai_list;
    int status = getaddrinfo(hostname, service, &ai_hints, &ai_list);
    if (status != 0) {
        PLUGIN_ERROR("getaddrinfo (%s, %s): %s", hostname, service,
                     (status == EAI_SYSTEM) ? STRERRNO : gai_strerror(status));
        return -1;
    }

    int fd = -1;
    for (struct addrinfo *ai_ptr = ai_list; ai_ptr != NULL; ai_ptr = ai_ptr->ai_next) {
        /* create our socket descriptor */
        fd = socket(ai_ptr->ai_family, ai_ptr->ai_socktype, ai_ptr->ai_protocol);
        if (fd < 0)
            continue;

        /* connect to the ntpd */
        if (connect(fd, ai_ptr->ai_addr, ai_ptr->ai_addrlen)) {
            close(fd);
            fd = -1;
            continue;
        }

        break;
    }

    freeaddrinfo(ai_list);

    if (fd < 0) {
        PLUGIN_ERROR("Unable to connect to server.");
        return -1;
    }

    return fd;
}

/* niptoha code originally from:
 * git://git.tuxfamily.org/gitroot/chrony/chrony.git:util.c */
/* Original code licensed as GPLv2, by Richard P. Purnow, Miroslav Lichvar */
/* Original name: char * UTI_IPToString(IPAddr *addr)*/
static char *niptoha(const chrony_ipaddr_t *addr, char *buf, size_t buf_size)
{
    int status = 1;

    switch (ntohs(addr->f_family)) {
    case IPADDR_UNSPEC:
        status = ssnprintf(buf, buf_size, "[UNSPEC]");
        break;
    case IPADDR_INET4: {
        unsigned long ip = ntohl(addr->addr.ip4);
        unsigned long a = (ip >> 24) & 0xff;
        unsigned long b = (ip >> 16) & 0xff;
        unsigned long c = (ip >> 8) & 0xff;
        unsigned long d = (ip >> 0) & 0xff;
        status = ssnprintf(buf, buf_size, "%lu.%lu.%lu.%lu", a, b, c, d);
    }   break;
    case IPADDR_INET6: {
        const char *rp = inet_ntop(AF_INET6, addr->addr.ip6, buf, buf_size);
        if (rp == NULL) {
            PLUGIN_ERROR("Error converting ipv6 address to string. Errno = %d", errno);
            status = ssnprintf(buf, buf_size, "[UNKNOWN]");
        }
        break;
    }
    default:
        status = ssnprintf(buf, buf_size, "[UNKNOWN]");
    }

    assert(status > 0);

    return buf;
}

static void nreftostr(uint32_t nrefid, char *buf, size_t buf_size)
{
    size_t j = 0;
    for (int i = 0; i < 4; i++) {
        int c = ntohl(nrefid) << i * 8 >> 24;
        if (!isalnum(c) || j + 1 >= buf_size)
            continue;
        buf[j++] = c;
    }
    if (j < buf_size)
        buf[j] = '\0';
}

static int chrony_connect(chrony_ctx_t *ctx)
{
    PLUGIN_DEBUG("Connecting to %s:%s", ctx->host, ctx->port);
    int socket = connect_client(ctx->host, ctx->port);
    if (socket < 0) {
        PLUGIN_ERROR("Error connecting to daemon. Errno = %d", errno);
        return -1;
    }
    PLUGIN_DEBUG("Connected");
    ctx->socket = socket;

    struct timeval tv = CDTIME_T_TO_TIMEVAL(ctx->timeout);
    int status = setsockopt(ctx->socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
    if (status < 0)
        PLUGIN_WARNING("Error setting timeout to %g: %s",
                       CDTIME_T_TO_DOUBLE(ctx->timeout), STRERRNO);

    return 0;
}

static int chrony_send_request(chrony_ctx_t *ctx, const chrony_request_t *req, size_t req_size)
{
    if (send(ctx->socket, req, req_size, 0) < 0) {
        PLUGIN_ERROR("Error sending packet: %s.", STRERRNO);
        return -1;
    }
    return 0;
}

static int chrony_recv_response(chrony_ctx_t *ctx, chrony_response_t *resp, size_t resp_max_size,
                                size_t *resp_size)
{
    ssize_t rc = recv(ctx->socket, resp, resp_max_size, 0);
    if (rc <= 0) {
        PLUGIN_ERROR("Error receiving packet: %s.", STRERRNO);
        return -1;
    }
    *resp_size = rc;
    return 0;
}

static void chrony_flush_recv_queue(chrony_ctx_t *ctx)
{
    char buf[1];

    if (ctx->is_connected) {
        while (recv(ctx->socket, buf, sizeof(buf), MSG_DONTWAIT) > 0)
            ;
    }
}

static int chrony_query(chrony_ctx_t *ctx, const int command, chrony_request_t *p_req,
                        chrony_response_t *p_resp, size_t *p_resp_size)
{
    /* Check connection. We simply perform one try as ncollectd already handles retries */
    assert(p_req);
    assert(p_resp);
    assert(p_resp_size);

    if (ctx->is_connected == 0) {
        if (chrony_connect(ctx) == 0) {
            ctx->is_connected = 1;
        } else {
            PLUGIN_ERROR("Unable to connect. Errno = %d", errno);
            return -1;
        }
    }

    do {
        int valid_command = 0;
        size_t req_size = sizeof(p_req->header) + sizeof(p_req->padding);
        size_t resp_size = sizeof(p_resp->header);
        uint16_t resp_code = RPY_NULL;
        switch (command) {
        case REQ_TRACKING:
            req_size += sizeof(p_req->body.tracking);
            resp_size += sizeof(p_resp->body.tracking);
            resp_code = RPY_TRACKING;
            valid_command = 1;
            break;
        case REQ_N_SOURCES:
            req_size += sizeof(p_req->body.n_sources);
            resp_size += sizeof(p_resp->body.n_sources);
            resp_code = RPY_N_SOURCES;
            valid_command = 1;
            break;
        case REQ_SOURCE_DATA:
            req_size += sizeof(p_req->body.source_data);
            resp_size += sizeof(p_resp->body.source_data);
            resp_code = RPY_SOURCE_DATA;
            valid_command = 1;
            break;
        case REQ_SOURCE_STATS:
            req_size += sizeof(p_req->body.source_stats);
            resp_size += sizeof(p_resp->body.source_stats);
            resp_code = RPY_SOURCE_STATS;
            valid_command = 1;
            break;
        default:
            PLUGIN_ERROR("Unknown request command (Was: %d)", command);
            break;
        }

        if (valid_command == 0)
            break;

        uint32_t seq_nr = rand_r(&ctx->rand);
        p_req->header.f_cmd = htons(command);
        p_req->header.f_cmd_try = 0;
        p_req->header.f_seq = seq_nr;

        PLUGIN_DEBUG("Sending request (.cmd = %d, .seq = %u)", command,
                    seq_nr);
        if (chrony_send_request(ctx, p_req, req_size) != 0)
            break;

        PLUGIN_DEBUG("Waiting for response");
        if (chrony_recv_response(ctx, p_resp, resp_size, p_resp_size) != 0)
            break;

        PLUGIN_DEBUG("Received response: .version = %u, .type = %u, .cmd = "
                     "%u, .reply = %u, .status = %u, .seq = %u",
                     p_resp->header.f_version, p_resp->header.f_type,
                     ntohs(p_resp->header.f_cmd), ntohs(p_resp->header.f_reply),
                     ntohs(p_resp->header.f_status), p_resp->header.f_seq);

        if (p_resp->header.f_version != p_req->header.f_version) {
            PLUGIN_ERROR("Wrong protocol version (Was: %d, expected: %d)",
                         p_resp->header.f_version, p_req->header.f_version);
            return -1;
        }
        if (p_resp->header.f_type != PKT_TYPE_CMD_REPLY) {
            PLUGIN_ERROR("Wrong packet type (Was: %d, expected: %d)",
                         p_resp->header.f_type, PKT_TYPE_CMD_REPLY);
            return -1;
        }
        if (p_resp->header.f_seq != seq_nr) {
            /* FIXME: Implement sequence number handling */
            PLUGIN_ERROR("Unexpected sequence number (Was: %"PRIu32", expected: %"PRIu32")",
                         p_resp->header.f_seq, p_req->header.f_seq);
            return -1;
        }
        if (p_resp->header.f_cmd != p_req->header.f_cmd) {
            PLUGIN_ERROR("Wrong reply command (Was: %d, expected: %d)",
                         p_resp->header.f_cmd, p_req->header.f_cmd);
            return -1;
        }

        if (ntohs(p_resp->header.f_reply) != resp_code) {
            PLUGIN_ERROR("Wrong reply code (Was: %d, expected: %d)",
                         ntohs(p_resp->header.f_reply), resp_code);
            return -1;
        }

        switch (p_resp->header.f_status) {
        case STT_SUCCESS:
            PLUGIN_DEBUG("Reply packet status STT_SUCCESS");
            break;
        default:
            PLUGIN_ERROR("Reply packet contains error status: %d (expected: %d)",
                         p_resp->header.f_status, STT_SUCCESS);
            return -1;
        }

        return 0;
    } while (0);

    return -1;
}

static void chrony_init_req(chrony_request_t *req)
{
    memset(req, 0, sizeof(*req));
    req->header.f_version = PROTO_VERSION_NUMBER;
    req->header.f_type = PKT_TYPE_CMD_REQUEST;
    req->header.f_dummy0 = 0;
    req->header.f_dummy1 = 0;
    req->header.f_dummy2 = 0;
    req->header.f_dummy3 = 0;
}

/* ntohf code originally from:
 * git://git.tuxfamily.org/gitroot/chrony/chrony.git:util.c */
/* Original code licensed as GPLv2, by Richard P. Purnow, Miroslav Lichvar */
/* Original name: double UTI_value_float_tNetworkToHost(value_float_t f) */
static double ntohf(value_float_t value_float)
{
    /* Convert value_float_t in Network-bit-order to double in host-bit-order */

#define FLOAT_EXP_BITS 7
#define FLOAT_EXP_MIN (-(1 << (FLOAT_EXP_BITS - 1)))
#define FLOAT_EXP_MAX (-FLOAT_EXP_MIN - 1)
#define FLOAT_COEF_BITS ((int)sizeof(int32_t) * 8 - FLOAT_EXP_BITS)
#define FLOAT_COEF_MIN (-(1 << (FLOAT_COEF_BITS - 1)))
#define FLOAT_COEF_MAX (-FLOAT_COEF_MIN - 1)

    uint32_t uval = ntohl(value_float.value);
    int32_t exp = (uval >> FLOAT_COEF_BITS);
    if (exp >= 1 << (FLOAT_EXP_BITS - 1))
        exp -= 1 << FLOAT_EXP_BITS;
    exp -= FLOAT_COEF_BITS;

    /* coef = (x << FLOAT_EXP_BITS) >> FLOAT_EXP_BITS; */
    int32_t coef = uval % (1U << FLOAT_COEF_BITS);
    if (coef >= 1 << (FLOAT_COEF_BITS - 1))
        coef -= 1 << FLOAT_COEF_BITS;

    return coef * pow(2.0, exp);
}

static int chrony_init_seq(chrony_ctx_t *ctx)
{
    /* Initialize the sequence number generator from /dev/urandom */
    /* Fallbacks: /dev/random and time(NULL) */
    /* Try urandom */
    int fh = open(URAND_DEVICE_PATH, O_RDONLY);
    if (fh >= 0) {
        ssize_t rc = read(fh, &ctx->rand, sizeof(ctx->rand));
        if (rc != sizeof(ctx->rand)) {
            PLUGIN_ERROR("Reading from random source '%s' failed: %s (%d)",
                         URAND_DEVICE_PATH, strerror(errno), errno);
            close(fh);
            return -1;
        }
        close(fh);
        PLUGIN_DEBUG("Seeding RNG from " URAND_DEVICE_PATH);
    } else {
        if (errno == ENOENT) {
            /* URAND_DEVICE_PATH device not found. Try RAND_DEVICE_PATH as fall-back */
            fh = open(RAND_DEVICE_PATH, O_RDONLY);
            if (fh >= 0) {
                ssize_t rc = read(fh, &ctx->rand, sizeof(ctx->rand));
                if (rc != sizeof(ctx->rand)) {
                    PLUGIN_ERROR("Reading from random source '%s' failed: %s (%d)",
                                 RAND_DEVICE_PATH, strerror(errno), errno);
                    close(fh);
                    return -1;
                }
                close(fh);
                PLUGIN_DEBUG("Seeding RNG from " RAND_DEVICE_PATH);
            } else {
                /* Error opening RAND_DEVICE_PATH. Try time(NULL) as fall-back */
                PLUGIN_DEBUG("Seeding RNG from time(NULL)");
                ctx->rand = time(NULL) ^ getpid();
            }
        } else {
            PLUGIN_ERROR("Opening random source '%s' failed: %s (%d)",
                         URAND_DEVICE_PATH, strerror(errno), errno);
            return -1;
        }
    }

    return 0;
}

static int chrony_request_daemon_stats(chrony_ctx_t *ctx)
{
    /* Perform Tracking request */
    chrony_request_t chrony_req;
    chrony_init_req(&chrony_req);

    size_t chrony_resp_size;
    chrony_response_t chrony_resp;
    int rc = chrony_query(ctx, REQ_TRACKING, &chrony_req, &chrony_resp, &chrony_resp_size);
    if (rc != 0) {
        PLUGIN_ERROR("chrony_query (REQ_TRACKING) failed with status %i", rc);
        return rc;
    }
#ifdef NCOLLECTD_DEBUG
    {
        char src_addr[IPV6_STR_MAX_SIZE] = {0};
        niptoha(&chrony_resp.body.tracking.addr, src_addr, sizeof(src_addr));
        PLUGIN_DEBUG(": Daemon stat: .addr = %s, .ref_id= %u, .stratum = %u, .leap_status "
                     "= %u, .ref_time = %u:%u:%u, .current_correction = %f, .last_offset "
                     "= %f, .rms_offset = %f, .freq_ppm = %f, .skew_ppm = %f, .root_delay "
                     "= %f, .root_dispersion = %f, .last_update_interval = %f",
                     src_addr, ntohs(chrony_resp.body.tracking.f_ref_id),
                     ntohs(chrony_resp.body.tracking.f_stratum),
                     ntohs(chrony_resp.body.tracking.f_leap_status),
                     ntohl(chrony_resp.body.tracking.f_ref_time.tv_sec_high),
                     ntohl(chrony_resp.body.tracking.f_ref_time.tv_sec_low),
                     ntohl(chrony_resp.body.tracking.f_ref_time.tv_nsec),
                     ntohf(chrony_resp.body.tracking.f_current_correction),
                     ntohf(chrony_resp.body.tracking.f_last_offset),
                     ntohf(chrony_resp.body.tracking.f_rms_offset),
                     ntohf(chrony_resp.body.tracking.f_freq_ppm),
                     ntohf(chrony_resp.body.tracking.f_skew_ppm),
                     ntohf(chrony_resp.body.tracking.f_root_delay),
                     ntohf(chrony_resp.body.tracking.f_root_dispersion),
                     ntohf(chrony_resp.body.tracking.f_last_update_interval));
    }
#endif

    double time_ref = ntohl(chrony_resp.body.tracking.f_ref_time.tv_nsec);
    time_ref /= 1000000000.0;
    time_ref += ntohl(chrony_resp.body.tracking.f_ref_time.tv_sec_low);
    if (chrony_resp.body.tracking.f_ref_time.tv_sec_high) {
        double secs_high = ntohl(chrony_resp.body.tracking.f_ref_time.tv_sec_high);
        secs_high *= 4294967296.0;
        time_ref += secs_high;
    }

    /* Forward results to ncollectd-daemon */
    /* source is always 'chrony' to tag daemon-wide data */
    value_t value = {0};

    value = VALUE_GAUGE(ntohs(chrony_resp.body.tracking.f_stratum));
    metric_family_append(&ctx->fams[FAM_CHRONY_CLOCK_STRATUM], value, &ctx->labels,
                         &(label_pair_const_t){.name="source", .value="chrony"}, NULL);

    value = VALUE_GAUGE(time_ref); /* unit: s */
    metric_family_append(&ctx->fams[FAM_CHRONY_TIME_REF], value, &ctx->labels,
                         &(label_pair_const_t){.name="source", .value="chrony"}, NULL);

    /* Offset between system time and NTP, unit: s */
    value = VALUE_GAUGE(ntohf(chrony_resp.body.tracking.f_current_correction));
    metric_family_append(&ctx->fams[FAM_CHRONY_TIME_OFFSET_NTP], value, &ctx->labels,
                         &(label_pair_const_t){.name="source", .value="chrony"}, NULL);

    /* Estimated Offset of the NTP time, unit: s */
    value = VALUE_GAUGE(ntohf(chrony_resp.body.tracking.f_last_offset));
    metric_family_append(&ctx->fams[FAM_CHRONY_TIME_OFFSET], value, &ctx->labels,
                         &(label_pair_const_t){.name="source", .value="chrony"}, NULL);

    /* averaged value of the above, unit: s */
    value = VALUE_GAUGE(ntohf(chrony_resp.body.tracking.f_rms_offset));
    metric_family_append(&ctx->fams[FAM_CHRONY_TIME_OFFSET_RMS], value, &ctx->labels,
                         &(label_pair_const_t){.name="source", .value="chrony"}, NULL);

    /* Frequency error of the local osc, unit: ppm */
    value = VALUE_GAUGE(ntohf(chrony_resp.body.tracking.f_freq_ppm));
    metric_family_append(&ctx->fams[FAM_CHRONY_FREQUENCY_ERROR], value, &ctx->labels,
                         &(label_pair_const_t){.name="source", .value="chrony"}, NULL);

    value = VALUE_GAUGE(ntohf(chrony_resp.body.tracking.f_skew_ppm));
    metric_family_append(&ctx->fams[FAM_CHRONY_CLOCK_SKEW], value, &ctx->labels,
                         &(label_pair_const_t){.name="source", .value="chrony"}, NULL);
    /* Network latency between local daemon and the current source */
    value = VALUE_GAUGE(ntohf(chrony_resp.body.tracking.f_root_delay));
    metric_family_append(&ctx->fams[FAM_CHRONY_ROOT_DELAY], value, &ctx->labels,
                         &(label_pair_const_t){.name="source", .value="chrony"}, NULL);

    value = VALUE_GAUGE(ntohf(chrony_resp.body.tracking.f_root_dispersion));
    metric_family_append(&ctx->fams[FAM_CHRONY_ROOT_DISPERSION], value, &ctx->labels,
                         &(label_pair_const_t){.name="source", .value="chrony"}, NULL);

    value = VALUE_GAUGE(ntohf(chrony_resp.body.tracking.f_last_update_interval));
    metric_family_append(&ctx->fams[FAM_CHRONY_CLOCK_LAST_UPDATE], value, &ctx->labels,
                         &(label_pair_const_t){.name="source", .value="chrony"}, NULL);

    return 0;
}

static int chrony_request_sources_count(chrony_ctx_t *ctx, unsigned int *count)
{
    /* Requests the number of time sources from the chrony daemon */
    size_t chrony_resp_size;
    chrony_request_t chrony_req;
    chrony_response_t chrony_resp;

    PLUGIN_DEBUG("Requesting data");
    chrony_init_req(&chrony_req);
    int status = chrony_query(ctx, REQ_N_SOURCES, &chrony_req, &chrony_resp, &chrony_resp_size);
    if (status != 0) {
        PLUGIN_ERROR("chrony_query (REQ_N_SOURCES) failed with status %i", status);
        return status;
    }

    *count = ntohl(chrony_resp.body.n_sources.f_n_sources);
    PLUGIN_DEBUG("Getting data of %u clock sources", *count);

    return 0;
}

static int chrony_request_source_data(chrony_ctx_t *ctx, int src_idx, char *src_addr,
                                      size_t addr_size, int *is_reachable)
{
    /* Perform Source data request for source #p_src_idx */
    size_t chrony_resp_size;
    chrony_request_t chrony_req;
    chrony_response_t chrony_resp;

    chrony_init_req(&chrony_req);
    chrony_req.body.source_data.f_index = htonl(src_idx);
    int rc = chrony_query(ctx, REQ_SOURCE_DATA, &chrony_req, &chrony_resp, &chrony_resp_size);
    if (rc != 0) {
        PLUGIN_ERROR("chrony_query (REQ_SOURCE_DATA) failed with status %i", rc);
        return rc;
    }

    if (ntohs(chrony_resp.body.source_data.f_mode) == MODE_REFCLOCK)
        nreftostr(chrony_resp.body.source_data.addr.addr.ip4, src_addr, addr_size);
    else
        niptoha(&chrony_resp.body.source_data.addr, src_addr, addr_size);

    PLUGIN_DEBUG("Source[%d] data: .addr = %s, .poll = %u, .stratum = %u, "
                 ".state = %u, .mode = %u, .flags = %u, .reach = %u, "
                 ".latest_meas_ago = %u, .orig_latest_meas = %f, "
                 ".latest_meas = %f, .latest_meas_err = %f",
                 src_idx, src_addr, ntohs(chrony_resp.body.source_data.f_poll),
                 ntohs(chrony_resp.body.source_data.f_stratum),
                 ntohs(chrony_resp.body.source_data.f_state),
                 ntohs(chrony_resp.body.source_data.f_mode),
                 ntohs(chrony_resp.body.source_data.f_flags),
                 ntohs(chrony_resp.body.source_data.f_reachability),
                 ntohl(chrony_resp.body.source_data.f_since_sample),
                 ntohf(chrony_resp.body.source_data.f_origin_latest_meas),
                 ntohf(chrony_resp.body.source_data.f_latest_meas),
                 ntohf(chrony_resp.body.source_data.f_latest_meas_err));

    /* Push NaN if source is currently not reachable */
    *is_reachable = ntohs(chrony_resp.body.source_data.f_reachability) & 0x01;

    /* Forward results to ncollectd-daemon */
    value_t value = {0};

    value = VALUE_GAUGE(is_reachable ? ntohs(chrony_resp.body.source_data.f_stratum) : NAN);
    metric_family_append(&ctx->fams[FAM_CHRONY_CLOCK_STRATUM], value, &ctx->labels,
                         &(label_pair_const_t){.name="source", .value=src_addr}, NULL);

    value = VALUE_GAUGE(is_reachable ? ntohs(chrony_resp.body.source_data.f_state) : NAN);
    metric_family_append(&ctx->fams[FAM_CHRONY_CLOCK_STATE], value, &ctx->labels,
                         &(label_pair_const_t){.name="source", .value=src_addr}, NULL);

    value = VALUE_GAUGE(is_reachable ? ntohs(chrony_resp.body.source_data.f_mode) : NAN);
    metric_family_append(&ctx->fams[FAM_CHRONY_CLOCK_MODE], value, &ctx->labels,
                         &(label_pair_const_t){.name="source", .value=src_addr}, NULL);

    value = VALUE_GAUGE(is_reachable ? ntohs(chrony_resp.body.source_data.f_reachability) : NAN);
    metric_family_append(&ctx->fams[FAM_CHRONY_CLOCK_REACHABILITY], value, &ctx->labels,
                         &(label_pair_const_t){.name="source", .value=src_addr}, NULL);

    value = VALUE_GAUGE(is_reachable ? ntohl(chrony_resp.body.source_data.f_since_sample) : NAN);
    metric_family_append(&ctx->fams[FAM_CHRONY_CLOCK_LAST_MEAS], value, &ctx->labels,
                         &(label_pair_const_t){.name="source", .value=src_addr}, NULL);

    value = VALUE_GAUGE(is_reachable ? ntohf(chrony_resp.body.source_data.f_origin_latest_meas)
                                       : NAN);
    metric_family_append(&ctx->fams[FAM_CHRONY_TIME_OFFSET], value, &ctx->labels,
                         &(label_pair_const_t){.name="source", .value=src_addr}, NULL);

    return 0;
}

static int chrony_request_source_stats(chrony_ctx_t *ctx, int src_idx,
                                       const char *src_addr, const int is_reachable)
{
    /* Perform Source stats request for source #src_idx */
    size_t chrony_resp_size;
    chrony_request_t chrony_req;
    chrony_response_t chrony_resp;
    double skew_ppm, frequency_error;

    if (is_reachable == 0) {
        skew_ppm = 0;
        frequency_error = 0;
    } else {
        chrony_init_req(&chrony_req);
        chrony_req.body.source_stats.f_index = htonl(src_idx);
        int rc = chrony_query(ctx, REQ_SOURCE_STATS, &chrony_req, &chrony_resp, &chrony_resp_size);
        if (rc != 0) {
            PLUGIN_ERROR("chrony_query (REQ_SOURCE_STATS) failed with status %i", rc);
            return rc;
        }

        skew_ppm = ntohf(chrony_resp.body.source_stats.f_skew_ppm);
        frequency_error = ntohf(chrony_resp.body.source_stats.f_rtc_gain_rate_ppm);

        PLUGIN_DEBUG(": Source[%d] stat: .addr = %s, .ref_id= %u, .n_samples = %u, "
                     ".n_runs = %u, .span_seconds = %u, .rtc_seconds_fast = %f, "
                     ".rtc_gain_rate_ppm = %f, .skew_ppm= %f, .est_offset = %f, "
                     ".est_offset_err = %f",
                     src_idx, src_addr, ntohl(chrony_resp.body.source_stats.f_ref_id),
                     ntohl(chrony_resp.body.source_stats.f_n_samples),
                     ntohl(chrony_resp.body.source_stats.f_n_runs),
                     ntohl(chrony_resp.body.source_stats.f_span_seconds),
                     ntohf(chrony_resp.body.source_stats.f_rtc_seconds_fast),
                     frequency_error, skew_ppm,
                     ntohf(chrony_resp.body.source_stats.f_est_offset),
                     ntohf(chrony_resp.body.source_stats.f_est_offset_err));

    }

    /* Forward results to ncollectd-daemon */

    value_t value = {0};

    value = VALUE_GAUGE(is_reachable ? skew_ppm : NAN);
    metric_family_append(&ctx->fams[FAM_CHRONY_CLOCK_SKEW], value, &ctx->labels,
                         &(label_pair_const_t){.name="source", .value=src_addr}, NULL);

    value = VALUE_GAUGE(is_reachable ? frequency_error : NAN); /* unit: ppm */
    metric_family_append(&ctx->fams[FAM_CHRONY_FREQUENCY_ERROR], value, &ctx->labels,
                         &(label_pair_const_t){.name="source", .value=src_addr}, NULL);

    return 0;
}

static int chrony_read(user_data_t *user_data)
{
    chrony_ctx_t *ctx = user_data->data;

    /* ncollectd read callback: Perform data acquisition */
    unsigned int n_sources;

    if (!ctx->seq_is_initialized) {
        /* Seed RNG for sequence number generation */
        int status = chrony_init_seq(ctx);
        if (status != 0)
            return status ;

        ctx->seq_is_initialized = true;
    }

    /* Ignore late responses that may have been received */
    chrony_flush_recv_queue(ctx);

    /* Get daemon stats */
    int status = chrony_request_daemon_stats(ctx);
    if (status != 0)
        return status;

    /* Get number of time sources, then check every source for status */
    status = chrony_request_sources_count(ctx, &n_sources);
    if (status != 0)
        return status;

    for (unsigned int now_src = 0; now_src < n_sources; ++now_src) {
        char src_addr[IPV6_STR_MAX_SIZE] = {0};
        int is_reachable = 0;
        status = chrony_request_source_data(ctx, now_src, src_addr, sizeof(src_addr), &is_reachable);
        if (status != 0)
            return status;

        status = chrony_request_source_stats(ctx, now_src, src_addr, is_reachable);
        if (status != 0)
            return status;
    }

    plugin_dispatch_metric_family_array(ctx->fams, FAM_CHRONY_MAX, 0);

    return 0;
}

static void chrony_free(void *arg)
{
    chrony_ctx_t *ctx = arg;

    if (ctx == NULL)
        return;

    if (ctx->is_connected) {
        close(ctx->socket);
        ctx->is_connected = false;
    }

    free(ctx->name);
    free(ctx->host);
    free(ctx->port);

    label_set_reset(&ctx->labels);

    free(ctx);
}

static int chrony_config_instance(config_item_t *ci)
{
    chrony_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    ctx->timeout = TIME_T_TO_CDTIME_T(CHRONY_DEFAULT_TIMEOUT);
    ctx->is_connected = false;
    ctx->seq_is_initialized = false;
    ctx->socket = -1;
    ctx->rand = 1;

    memcpy(ctx->fams, fams, sizeof(ctx->fams[0])*FAM_CHRONY_MAX);

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
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &ctx->timeout);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &ctx->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else {
            PLUGIN_ERROR("Option '%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }


    if (status != 0) {
        chrony_free(ctx);
        return -1;
    }

    if (ctx->host == NULL) {
        ctx->host = strdup(CHRONY_DEFAULT_HOST);
        if (ctx->host == NULL) {
            PLUGIN_ERROR("strdup failed.");
            chrony_free(ctx);
            return -1;
        }
    }

    if (ctx->port == NULL) {
        ctx->port = strdup(CHRONY_DEFAULT_PORT);
        if (ctx->port == NULL) {
            PLUGIN_ERROR("strdup failed.");
            chrony_free(ctx);
            return -1;
        }
    }

    label_set_add(&ctx->labels, true, "instance", ctx->name);

    return plugin_register_complex_read("chrony", ctx->name, chrony_read, interval,
                                        &(user_data_t){.data = ctx, .free_func = chrony_free});
}

static int chrony_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = chrony_config_instance(child);
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
    plugin_register_config("chrony", chrony_config);
}
