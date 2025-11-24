// SPDX-License-Identifier: GPL-2.0-only OR MIT OR BSD-2-Clause
// SPDX-FileCopyrightText: Copyright (C) 2006-2012 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileCopyrightText: Copyright (c) 1992-2015 University of Delaware
// SPDX-FileCopyrightText: Copyright (c) 2011-2024 Network Time Foundation
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

/* Based on ntpq from ntp distribution */

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/time.h"
#include "libutils/socket.h"
#include "libutils/strbuf.h"

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#define NTPD_DEFAULT_HOST "localhost"
#define NTPD_DEFAULT_PORT "123"

#define	MAX_MAC_LEN	(6 * sizeof(uint32_t))	/* any MAC */

typedef union ctl_pkt_u_tag {
    uint8_t data[480 + MAX_MAC_LEN]; /* data + auth */
    uint32_t u32[(480 + MAX_MAC_LEN) / sizeof(uint32_t)];
} ctl_pkt_u;

typedef uint16_t associd_t;

struct ntp_control {
    uint8_t li_vn_mode;  /* leap, version, mode */
    uint8_t r_m_e_op;    /* response, more, error, opcode */
    uint16_t sequence;   /* sequence number of request */
    uint16_t status;     /* status word for association */
    associd_t associd;   /* association ID */
    uint16_t offset;     /* offset of this batch of data */
    uint16_t count;      /* count of data in this packet */
    ctl_pkt_u u;
};

/* Length of the control header, in octets */
#define CTL_HEADER_LEN      (offsetof(struct ntp_control, u))
#define CTL_MAX_DATA_LEN    468

/* Decoding for the r_m_e_op field */
#define CTL_RESPONSE    0x80
#define CTL_ERROR   0x40
#define CTL_MORE    0x20
#define CTL_OP_MASK 0x1f

#define CTL_ISRESPONSE(r_m_e_op) ((CTL_RESPONSE & (r_m_e_op)) != 0)
#define CTL_ISMORE(r_m_e_op)     ((CTL_MORE & (r_m_e_op)) != 0)
#define CTL_ISERROR(r_m_e_op)    ((CTL_ERROR    & (r_m_e_op)) != 0)
#define CTL_OP(r_m_e_op)         (CTL_OP_MASK   & (r_m_e_op))

/* Opcodes */
#define CTL_OP_UNSPEC         0   /* unspeciffied */
#define CTL_OP_READSTAT       1   /* read status */
#define CTL_OP_READVAR        2   /* read variables */
#define CTL_OP_WRITEVAR       3   /* write variables */
#define CTL_OP_READCLOCK      4   /* read clock variables */
#define CTL_OP_WRITECLOCK     5   /* write clock variables */
#define CTL_OP_SETTRAP        6   /* set trap address */
#define CTL_OP_ASYNCMSG       7   /* asynchronous message */
#define CTL_OP_CONFIGURE      8   /* runtime configuration */
#define CTL_OP_SAVECONFIG     9   /* save config to file */
#define CTL_OP_READ_MRU       10  /* retrieve MRU (mrulist) */
#define CTL_OP_READ_ORDLIST_A 11  /* ordered list req. auth. */
#define CTL_OP_REQ_NONCE      12  /* request a client nonce */
#define CTL_OP_UNSETTRAP      31  /* unset trap */

#define CTL_PEER_STATVAL(status)(((status)>>8) & 0xff)

/* Error code responses returned when the E bit is set. */
#define CERR_UNSPEC     0
#define CERR_PERMISSION 1
#define CERR_BADFMT     2
#define CERR_BADOP      3
#define CERR_BADASSOC   4
#define CERR_UNKNOWNVAR 5
#define CERR_BADVALUE   6
#define CERR_RESTRICT   7

#define CERR_NORESOURCE CERR_PERMISSION /* wish there was a different code */

#define	MAXFRAGS	32
#define	DATASIZE	(MAXFRAGS*480)	/* maximum amount of data */

#define VN_MODE(v, m)		((((v) & 7) << 3) | ((m) & 0x7))
#define	PKT_VERSION(li_vn_mode)	((u_char)(((li_vn_mode) >> 3) & 0x7))
#define	PKT_MODE(li_vn_mode)	((u_char)((li_vn_mode) & 0x7))
#define	PKT_LI_VN_MODE(l, v, m) ((((l) & 3) << 6) | VN_MODE((v), (m)))

#define	MODE_CONTROL	6	/* control mode */

#define	NTP_OLDVERSION	((u_char)1) /* oldest credible version */
#define	NTP_VERSION	    ((u_char)4) /* current version number */

#if 0
#define	ERR_UNSPEC		256
#define	ERR_INCOMPLETE	257
#define	ERR_TIMEOUT		258
#define	ERR_TOOMUCH		259
#endif

#define	DEFTIMEOUT	5		/* wait 5 seconds for 1st pkt */
#define	DEFSTIMEOUT	3		/* and 3 more for each additional */

static u_char pktversion = NTP_OLDVERSION + 1;

#define	MAXVARLEN	256		/* maximum length of a variable name */
#define	MAXVALLEN	2048		/* maximum length of a variable value */

enum {
    FAM_NTPD_UP,
    FAM_NTPD_LEAP_STATUS,
    FAM_NTPD_STRATUM,
    FAM_NPTD_ROOT_DELAY,
    FAM_NPTD_ROOT_DISPERSION,
    FAM_NPTD_ROOT_DISTANCE,
    FAM_NTPD_SYSTEM_JITTER,
    FAM_NTPD_CLOCK_JITTER,
    FAM_NTPD_CLOCK_FREQUENCY_WANDER,
    FAM_NTPD_UPTIME,
    FAM_NTPD_REQUESTS_CONTROL,
    FAM_NTPD_PACKETS_RECEIVED,
    FAM_NTPD_REQUESTS_CURRENT_VERSION,
    FAM_NTPD_REQUESTS_OLDER_VERSION,
    FAM_NTPD_REQUESTS_BAD,
    FAM_NTPD_AUTHENTICATION_FAILED,
    FAM_NTPD_REQUESTS_DECLINED,
    FAM_NTPD_REQUESTS_RESTRICTED,
    FAM_NTPD_REQUESTS_RATE_LIMITED,
    FAM_NTPD_RESPONSES_KODS,
    FAM_NTPD_PACKETS_PROCESSED,
    FAM_NTPD_RECEIVE_BUFFERS,
    FAM_NTPD_RECEIVE_BUFFERS_FREE,
    FAM_NTPD_IO_PACKETS_DROPPED,
    FAM_NTPD_IO_PACKETS_IGNORED,
    FAM_NTPD_IO_PACKETS_RECEIVED,
    FAM_NTPD_IO_PACKETS_SEND,
    FAM_NTPD_IO_PACKETS_SEND_FAILURES,
    FAM_NTPD_IO_WAKEUPS,
    FAM_NTPD_IO_GOOOD_WAKEUPS,
    FAM_NTPD_AUTH_KEYS,
    FAM_NTPD_AUTH_KEYS_FREE,
    FAM_NTPD_AUTH_KEYS_LOOKUPS,
    FAM_NTPD_AUTH_KEYS_NOTFOUND,
    FAM_NTPD_AUTH_ENCRYPTS,
    FAM_NTPD_AUTH_DIGEST_ENCRYPTS,
    FAM_NTPD_AUTH_CMAC_ENCRYPTS,
    FAM_NTPD_AUTH_DECRYPTS,
    FAM_NTPD_AUTH_DIGEST_DECRYPTS,
    FAM_NTPD_AUTH_DIGEST_FAILS,
    FAM_NTPD_AUTH_CMAC_DECRYPTS,
    FAM_NTPD_AUTH_CMAC_FAILS,
    FAM_NTPD_KERNEL_PLL_OFFSET,
    FAM_NTPD_KERNEL_PLL_FREQUENCY,
    FAM_NTPD_KERNEL_PLL_MAXIMUM_ERROR,
    FAM_NTPD_KERNEL_PLL_ESTIMATED_ERROR,
    FAM_NTPD_KERNEL_CLOCK_STATUS,
    FAM_NTPD_KERNEL_PLL_TIME_CONSTANT,
    FAM_NTPD_KERNEL_CLOCK_PRECISION,
    FAM_NTPD_KERNEL_CLOCK_FREQUENCY_TOLERANCE,
    FAM_NTPD_KERNEL_PPS_FREQUENCY,
    FAM_NTPD_KERNEL_PPS_JITTER,
    FAM_NTPD_KERNEL_PPS_CALIBRATION_INTERVAL,
    FAM_NTPD_KERNEL_PPS_STABILITY,
    FAM_NTPD_KERNEL_PPS_JITTER_LIMIT,
    FAM_NTPD_KERNEL_PPS_CALIBRATION_CICLES,
    FAM_NTPD_KERNEL_PPS_CALIBRATION_ERROR,
    FAM_NTPD_KERNEL_PPS_STABILITY_EXCEEDED,
    FAM_NTPD_NTS_CLIENT_SENDS,
    FAM_NTPD_NTS_CLIENT_RECVS_GOOD,
    FAM_NTPD_NTS_CLIENT_RECVS_BAD,
    FAM_NTPD_NTS_SERVER_RECVS_GOOD,
    FAM_NTPD_NTS_SERVER_RECVS_BAD,
    FAM_NTPD_NTS_SERVER_SENDS,
    FAM_NTPD_NTS_COOKIE_MAKE,
    FAM_NTPD_NTS_COOKIE_NOT_SERVER,
    FAM_NTPD_NTS_COOKIE_DECODE,
    FAM_NTPD_NTS_COOKIE_DECODE_ERROR,
    FAM_NTPD_NTS_KE_SERVES_GOOD,
    FAM_NTPD_NTS_KE_SERVES_GOOD_WALL,
    FAM_NTPD_NTS_KE_SERVES_GOOD_CPU,
    FAM_NTPD_NTS_KE_SERVES_NO_TLS,
    FAM_NTPD_NTS_KE_SERVES_NO_TLS_WALL,
    FAM_NTPD_NTS_KE_SERVES_NO_TLS_CPU,
    FAM_NTPD_NTS_KE_SERVES_BAD,
    FAM_NTPD_NTS_KE_SERVES_BAD_WALL,
    FAM_NTPD_NTS_KE_SERVES_BAD_CPU,
    FAM_NTPD_NTS_KE_PROBES_GOOD,
    FAM_NTPD_NTS_KE_PROBES_BAD,
    FAM_NTPD_PEER_STRATUM,
    FAM_NTPD_PEER_DISPERSION_SECONDS,
    FAM_NTPD_PEER_OFFSET_SECONDS,
    FAM_NTPD_PEER_DELAY_SECONDS,
    FAM_NTPD_PEER_STATUS,
    FAM_NTPD_MAX
};

static metric_family_t fams[FAM_NTPD_MAX] = {
    [FAM_NTPD_UP] = {
        .name = "ntpd_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the ntpd server be reached.",
    },
    [FAM_NTPD_LEAP_STATUS] = {
        .name = "ntpd_leap_status",
        .type = METRIC_TYPE_GAUGE,
        .help = "The leap status can be: 0 Normal, 1 Insert second, 2 Delete second or "
                "3 Not synchronized.",
    },
    [FAM_NTPD_STRATUM] = {
        .name = "nptd_stratum",
        .type = METRIC_TYPE_GAUGE,
        .help = "The distance from the reference clock."
    },
    [FAM_NPTD_ROOT_DELAY] = {
        .name = "nptd_root_delay_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Roundtrip delay to the primary reference clock in seconds."
    },
    [FAM_NPTD_ROOT_DISPERSION] = {
        .name = "nptd_root_dispersion_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Dispersion to the primary reference clock in seconds."
    },
    [FAM_NPTD_ROOT_DISTANCE] = {
        .name = "nptd_root_distance_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Distance to the primary reference clock in seconds."
    },
    [FAM_NTPD_SYSTEM_JITTER] = {
        .name = "ntpd_system_jitter_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_NTPD_CLOCK_JITTER] = {
        .name = "ntpd_clock_jitter_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_NTPD_CLOCK_FREQUENCY_WANDER] = {
        .name = "ntpd_clock_frequency_wander_ppm",
        .type = METRIC_TYPE_GAUGE,
        .help = "Clock frequency wander in parts per million."
    },
    [FAM_NTPD_PEER_STRATUM] = {
        .name = "ntpd_peer_stratum",
        .type = METRIC_TYPE_GAUGE,
        .help = "NTPD peer stratum",
    },
    [FAM_NTPD_PEER_DISPERSION_SECONDS] = {
        .name = "ntpd_peer_dispersion_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "NTPD peer dispersion",
    },
    [FAM_NTPD_PEER_OFFSET_SECONDS] = {
        .name = "ntpd_peer_offset_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "ClockOffset between NTP and local clock",
    },
    [FAM_NTPD_PEER_DELAY_SECONDS] = {
        .name = "ntpd_peer_delay_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "NTPD peer delay",
    },
    [FAM_NTPD_PEER_STATUS] = {
        .name = "ntpd_peer_status",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current selection status of this peer. "
                "0 discarded as not valid. "
                "1 discarded by intersection algorithm. "
                "2 discarded by table overflow. "
                "3 discarded by the cluster algorithm. "
                "4 included by the combine algorithm. "
                "5 backup (more than +tos maxclock+ sources). "
                "6 system peer. "
                "7 PPS peer (when the prefer peer is valid)."
    },
    [FAM_NTPD_UPTIME] = {
        .name = "ntpd_uptime",
        .type = METRIC_TYPE_GAUGE,
        .help = "Uptime in seconds of NTP Daemon.",
    },
    [FAM_NTPD_REQUESTS_CONTROL] = {
        .name = "ntpd_requests_control",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of control request.",
    },
    [FAM_NTPD_PACKETS_RECEIVED] = {
        .name = "ntpd_packets_received",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of packets received.",
    },
    [FAM_NTPD_REQUESTS_CURRENT_VERSION] = {
        .name = "ntpd_requests_current_version",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of client requests matching server protocol version.",
    },
    [FAM_NTPD_REQUESTS_OLDER_VERSION] = {
        .name = "ntpd_requests_older_version",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of older version requests from clients than "
                "the server protocol version."
    },
    [FAM_NTPD_REQUESTS_BAD] = {
        .name = "ntpd_requests_bad",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total request with bad length or format.",
    },
    [FAM_NTPD_AUTHENTICATION_FAILED] = {
        .name = "ntpd_authentication_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of authentication failures.",
    },
    [FAM_NTPD_REQUESTS_DECLINED] = {
        .name = "ntpd_requests_declined",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of declined requests.",
    },
    [FAM_NTPD_REQUESTS_RESTRICTED] = {
        .name = "ntpd_requests_restricted",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of restricted requests.",
    },
    [FAM_NTPD_REQUESTS_RATE_LIMITED] = {
        .name = "ntpd_requests_rate_limited",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of rate limeted requests."
    },
    [FAM_NTPD_RESPONSES_KODS] = {
        .name = "ntpd_responses_kod",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of kiss-o'-death (KoD) responses.",
    },
    [FAM_NTPD_PACKETS_PROCESSED] = {
        .name = "ntpd_packets_processed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of packets packets for this host.",
    },
    [FAM_NTPD_RECEIVE_BUFFERS] = {
        .name = "ntpd_receive_buffers",
        .type = METRIC_TYPE_GAUGE,
        .help = "recvbufs currently in use.",
    },
    [FAM_NTPD_RECEIVE_BUFFERS_FREE] = {
        .name = "ntpd_receive_buffers_free",
        .type = METRIC_TYPE_GAUGE,
        .help = "recvbufs on free_recv_list.",
    },
    [FAM_NTPD_IO_PACKETS_DROPPED] = {
        .name = "ntpd_io_packets_dropped",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of packets droped on reception.",
    },
    [FAM_NTPD_IO_PACKETS_IGNORED] = {
        .name = "ntpd_io_packets_ignored",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of packets received on wild card interface ",
    },
    [FAM_NTPD_IO_PACKETS_RECEIVED] = {
        .name = "ntpd_io_packets_received",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of packets received.",
    },
    [FAM_NTPD_IO_PACKETS_SEND] = {
        .name = "ntpd_io_packets_send",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of packets send.",
    },
    [FAM_NTPD_IO_PACKETS_SEND_FAILURES] = {
        .name = "ntpd_io_packets_send_failures",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of packets which couldn't be sent.",
    },
    [FAM_NTPD_IO_WAKEUPS] = {
        .name = "ntpd_io_wakeups",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of io wakeups.",
    },
    [FAM_NTPD_IO_GOOOD_WAKEUPS] = {
        .name = "ntpd_io_good_wakeups",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of input packets.",
    },
    [FAM_NTPD_AUTH_KEYS] = {
        .name = "ntpd_auth_keys",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of active keys.",
    },
    [FAM_NTPD_AUTH_KEYS_FREE] = {
        .name = "ntpd_auth_keys_free",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of free ketys.",
    },
    [FAM_NTPD_AUTH_KEYS_LOOKUPS] = {
        .name = "ntpd_auth_keys_lookups",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of calls to lookup keys.",
    },
    [FAM_NTPD_AUTH_KEYS_NOTFOUND] = {
        .name = "ntpd_auth_keys_notfound",
        .type = METRIC_TYPE_COUNTER,
        .help = "otal number of keys not found.",
    },
    [FAM_NTPD_AUTH_ENCRYPTS] = {
        .name = "ntpd_auth_encrypts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of calls to authencrypt.",
    },
    [FAM_NTPD_AUTH_DIGEST_ENCRYPTS] = {
        .name = "ntpd_auth_digest_encrypts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of calls to digest_encrypt.",
    },
    [FAM_NTPD_AUTH_CMAC_ENCRYPTS] = {
        .name = "ntpd_auth_cmac_encrypts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of calls to cmac_encrypt.",
    },
    [FAM_NTPD_AUTH_DECRYPTS] = {
        .name = "ntpd_auth_decrypts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of calls to authdecrypt.",
    },
    [FAM_NTPD_AUTH_DIGEST_DECRYPTS] = {
        .name = "ntpd_auth_digest_decrypts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of calls to digest_decrypt.",
    },
    [FAM_NTPD_AUTH_DIGEST_FAILS] = {
        .name = "ntpd_auth_digest_fails",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of fails from digest_decrypt.",
    },
    [FAM_NTPD_AUTH_CMAC_DECRYPTS] = {
        .name = "ntpd_auth_cmac_decrypts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of calls to cmac_decrypt.",
    },
    [FAM_NTPD_AUTH_CMAC_FAILS] = {
        .name = "ntpd_auth_cmac_fails",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of fails from cmac_decrypt.",
    },
    [FAM_NTPD_KERNEL_PLL_OFFSET] = {
        .name = "ntpd_kernel_pll_offset_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Kernel phase-locked loop offset  between local system "
                "and reference clock in seconds.",
    },
    [FAM_NTPD_KERNEL_PLL_FREQUENCY] = {
        .name = "ntpd_kernel_pll_frequency_ppm",
        .type = METRIC_TYPE_GAUGE,
        .help = "Kernel phase-locked loop frequency in parts per million.",
    },
    [FAM_NTPD_KERNEL_PLL_MAXIMUM_ERROR] = {
        .name = "ntpd_kernel_pll_maximum_error_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Maximum error for the kernel phase-locked loop in seconds.",
    },
    [FAM_NTPD_KERNEL_PLL_ESTIMATED_ERROR] = {
        .name = "ntpd_kernel_pll_estimated_error_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Estimated error for the kernel phase-locked loop in seconds.",
    },
    [FAM_NTPD_KERNEL_CLOCK_STATUS] = {
        .name = "ntpd_kernel_clock_status.",
        .type = METRIC_TYPE_GAUGE,
        .help = "Kernel clock status array bits.",
    },
    [FAM_NTPD_KERNEL_PLL_TIME_CONSTANT] = {
        .name = "ntpd_kernel_pll_time_constant",
        .type = METRIC_TYPE_GAUGE,
        .help = "Kernel phase-locked loop time constant."
    },
    [FAM_NTPD_KERNEL_CLOCK_PRECISION] = {
        .name = "ntpd_kernel_clock_precision_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Clock precision in seconds.",
    },
    [FAM_NTPD_KERNEL_CLOCK_FREQUENCY_TOLERANCE] = {
        .name = "ntpd_kernel_clock_frequency_tolerance_ppm",
        .type = METRIC_TYPE_GAUGE,
        .help = "Clock frequency tolerance in Parts Per Million.",
    },
    [FAM_NTPD_KERNEL_PPS_FREQUENCY] = {
        .name = "ntpd_kernel_pps_frequency_ppm",
        .type = METRIC_TYPE_GAUGE,
        .help = "Pulse per second frequency in Parts Per Million.",
    },
    [FAM_NTPD_KERNEL_PPS_JITTER] = {
        .name = "ntpd_kernel_pps_jitter_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Pulse per second jitter in seconds.",
    },
    [FAM_NTPD_KERNEL_PPS_CALIBRATION_INTERVAL] = {
        .name = "ntpd_kernel_pps_calibration_interval_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Pulse per second interval duration in seconds.",
    },
    [FAM_NTPD_KERNEL_PPS_STABILITY] = {
        .name = "ntpd_kernel_pps_stability_ppm",
        .type = METRIC_TYPE_GAUGE,
        .help = "Pulse per second stability in Parts Per Million."
    },
    [FAM_NTPD_KERNEL_PPS_JITTER_LIMIT] = {
        .name = "ntpd_kernel_pps_jitter_limit",
        .type = METRIC_TYPE_COUNTER,
        .help = "Pulse per second (PPS) count of jitter limit exceeded events.",
    },
    [FAM_NTPD_KERNEL_PPS_CALIBRATION_CICLES] = {
        .name = "ntpd_kernel_pps_calibration_cicles",
        .type = METRIC_TYPE_COUNTER,
        .help = "Pulse per second (PPS) count of calibration intervals.",
    },
    [FAM_NTPD_KERNEL_PPS_CALIBRATION_ERROR] = {
        .name = "ntpd_kernel_pps_calibration_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Pulse per second (PPS) count of calibration errors.",
    },
    [FAM_NTPD_KERNEL_PPS_STABILITY_EXCEEDED] = {
        .name = "ntpd_kernel_pps_stability_exceeded",
        .type = METRIC_TYPE_COUNTER,
        .help = "Pulse per second (PPS) count of stability limit exceeded events.",
    },
    [FAM_NTPD_NTS_CLIENT_SENDS] = {
        .name = "ntpd_nts_client_sends",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NTS client sends.",
    },
    [FAM_NTPD_NTS_CLIENT_RECVS_GOOD] = {
        .name = "ntpd_nts_client_recvs_good",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NTS client recvs good.",
    },
    [FAM_NTPD_NTS_CLIENT_RECVS_BAD] = {
        .name = "ntpd_nts_client_recvs_bad",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NTS client recvs w error.",
    },
    [FAM_NTPD_NTS_SERVER_RECVS_GOOD] = {
        .name = "ntpd_nts_server_recvs_good",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NTS server recvs good.",
    },
    [FAM_NTPD_NTS_SERVER_RECVS_BAD] = {
        .name = "ntpd_nts_server_recvs_bad",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NTS server recvs with error."
    },
    [FAM_NTPD_NTS_SERVER_SENDS] = {
        .name = "ntpd_nts_server_sends",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NTS server sends.",
    },
    [FAM_NTPD_NTS_COOKIE_MAKE] = {
        .name = "ntpd_nts_cookie_make",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NTS make cookies.",
    },
    [FAM_NTPD_NTS_COOKIE_NOT_SERVER] = {
        .name = "ntpd_nts_cookie_not_server",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NTS cookies not server.",
    },
    [FAM_NTPD_NTS_COOKIE_DECODE] = {
        .name = "ntpd_nts_cookie_decode",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NTS decode cookies total.",
    },
    [FAM_NTPD_NTS_COOKIE_DECODE_ERROR] = {
        .name = "ntpd_nts_cookie_decode_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NTS decode cookies error.",
    },
    [FAM_NTPD_NTS_KE_SERVES_GOOD] = {
        .name = "ntpd_nts_ke_serves_good",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NTS KE serves good.",
    },
    [FAM_NTPD_NTS_KE_SERVES_GOOD_WALL] = {
        .name = "ntpd_nts_ke_serves_good_wall",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NTS KE serves good wall.",
    },
    [FAM_NTPD_NTS_KE_SERVES_GOOD_CPU] = {
        .name = "ntpd_nts_ke_serves_good_cpu",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NTS KE serves good CPU.",
    },
    [FAM_NTPD_NTS_KE_SERVES_NO_TLS] = {
        .name = "ntpd_nts_ke_serves_no_tls",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NTS KE serves no-TLS.",
    },
    [FAM_NTPD_NTS_KE_SERVES_NO_TLS_WALL] = {
        .name = "ntpd_nts_ke_serves_no_tls_wall",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NTS KE serves no-TLS wall.",
    },
    [FAM_NTPD_NTS_KE_SERVES_NO_TLS_CPU] = {
        .name = "ntpd_nts_ke_serves_no_tls_cpu",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NTS KE serves no-TLS CPU.",
    },
    [FAM_NTPD_NTS_KE_SERVES_BAD] = {
        .name = "ntpd_nts_ke_serves_bad",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NTS KE serves bad.",
    },
    [FAM_NTPD_NTS_KE_SERVES_BAD_WALL] = {
        .name = "ntpd_nts_ke_serves_bad_wall",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NTS KE serves bad wall.",
    },
    [FAM_NTPD_NTS_KE_SERVES_BAD_CPU] = {
        .name = "ntpd_nts_ke_serves_bad_cpu",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NTS KE serves bad CPU.",
    },
    [FAM_NTPD_NTS_KE_PROBES_GOOD] = {
        .name = "ntpd_nts_ke_probes_good",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NTS KE client probes good.",
    },
    [FAM_NTPD_NTS_KE_PROBES_BAD] = {
        .name = "ntpd_nts_ke_probes_bad",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NTS KE client probes bad.",
    },
};

typedef enum {
    COLLECT_SYSINFO    = (1 <<  0),
    COLLECT_KERNINFO   = (1 <<  1),
    COLLECT_SYSSTATS   = (1 <<  2),
    COLLECT_AUTHINFO   = (1 <<  3),
    COLLECT_IOSTATS    = (1 <<  4),
    COLLECT_NTSINFO    = (1 <<  5),
    COLLECT_NTSKEINFO  = (1 <<  6),
    COLLECT_PEERS      = (1 <<  8),
} ntpd_flags_t;

static cf_flags_t ntpd_flags[] = {
    { "sysinfo",    COLLECT_SYSINFO    },
    { "kerninfo",   COLLECT_KERNINFO   },
    { "sysstats",   COLLECT_SYSSTATS   },
    { "authinfo",   COLLECT_AUTHINFO   },
    { "iostats",    COLLECT_IOSTATS    },
    { "ntsinfo",    COLLECT_NTSINFO    },
    { "ntskeinfo",  COLLECT_NTSKEINFO  },
    { "peers",      COLLECT_PEERS      },
};
static size_t ntpd_flags_size = STATIC_ARRAY_SIZE(ntpd_flags);

#include "plugins/ntpd/ntpd_vars.h"

typedef struct {
    char buffer[CTL_MAX_DATA_LEN];
    size_t len;
} data_frag_t;

typedef struct {
    data_frag_t *frags;
    size_t nfrags;
    size_t nvars;
} data_frags_t;

typedef struct {
    associd_t assid;
    uint16_t status;
} ntp_stat_t;

typedef struct {
    char *name;
    char *host;
    char *port;
    cdtime_t timeout;
    uint64_t flags;
    label_set_t labels;
    plugin_filter_t *filter;
    metric_family_t fams[FAM_NTPD_MAX];
    data_frags_t sys_vars;
    int sd;
    uint16_t sequence;
} ntpd_ctx_t;

typedef struct kv {
    char *key;
    char *value;
} kv_t;

static char *kv_parser_pair(char *str, kv_t *kv)
{
    if (str == NULL)
        return NULL;

    char *ptr = str;

    while ((*ptr == ',') || ((*ptr > 0) && (*ptr <= ' ')))
        ptr++;
    if (*ptr == '\0')
        return NULL;

    char *name = ptr;
    while ((*ptr != '\0') && (*ptr != ',') && (*ptr != '=') && (*ptr != '\r') && (*ptr != '\n'))
        ptr++;
    if (*ptr != '=')
        return NULL;
    if (ptr == name)
        return NULL;

    *ptr = '\0';
    ptr++;
    char *value = ptr;
    if (*ptr== '"') {
        ptr++;
        value = ptr;
        while ((*ptr != '\0') && (*ptr != '"') && (*ptr != '\r') && (*ptr != '\n'))
            ptr++;
        if (*ptr != '"')
            return NULL;
        if (ptr == value)
            return NULL;
        *ptr = '\0';
        ptr++;
    } else {
        while ((*ptr != '\0') && (*ptr != ',') && (*ptr != '"') && (*ptr != '\r') && (*ptr != '\n'))
            ptr++;
        if (ptr == value)
            return NULL;
        if (*ptr != '\0') {
            *ptr = '\0';
            ptr++;
        }
    }

    kv->key = name;
    kv->value = value;

    return ptr;
}

static size_t kv_parser_split(char *str, kv_t *kv, size_t size)
{
    size_t n = 0;

    while((str = kv_parser_pair(str, &kv[n])) != NULL) {
        n++;
        if (n >= size)
            break;
    }
    return n;
}

static void data_frags_reset(data_frags_t *dfrags)
{
    free(dfrags->frags);
    dfrags->frags = NULL;
    dfrags->nfrags = 0;
    dfrags->nvars= 0;
}

static int data_frags_alloc(data_frags_t *dfrags)
{
    data_frag_t *tmp = realloc(dfrags->frags, sizeof(data_frag_t) * (dfrags->nfrags+1));
    if (tmp == NULL)
        return -1;

    dfrags->frags = tmp;
    dfrags->frags[dfrags->nfrags].buffer[0] = '\0';
    dfrags->frags[dfrags->nfrags].len = 0;
    dfrags->nfrags++;

    return 0;
}

static int data_frags_append(data_frags_t *dfrags, char *var, size_t var_len)
{
    if (var_len >= MAXVARLEN)
        return -1;

    if (dfrags->nfrags == 0) {
        int status = data_frags_alloc(dfrags);
        if (status != 0)
            return -1;
    }

    data_frag_t *dfrag = &dfrags->frags[dfrags->nfrags-1];

    if ((sizeof(dfrag->buffer) - dfrag->len) < (var_len + 2)) {
        int status = data_frags_alloc(dfrags);
        if (status != 0)
            return -1;
        dfrag = &dfrags->frags[dfrags->nfrags-1];
    }

    if (dfrag->len > 0) {
        dfrag->buffer[dfrag->len] = ',';
        dfrag->len++;
    }

    memcpy(dfrag->buffer + dfrag->len, var, var_len);
    dfrag->buffer[dfrag->len + var_len] = '\0';
    dfrag->len += var_len;

    dfrags->nvars++;

    return 0;
}

static int str_comma_split(char *str, char **fields, size_t size)
{
    size_t i = 0;
    char *ptr = str;
    char *saveptr = NULL;
    while ((fields[i] = strtok_r(ptr, " ,", &saveptr)) != NULL) {
        ptr = NULL;
        i++;

        if (i >= size)
            break;
    }

    return (int)i;
}

static int ntp_get_response(ntpd_ctx_t *ctx, int opcode, int associd,
                            uint16_t *rstatus, char **rdata, size_t *rsize)
{
    uint16_t offsets[MAXFRAGS+1];
    uint16_t counts[MAXFRAGS+1];
    size_t numfrags = 0;
    int seenlastfrag = 0;

    memset(offsets, 0, sizeof(offsets));
    memset(counts , 0, sizeof(counts ));

    *rsize = 0;
    if (rstatus != NULL)
        *rstatus = 0;

    char *pktdata = *rdata;

    cdtime_t end = cdtime() + ctx->timeout;

    while (true) {
        cdtime_t now = cdtime();
        /* timeout reached */
        if (now > end)
            break;

        struct pollfd poll_s;
        poll_s.fd = ctx->sd;
        poll_s.events = POLLIN | POLLPRI;
        poll_s.revents = 0;

        int status = poll(&poll_s, 1, CDTIME_T_TO_MS(end-now));

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

        struct ntp_control rpkt = {0};
        ssize_t recvsize = recv(ctx->sd, (void *)&rpkt, sizeof(rpkt), 0 /* no flags */);

        if ((recvsize < 0) && ((errno == EAGAIN) || (errno == EINTR)))
            continue;

        if (recvsize < 0) {
            PLUGIN_INFO("recv(2) failed: %s", STRERRNO);
            PLUGIN_DEBUG("Closing socket #%i", ctx->sd);
            close(ctx->sd);
            ctx->sd = -1;
            return -1;
        }

        if (recvsize < (ssize_t)CTL_HEADER_LEN) {
            PLUGIN_ERROR("Short (%zd bytes) packet received.", recvsize);
            continue;
        }
        if ((PKT_VERSION(rpkt.li_vn_mode) > NTP_VERSION) ||
            (PKT_VERSION(rpkt.li_vn_mode) < NTP_OLDVERSION)) {
            PLUGIN_ERROR("Packet received with version %d.", PKT_VERSION(rpkt.li_vn_mode));
            continue;
        }
        if (PKT_MODE(rpkt.li_vn_mode) != MODE_CONTROL) {
            PLUGIN_ERROR("Packet received with mode %d.", PKT_MODE(rpkt.li_vn_mode));
            continue;
        }
        if (!CTL_ISRESPONSE(rpkt.r_m_e_op)) {
            PLUGIN_ERROR("Received request packet, wanted response.");
            continue;
        }
        if (ntohs(rpkt.sequence) != ctx->sequence) {
            PLUGIN_ERROR("Received sequnce number %d, wanted %d.",
                         ntohs(rpkt.sequence), ctx->sequence);
            continue;
        }
        if (CTL_OP(rpkt.r_m_e_op) != opcode) {
            PLUGIN_ERROR("Received opcode %d, wanted %d.", CTL_OP(rpkt.r_m_e_op), opcode);
            continue;
        }
        if (CTL_ISERROR(rpkt.r_m_e_op)) {
            int errcode = (ntohs(rpkt.status) >> 8) & 0xff;
            if (CTL_ISMORE(rpkt.r_m_e_op))
                PLUGIN_ERROR("Error code %d received on not-final packet\n", errcode);
            return -1;
        }
        if (ntohs(rpkt.associd) != associd) {
            PLUGIN_ERROR("Association ID %d doesn't match expected %d\n",
                         ntohs(rpkt.associd), associd);
            continue; // FIXME ¿?
        }

        uint16_t offset = ntohs(rpkt.offset);
        uint16_t count = ntohs(rpkt.count);

        if (recvsize & 0x3) {
            PLUGIN_ERROR("Response packet not padded, size = %zd\n", recvsize);
            continue;
        }

        int shouldbesize = (CTL_HEADER_LEN + count + 3) & ~3;

        if (recvsize < shouldbesize) {
            PLUGIN_ERROR("Response packet claims %u octets payload, above %zu received\n",
                         count, recvsize - CTL_HEADER_LEN);
            return -1;
        }

        if (count > (recvsize - CTL_HEADER_LEN)) {
            PLUGIN_ERROR("Received count of %u octets, data in packet is %zu\n",
                          count, recvsize - CTL_HEADER_LEN);
            continue;
        }
        if (count == 0 && CTL_ISMORE(rpkt.r_m_e_op)) {
            PLUGIN_ERROR("Received count of 0 in non-final fragment\n");
            continue;
        }
        if (offset + count > DATASIZE) {
            PLUGIN_ERROR("Offset %u, count %u, too big for buffer\n", offset, count);
            return -1;
        }
        if (seenlastfrag && !CTL_ISMORE(rpkt.r_m_e_op)) {
            PLUGIN_ERROR("Received second last fragment packet\n");
            continue;
        }
        if (numfrags > (MAXFRAGS - 1)) {
            PLUGIN_ERROR("Number of fragments exceeds maximum %d\n", MAXFRAGS - 1);
            return -1;
        }

        /*
         * Find the position for the fragment relative to any
         * previously received.
         */
        size_t f = 0;
        for (f = 0; f < numfrags && offsets[f] < offset; f++);

        if (f < numfrags && offset == offsets[f]) {
            PLUGIN_DEBUG("duplicate %u octets at %u ignored, prior %u at %u\n",
                          count, offset, counts[f], offsets[f]);
            continue;
        }

        if (f > 0 && (offsets[f-1] + counts[f-1]) > offset) {
            PLUGIN_DEBUG("received frag at %u overlaps with %u octet frag at %u\n",
                          offset, counts[f-1], offsets[f-1]);
            continue;
        }

        if (f < numfrags && (offset + count) > offsets[f]) {
            PLUGIN_DEBUG("received %u octet frag at %u overlaps with frag at %u\n",
                          count, offset, offsets[f]);
            continue;
        }

        for (size_t ff = numfrags; ff > f; ff--) {
            offsets[ff] = offsets[ff-1];
            counts[ff] = counts[ff-1];
        }
        offsets[f] = offset;
        counts[f] = count;
        numfrags++;

        /*
         * Got that stuffed in right.  Figure out if this was the last.
         * Record status info out of the last packet.
         */
        if (!CTL_ISMORE(rpkt.r_m_e_op)) {
            seenlastfrag = 1;
            if (rstatus != 0)
                *rstatus = ntohs(rpkt.status);
        }

        /*
         * Copy the data into the data buffer, and bump the
         * timout base in case we need more.
         */
        memcpy((char *)pktdata + offset, &rpkt.u, count);

        end = cdtime() + ctx->timeout / 2;

        if (seenlastfrag && offsets[0] == 0) {
            for (f = 1; f < numfrags; f++) {
                if (offsets[f-1] + counts[f-1] != offsets[f])
                    break;
            }
            if (f == numfrags) {
                *rsize = offsets[f-1] + counts[f-1];
                PLUGIN_DEBUG("%lu packets reassembled into response\n", (u_long)numfrags);
                return 0;
            }
        }
    }

    return -1;
}

static ssize_t ntpd_send_request(ntpd_ctx_t *ctx, int opcode, associd_t associd,
                                 const char *qdata, size_t qsize)
{
    if (qsize > CTL_MAX_DATA_LEN) {
        PLUGIN_ERROR("Packet data size too large: %zu > %d.", qsize, CTL_MAX_DATA_LEN);
        return -1;
    }

    struct ntp_control qpkt;
    qpkt.li_vn_mode = PKT_LI_VN_MODE(0, pktversion, MODE_CONTROL);
    qpkt.r_m_e_op = (u_char)(opcode & CTL_OP_MASK);
    qpkt.sequence = htons(ctx->sequence);
    qpkt.status = 0;
    qpkt.associd = htons((uint16_t)associd);
    qpkt.offset = 0;
    qpkt.count = htons((uint16_t)qsize);

    size_t pktsize = CTL_HEADER_LEN;

    /* If we have data, copy and pad it out to a 32-bit boundary. */
    if (qsize > 0) {
        memcpy(&qpkt.u, qdata, (size_t)qsize);
        pktsize += qsize;
        while (pktsize & (sizeof(uint32_t) - 1)) {
            qpkt.u.data[qsize++] = 0;
            pktsize++;
        }
    }

    ssize_t size = send(ctx->sd, &qpkt, pktsize, 0);
    if (size != (ssize_t)pktsize)
        return -1;

    return 0;
}

static int ntpd_query(ntpd_ctx_t *ctx, int opcode, associd_t associd,
                      const char *qdata, size_t qsize,
                      uint16_t *rstatus, char **rdata , size_t *rsize)
{
    ctx->sequence++;

    int status  = ntpd_send_request(ctx, opcode, associd, qdata, qsize);
    if (status != 0)
        return status;

    status = ntp_get_response(ctx, opcode, associd, rstatus, rdata, rsize);
    if (status != 0)
        return status;

    return 0;
}

static int ntpd_readvar(ntpd_ctx_t *ctx, associd_t associd, char *var,
                                         char data[DATASIZE], char **rvalue)
{
    size_t rsize = 0;
    uint16_t rstatus = 0;
    char qdata[CTL_MAX_DATA_LEN];

    strncpy(qdata, var, sizeof(qdata));
    size_t qdatalen = strlen(qdata);

    int status = ntpd_query(ctx, CTL_OP_READVAR, associd, qdata, qdatalen, &rstatus, &data, &rsize);
    if (status != 0)
        return -1;

    data[rsize] = '\0';

    kv_t kv[256] = {0};

    size_t kvlen = kv_parser_split(data, kv, 256);

    for (size_t i = 0; i < kvlen; i++) {
        if (strcmp(kv[i].key, var) == 0) {
            *rvalue = kv[i].value;
            return 0;
        }
    }

    return -1;
}

static size_t ntpd_readvars(ntpd_ctx_t *ctx, associd_t associd, char *vars,
                                             char data[DATASIZE], kv_t *kv, size_t kv_size)
{
    size_t rsize = 0;
    uint16_t rstatus = 0;
    char qdata[CTL_MAX_DATA_LEN];

    strncpy(qdata, vars, sizeof(qdata));
    size_t qdatalen = strlen(qdata);

    int status = ntpd_query(ctx, CTL_OP_READVAR, associd, qdata, qdatalen, &rstatus, &data, &rsize);
    if (status != 0)
        return 0;

    data[rsize] = '\0';

    return kv_parser_split(data, kv, kv_size);
}

static int ntpd_read_sys_vars(ntpd_ctx_t *ctx)
{
    char data[DATASIZE];

    if (ctx->sys_vars.nfrags == 0) {
        char *value = NULL;
        int status = ntpd_readvar(ctx, 0, "sys_var_list", data, &value);
        if (status != 0)
            return -1;

        if (value != NULL) {
            char *values[512];
            size_t values_size = str_comma_split(value, values, 512);
            for (size_t i = 0; i < values_size; i++) {
                char *val = values[i];
                size_t val_len = strlen(val);
                const struct ntpd_vars *ntpd_var = ntpd_vars_get_key(val, val_len);
                if (ntpd_var == NULL)
                    continue;
                data_frags_append(&ctx->sys_vars, val, val_len);
            }
        } else {
            return -1;
        }
    }

    size_t nvars = 0;
    for(size_t n = 0; n < ctx->sys_vars.nfrags; n++) {
        data_frag_t *dfrag =  &ctx->sys_vars.frags[n];

        kv_t kv[256] = {0};
        size_t len = ntpd_readvars(ctx, 0, dfrag->buffer, data, kv, 256);
        if (len == 0)
            return -1;

        for (size_t i = 0;  i < len ; i++) {
            const struct ntpd_vars *ntpd_var = ntpd_vars_get_key(kv[i].key, strlen(kv[i].key));
            if (ntpd_var == NULL)
                continue;

            nvars++;

            if (!(ctx->flags & ntpd_var->flags))
                continue;

            metric_family_t *fam = &ctx->fams[ntpd_var->fam];
            value_t value = {0};

            if (fam->type == METRIC_TYPE_GAUGE) {
                double raw = 0;
                if (parse_double(kv[i].value, &raw) != 0) {
                    PLUGIN_WARNING("Unable to parse var '%s' with value '%s'.",
                                   kv[i].key, kv[i].value);
                    continue;
                }
                if (ntpd_var->scale != 0.0)
                    raw = raw * ntpd_var->scale;
                value = VALUE_GAUGE(raw);
            } else if (fam->type == METRIC_TYPE_COUNTER) {
                if ((ntpd_var->fam == FAM_NTPD_NTS_KE_SERVES_GOOD_WALL) ||
                    (ntpd_var->fam == FAM_NTPD_NTS_KE_SERVES_GOOD_CPU) ||
                    (ntpd_var->fam == FAM_NTPD_NTS_KE_SERVES_NO_TLS_WALL) ||
                    (ntpd_var->fam == FAM_NTPD_NTS_KE_SERVES_NO_TLS_CPU) ||
                    (ntpd_var->fam == FAM_NTPD_NTS_KE_SERVES_BAD_WALL) ||
                    (ntpd_var->fam == FAM_NTPD_NTS_KE_SERVES_BAD_CPU)) {

                    double raw = 0;
                    if (parse_double(kv[i].value, &raw) != 0) {
                        PLUGIN_WARNING("Unable to parse var '%s' with value '%s'.",
                                       kv[i].key, kv[i].value);
                        continue;
                    }
                    if (ntpd_var->scale != 0.0)
                        raw = raw * ntpd_var->scale;

                    value = VALUE_COUNTER_FLOAT64(raw);
                } else {
                    if (strchr(kv[i].value, '.') != NULL) {
                        double raw = 0;
                        if (parse_double(kv[i].value, &raw) != 0) {
                            PLUGIN_WARNING("Unable to parse var '%s' with value '%s'.",
                                           kv[i].key, kv[i].value);
                            continue;
                        }
                        if (ntpd_var->scale != 0.0)
                            raw = raw * ntpd_var->scale;

                        uint64_t raw_int = raw;

                        if ((double)raw_int != raw)
                            value = VALUE_COUNTER_FLOAT64(raw);
                        else
                            value = VALUE_COUNTER(raw_int);
                    } else {
                        uint64_t raw = 0;
                        if (parse_uinteger(kv[i].value, &raw) != 0) {
                            PLUGIN_WARNING("Unable to parse var '%s' with value '%s'.",
                                           kv[i].key, kv[i].value);
                            continue;
                        }
                        value = VALUE_COUNTER(raw);
                    }
                }
            } else {
                continue;
            }

            metric_family_append(fam, value, &ctx->labels, NULL);
        }
    }

    if (ctx->sys_vars.nvars != nvars)
        data_frags_reset(&ctx->sys_vars);

    return 0;
}

static int ntpd_read_peer_vars(ntpd_ctx_t *ctx)
{
    ntp_stat_t ntp_stat_raw[DATASIZE / sizeof(ntp_stat_t)];
    char *data = (char *)ntp_stat_raw;

    size_t dsize;
    uint16_t rstatus;

    int status = ntpd_query(ctx, CTL_OP_READSTAT, 0, NULL, 0, &rstatus, &data, &dsize);
    if (status != 0)
        return -1;

    if (dsize == 0) {
        PLUGIN_DEBUG("No association ID's returned\n");
        return 0;
    }

    if (dsize & 0x3) {
        PLUGIN_ERROR("Server returned %zu octets, should be multiple of 4\n", dsize);
        return 0;
    }

    ntp_stat_t *ntp_stat = calloc(dsize, sizeof(ntp_stat_t));
    if (ntp_stat == NULL) {
        PLUGIN_ERROR("malloc failed");
        return -1;
    }

    size_t ntp_stat_size = dsize / sizeof(ntp_stat_t);
    for (size_t i = 0 ; i < ntp_stat_size; i++) {
        ntp_stat[i].assid = ntohs(ntp_stat_raw[i].assid);
        ntp_stat[i].status = ntohs(ntp_stat_raw[i].status);
    }

    for (size_t n = 0; n < ntp_stat_size; n++) {
        kv_t kv[5] = {0};
        size_t len = ntpd_readvars(ctx, ntp_stat[n].assid,
                                   "srcadr,stratum,delay,offset,jitter", data, kv, 5);
        if (len != 5)
            continue;

        enum {
            FOUND_SRCADR  = (1 << 0),
            FOUND_STRATUM = (1 << 1),
            FOUND_DELAY   = (1 << 2),
            FOUND_OFFSET  = (1 << 3),
            FOUND_JITTER  = (1 << 4)
        };

        char *srcadr = NULL;
        double stratum = 0.0;
        double delay = 0.0;
        double offset = 0.0;
        double jitter = 0.0;

        int found = 0;

        for (size_t i = 0;  i < len ; i++) {
            if (strcmp(kv[i].key, "srcadr") == 0) {
                srcadr = kv[i].value;
                found |= FOUND_SRCADR;
            } else if (strcmp(kv[i].key, "stratum") == 0) {
                if (parse_double(kv[i].value, &stratum) != 0) {
                    PLUGIN_WARNING("Unable to parse var '%s' with value '%s'.",
                                   kv[i].key, kv[i].value);
                    break;
                }
                found |= FOUND_STRATUM;
            } else if (strcmp(kv[i].key, "delay") == 0) {
                if (parse_double(kv[i].value, &delay) != 0) {
                    PLUGIN_WARNING("Unable to parse var '%s' with value '%s'.",
                                   kv[i].key, kv[i].value);
                    break;
                }
                delay *= 0.001;
                found |= FOUND_DELAY;
            } else if (strcmp(kv[i].key, "offset") == 0) {
                if (parse_double(kv[i].value, &offset) != 0) {
                    PLUGIN_WARNING("Unable to parse var '%s' with value '%s'.",
                                   kv[i].key, kv[i].value);
                    break;
                }
                offset *= 0.001;
                found |= FOUND_OFFSET;
            } else if (strcmp(kv[i].key, "jitter") == 0) {
                if (parse_double(kv[i].value, &jitter) != 0) {
                    PLUGIN_WARNING("Unable to parse var '%s' with value '%s'.",
                                   kv[i].key, kv[i].value);
                    break;
                }
                jitter *= 0.001;
                found |= FOUND_JITTER;
            } else {
                break;
            }
        }

        if (found != (FOUND_SRCADR|FOUND_STRATUM|FOUND_DELAY|FOUND_OFFSET|FOUND_JITTER))
            continue;

        if (strcmp(srcadr, "0.0.0.0") == 0)
            continue;

        metric_family_append(&ctx->fams[FAM_NTPD_PEER_STRATUM],
                             VALUE_GAUGE(stratum), &ctx->labels,
                             &(label_pair_const_t){.name="peer", .value=srcadr}, NULL);
        metric_family_append(&ctx->fams[FAM_NTPD_PEER_DISPERSION_SECONDS],
                             VALUE_GAUGE(jitter), &ctx->labels,
                             &(label_pair_const_t){.name="peer", .value=srcadr}, NULL);
        metric_family_append(&ctx->fams[FAM_NTPD_PEER_OFFSET_SECONDS],
                             VALUE_GAUGE(offset), &ctx->labels,
                             &(label_pair_const_t){.name="peer", .value=srcadr}, NULL);
        metric_family_append(&ctx->fams[FAM_NTPD_PEER_DELAY_SECONDS],
                             VALUE_GAUGE(delay), &ctx->labels,
                             &(label_pair_const_t){.name="peer", .value=srcadr}, NULL);
        metric_family_append(&ctx->fams[FAM_NTPD_PEER_STATUS],
                             VALUE_GAUGE(CTL_PEER_STATVAL(ntp_stat[n].status) & 0x7), &ctx->labels,
                             &(label_pair_const_t){.name="peer", .value=srcadr}, NULL);
    }

    free(ntp_stat);

    return 0;
}

static int ntpd_read(user_data_t *user_data)
{
    ntpd_ctx_t *ctx = user_data->data;

    if (ctx->sd < 0) {
        int port = atoi(ctx->port);
        ctx->sd = socket_connect_udp(ctx->host, port, 0);
        if (ctx->sd < 0) {
            metric_family_append(&ctx->fams[FAM_NTPD_UP], VALUE_GAUGE(0), &ctx->labels, NULL);
            plugin_dispatch_metric_family(&ctx->fams[FAM_NTPD_UP], 0);
            return 0;
        }
    }

    int status = ntpd_read_sys_vars(ctx);
    if (status != 0) {
        metric_family_append(&ctx->fams[FAM_NTPD_UP], VALUE_GAUGE(0), &ctx->labels, NULL);
        plugin_dispatch_metric_family(&ctx->fams[FAM_NTPD_UP], 0);
        return 0;
    }

    if (ctx->flags & COLLECT_PEERS)
        ntpd_read_peer_vars(ctx);

    metric_family_append(&ctx->fams[FAM_NTPD_UP], VALUE_GAUGE(1), &ctx->labels, NULL);

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

    data_frags_reset(&ctx->sys_vars);

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

    memcpy(ctx->fams, fams, sizeof(ctx->fams[0])*FAM_NTPD_MAX);

    int status = cf_util_get_string(ci, &ctx->name);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        free(ctx);
        return status;
    }

    ctx->flags = COLLECT_SYSINFO | COLLECT_PEERS;

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("host", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->host);
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_service(child, &ctx->port);
        } else if (strcasecmp("collect", child->key) == 0) {
            status = cf_util_get_flags(child, ntpd_flags, ntpd_flags_size, &ctx->flags);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &ctx->timeout);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &ctx->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
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

    if (ctx->timeout == 0)
        ctx->timeout = TIME_T_TO_CDTIME_T(2);

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
