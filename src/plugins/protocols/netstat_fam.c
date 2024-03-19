// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"
#include "plugins/protocols/netstat_fam.h"
#include "plugins/protocols/flags.h"

static metric_family_t fams[FAM_NETSTAT_MAX] = {
    [FAM_TCP_SYNCOOKIES_SENT] = {
        .name = "system_tcp_syncookies_sent",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many SYN cookies are sent.",
    },
    [FAM_TCP_SYNCOOKIES_RECV] = {
        .name = "system_tcp_syncookies_recv",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many reply packets of the SYN cookies the TCP stack receives.",
    },
    [FAM_TCP_SYNCOOKIES_FAILED] = {
        .name = "system_tcp_syncookies_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = "The MSS decoded from the SYN cookie is invalid.",
    },
    [FAM_TCP_EMBRYONIC_RSTS] = {
        .name = "system_tcp_embryonic_rsts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Resets received for a connection in the SYN_RECV state.",
    },
    [FAM_TCP_PRUNE_CALLED] = {
        .name = "system_tcp_prune_called",
        .type = METRIC_TYPE_COUNTER,
        .help = "Increased on attempt to reduce a socket allocated memory."
    },
    [FAM_TCP_RCV_PRUNED] = {
        .name = "system_tcp_rcv_pruned",
        .type = METRIC_TYPE_COUNTER,
        .help = "Increased when the tentative to reduce socket allocated memory failed, "
                "data is dropped.",
    },
    [FAM_TCP_OUT_OF_ORDER_PRUNED] = {
        .name = "system_tcp_out_of_order_pruned",
        .type = METRIC_TYPE_COUNTER,
        .help = "Increased on clean of the out-of-order queue of a struct tcp_soc.",
    },
    [FAM_TCP_OUT_OF_WINDOW_ICMPS] = {
        .name = "system_tcp_out_of_window_icmps",
        .type = METRIC_TYPE_COUNTER,
        .help = "Increased during an error detected in the state of a tcp/dccp connection.",
    },
    [FAM_TCP_LOCK_DROPPED_ICMPS] = {
        .name = "system_tcp_lock_dropped_icmps",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of ICMP packets that hit a locked (busy) socket and were dropped.",
    },
    [FAM_TCP_ARP_FILTER] = {
        .name = "system_tcp_arp_filter",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of Address Resolution Protocol messages not sent because "
                "they were meant for the recipient device.",
    },
    [FAM_TCP_TIMEWAIT] = {
        .name = "system_tcp_timewait",
        .type = METRIC_TYPE_COUNTER,
        .help = "TCP sockets finished time wait in fast timer.",
    },
    [FAM_TCP_TIMEWAIT_RECYCLED] = {
        .name = "system_tcp_timewait_recycled",
        .type = METRIC_TYPE_COUNTER,
        .help = "Time wait sockets recycled by timestamp.",
    },
    [FAM_TCP_TIMEWAIT_KILLED] = {
        .name = "system_tcp_timewait_killed",
        .type = METRIC_TYPE_COUNTER,
        .help = "TCP sockets finished timewait in slow timer.",
    },
    [FAM_TCP_PAWS_ACTIVE] = {
        .name = "system_tcp_paws_active",
        .type = METRIC_TYPE_GAUGE,
        .help = "Packets are dropped by PAWS (Protection Against Wrapped Sequence numbers) "
                "in Syn-Sent status."
    },
    [FAM_TCP_PAWS_ESTABLISHED] = {
        .name = "system_tcp_paws_established",
        .type = METRIC_TYPE_COUNTER,
        .help = "Packets are dropped by PAWS (Protection Against Wrapped Sequence numbers) "
                "in any status other than Syn-Sent."
    },
    [FAM_TCP_DELAYED_ACKS] = {
        .name = "system_tcp_delayed_acks",
        .type = METRIC_TYPE_COUNTER,
        .help = "A delayed ACK timer expires. "
                "The TCP stack will send a pure ACK packet and exit the delayed ACK mode."
    },
    [FAM_TCP_DELAYED_ACK_LOCKED] = {
        .name = "system_tcp_delayed_ack_locked",
        .type = METRIC_TYPE_COUNTER,
        .help = "A delayed ACK timer expires, but the TCP stack can’t send an ACK immediately "
                "due to the socket is locked by a userspace program."
    },
    [FAM_TCP_DELAYED_ACK_LOST] = {
        .name = "system_tcp_delayed_ack_lost",
        .type = METRIC_TYPE_COUNTER,
        .help = "It will be updated when the TCP stack receives a packet which has been ACKed.",
    },
    [FAM_TCP_LISTEN_OVERFLOWS] = {
        .name = "system_tcp_listen_overflows",
        .type = METRIC_TYPE_COUNTER,
        .help = "When kernel receives a SYN from a client and the TCP accept queue is full.",
    },
    [FAM_TCP_LISTEN_DROPS] = {
        .name = "system_tcp_listen_drops",
        .type = METRIC_TYPE_COUNTER,
        .help = "When kernel receives a SYN from a client and the TCP accept queue is full or "
                "when a TCP socket is in LISTEN state and kernel need to drop a packet.",
    },
    [FAM_TCP_HP_HITS] = {
        .name = "system_tcp_hp_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "If a TCP packet has data (which means it is not a pure ACK packet), "
                "and this packet is handled in the fast path.",
    },
    [FAM_TCP_PURE_ACKS] = {
        .name = "system_tcp_pure_acks",
        .type = METRIC_TYPE_COUNTER,
        .help = "If a packet set ACK flag and has no data, it is a pure ACK packet, "
                "and the kernel handles it in the slow path",
    },
    [FAM_TCP_HP_ACKS] = {
        .name = "system_tcp_hp_acks",
        .type = METRIC_TYPE_COUNTER,
        .help = "If a packet set ACK flag and has no data, it is a pure ACK packet, "
                "and kernel then handles it in the fast path",

    },
    [FAM_TCP_RENO_RECOVERY] = {
        .name = "system_tcp_reno_recovery",
        .type = METRIC_TYPE_COUNTER,
        .help = "When the congestion control comes into Recovery state, and SACK is not used. "
                "The TCP stack begins to retransmit the lost packets.",
    },
    [FAM_TCP_SACK_RECOVERY] = {
        .name = "system_tcp_sack_recovery",
        .type = METRIC_TYPE_COUNTER,
        .help = "When the congestion control comes into Recovery state, and SACK is used. "
                "The TCP stack begins to retransmit the lost packets.",
    },
    [FAM_TCP_SACK_RENEGING] = {
        .name = "system_tcp_sack_reneging",
        .type = METRIC_TYPE_COUNTER,
        .help = "A packet was acknowledged by SACK, but the receiver has dropped this packet, "
                "so the sender needs to retransmit this packet."
    },
    [FAM_TCP_SACK_REORDER] = {
        .name = "system_tcp_sack_reorder",
        .type = METRIC_TYPE_COUNTER,
        .help = "The reorder packet detected by SACK.",
    },
    [FAM_TCP_RENO_REORDER] = {
        .name = "system_tcp_reno_reorder",
        .type = METRIC_TYPE_COUNTER,
        .help = "The reorder packet is detected by fast recovery and SACK is disabled"
    },
    [FAM_TCP_TS_REORDER] = {
        .name = "system_tcp_ts_reorder",
        .type = METRIC_TYPE_COUNTER,
        .help = "The reorder packet is detected when a hole is filled.",
    },
    [FAM_TCP_FULL_UNDO] = {
        .name = "system_tcp_full_undo",
        .type = METRIC_TYPE_COUNTER,
        .help = "We detected some erroneous retransmits and undid our CWND reduction.",
    },
    [FAM_TCP_PARTIAL_UNDO] = {
        .name = "system_tcp_partial_undo",
        .type = METRIC_TYPE_COUNTER,
        .help = "We detected some erroneous retransmits, a partial ACK arrived while "
                "we were fast retransmitting, so we were able to partially undo some "
                "of our CWND reduction.",
    },
    [FAM_TCP_DACK_UNDO] = {
        .name = "system_tcp_dack_undo",
        .type = METRIC_TYPE_COUNTER,
        .help = "We detected some erroneous retransmits, a D-SACK arrived and ACK'ed all "
                "the retransmitted data, so we undid our CWND reduction."
    },
    [FAM_TCP_LOST_UNDO] = {
        .name = "system_tcp_lost_undo",
        .type = METRIC_TYPE_COUNTER,
        .help = "We detected some erroneous retransmits, a partial ACK arrived, "
                "so we undid our CWND reduction.",
    },
    [FAM_TCP_LOST_RETRANSMIT] = {
        .name = "system_tcp_lost_retransmit",
        .type = METRIC_TYPE_COUNTER,
        .help = "A SACK points out that a retransmission packet is lost again."
    },
    [FAM_TCP_RENO_FAILURES] = {
        .name = "system_tcp_reno_failures",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_TCP_SACK_FAILURES] = {
        .name = "system_tcp_sack_failures",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_TCP_LOSS_FAILURES] = {
        .name = "system_tcp_loss_failures",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_TCP_FAST_RETRANS] = {
        .name = "system_tcp_fast_retrans",
        .type = METRIC_TYPE_COUNTER,
        .help = "The TCP stack wants to retransmit a packet and "
                "the congestion control state is not 'Loss'."
    },
    [FAM_TCP_SLOW_START_RETRANS] = {
        .name = "system_tcp_slow_start_retrans",
        .type = METRIC_TYPE_COUNTER,
        .help = "The TCP stack wants to retransmit a packet and "
                "the congestion control state is 'Loss'."
    },
    [FAM_TCP_TIMEOUTS] = {
        .name = "system_tcp_timeouts",
        .type = METRIC_TYPE_COUNTER,
        .help = "TCP timeout events.",
    },
    [FAM_TCP_LOSS_PROBES] = {
        .name = "system_tcp_loss_probes",
        .type = METRIC_TYPE_COUNTER,
        .help = "A TLP (Tail Loss Probe) probe packet is sent.",
    },
    [FAM_TCP_LOSS_PROBE_RECOVERY] = {
        .name = "system_tcp_loss_probe_recovery",
        .type = METRIC_TYPE_COUNTER,
        .help = "A packet loss is detected and recovered by TLP (Tail Loss Probe)."
    },
    [FAM_TCP_RENO_RECOVERY_FAIL] = {
        .name = "system_tcp_reno_recovery_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times that the TCP fast recovery algorithm failed "
                "to recover from a packet loss."
    },
    [FAM_TCP_SACK_RECOVERY_FAIL] = {
        .name = "system_tcp_sack_recovery_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times that the device failed to recover from a SACK packet loss.",
    },
    [FAM_TCP_RCV_COLLAPSED] = {
        .name = "system_tcp_rcv_collapsed",
        .type = METRIC_TYPE_COUNTER,
        .help = "This counter indicates how many skbs are freed during 'collapse'",
    },
    [FAM_TCP_BACKLOG_COALESCE] = {
        .name = "system_tcp_backlog_coalesce",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_TCP_DSACK_OLD_SENT] = {
        .name = "system_tcp_dsack_old_sent",
        .type = METRIC_TYPE_COUNTER,
        .help = "The TCP stack receives a duplicate packet which has been acked, "
                "so it sends a DSACK to the sender."
    },
    [FAM_TCP_DSACK_OUT_OF_ORDER_SENT] = {
        .name = "system_tcp_dsack_out_of_order_sent",
        .type = METRIC_TYPE_COUNTER,
        .help = "The TCP stack receives an out of order duplicate packet, "
                "so it sends a DSACK to the sender."
    },
    [FAM_TCP_DSACK_RECV] = {
        .name = "system_tcp_dsack_recv",
        .type = METRIC_TYPE_COUNTER,
        .help = "The TCP stack receives a DSACK, which indicates an acknowledged "
                "duplicate packet is received."
    },
    [FAM_TCP_DSACK_OUT_OF_ORDER_RECV] = {
        .name = "system_tcp_dsack_out_of_order_recv",
        .type = METRIC_TYPE_COUNTER,
        .help = "The TCP stack receives a DSACK, which indicate an out of order "
                "duplicate packet is received."
    },
    [FAM_TCP_ABORT_ON_DATA] = {
        .name = "system_tcp_abort_on_data",
        .type = METRIC_TYPE_COUNTER,
        .help = "It means TCP layer has data in flight, but need to close the connection. "
                "So TCP layer sends a RST to the other sided."
    },
    [FAM_TCP_ABORT_ON_CLOSE] = {
        .name = "system_tcp_abort_on_close",
        .type = METRIC_TYPE_COUNTER,
        .help = "This counter means the application has unread data in the TCP layer "
                "when the application wants to close the TCP connection."
    },
    [FAM_TCP_ABORT_ON_MEMORY] = {
        .name = "system_tcp_abort_on_memory",
        .type = METRIC_TYPE_COUNTER,
        .help = "It happens when there are too many orphaned sockets (not attached a FD) "
                "and the kernel has to drop a connection."
    },
    [FAM_TCP_ABORT_ON_TIMEOUT] = {
        .name = "system_tcp_abort_on_timeout",
        .type = METRIC_TYPE_COUNTER,
        .help = "This counter will increase when any of the TCP timers expire.",
    },
    [FAM_TCP_ABORT_ON_LINGER] = {
        .name = "system_tcp_abort_on_linger",
        .type = METRIC_TYPE_COUNTER,
        .help = "When a TCP connection comes into FIN_WAIT_2 state, instead of "
                "waiting for the fin packet from the other side, kernel could send "
                "a RST and delete the socket immediately.",
    },
    [FAM_TCP_ABORT_FAILED] = {
        .name = "system_tcp_abort_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = "The kernel TCP layer will send RST if the RFC2525 2.17 section is satisfied. "
                "If an internal error occurs during this process, this counter will be increased.",
    },
    [FAM_TCP_MEMORY_PRESSURES] = {
        .name = "system_tcp_memory_pressures",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count number of times that the sysctl tcp_mem limits was hit.",
    },
    [FAM_TCP_MEMORY_PRESSURES_MSECONDS] = {
        .name = "system_tcp_memory_pressures_mseconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Cumulative duration of memory pressure events, given in ms units.",
    },
    [FAM_TCP_SACK_DISCARD] = {
        .name = "system_tcp_sack_discard",
        .type = METRIC_TYPE_COUNTER,
        .help = "This counter indicates how many SACK blocks are invalid.",
    },
    [FAM_TCP_DSACK_IGNORED_OLD] = {
        .name = "system_tcp_dsack_ignored_old",
        .type = METRIC_TYPE_COUNTER,
        .help = "When a DSACK block is invalid and the undo_marker in the TCP socket is set.",
    },
    [FAM_TCP_DSACK_IGNORED_NO_UNDO] = {
        .name = "system_tcp_dsack_ignored_no_undo",
        .type = METRIC_TYPE_COUNTER,
        .help = "When a DSACK block is invalid and the undo_marker in the TCP socket is not set.",
    },
    [FAM_TCP_SPURIOUS_RTO] = {
        .name = "system_tcp_spurious_rto",
        .type = METRIC_TYPE_COUNTER,
        .help = "The spurious retransmission timeout detected by the F-RTO algorithm."
    },
    [FAM_TCP_MD5_NOT_FOUND] = {
        .name = "system_tcp_md5_not_found",
        .type = METRIC_TYPE_COUNTER,
        .help = "Increased when the MD5 tcp option is missing."
    },
    [FAM_TCP_MD5_UNEXPECTED] = {
        .name = "system_tcp_md5_unexpected",
        .type = METRIC_TYPE_COUNTER,
        .help = "Increased when the MD5 tcp option is not the one expected.",
    },
    [FAM_TCP_MD5_FAILURE] = {
        .name = "system_tcp_md5_failure",
        .type = METRIC_TYPE_COUNTER,
        .help = "Increased when the computed MD5 checksum does not match the one expected.",
    },
    [FAM_TCP_SACK_SHIFTED] = {
        .name = "system_tcp_sack_shifted",
        .type = METRIC_TYPE_COUNTER,
        .help = "A sk_buff struct skb is shifted because "
                "a SACK block acrosses multiple sk_buff struct.",
    },
    [FAM_TCP_SACK_MERGED] = {
        .name = "system_tcp_sack_merged",
        .type = METRIC_TYPE_COUNTER,
        .help = "A sk_buff struct is merged because "
                "a SACK block acrosses multiple sk_buff struct.",
    },
    [FAM_TCP_SACK_SHIFT_FALLBACK] = {
        .name = "system_tcp_sack_shift_fallback",
        .type = METRIC_TYPE_COUNTER,
        .help = "A sk_buff struct should be shifted or merged because "
                "a SACK block acrosses multiple sk_buff struct, "
                "but the TCP stack doesn’t do it for some reasons.",
    },
    [FAM_TCP_BACKLOG_DROP] = {
        .name = "system_tcp_backlog_drop",
        .type = METRIC_TYPE_COUNTER,
        .help = "We received something but had to drop it because "
                "the socket's receive queue was full."
    },
    [FAM_TCP_PF_MEM_ALLOC_DROP] = {
        .name = "system_tcp_pf_mem_alloc_drop",
        .type = METRIC_TYPE_COUNTER,
        .help = "TCP packets were getting dropped by sk_filter_trim_cap() due to "
                "returning -ENOMEM. This is due to memory fragmentation causing "
                "allocations to fail."
    },
    [FAM_TCP_MIN_TTL_DROP] = {
        .name = "system_tcp_min_ttl_drop",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_TCP_DEFER_ACCEPT_DROP] = {
        .name = "system_tcp_defer_accept_drop",
        .type = METRIC_TYPE_COUNTER,
        .help = "If an application has set TCP_DEFER_ACCEPT on its listening socket, "
                "and the ACK carries no data, then the ACK packet is dropped and "
                "this counter is incremented."
    },
    [FAM_TCP_IP_REVERSE_PATH_FILTER] = {
        .name = "system_tcp_ip_reverse_path_filter",
        .type = METRIC_TYPE_COUNTER,
        .help = "Packets dropped by rp_filter."
    },
    [FAM_TCP_TIME_WAIT_OVERFLOW] = {
        .name = "system_tcp_time_wait_overflow",
        .type = METRIC_TYPE_COUNTER,
        .help = "Time wait bucket table overflow occurs.",
    },
    [FAM_TCP_REQ_QFULL_DO_COOKIES] = {
        .name = "system_tcp_req_qfull_do_cookies",
        .type = METRIC_TYPE_COUNTER,
        .help = "This counter is incremented when the backlog overflows and SYN cookies are sent.",
    },
    [FAM_TCP_REQ_QFULL_DROP] = {
        .name = "system_tcp_req_qfull_drop",
        .type = METRIC_TYPE_COUNTER,
        .help = "This counter is incremented when SYNs were dropped because SYN cookies "
                "were disabled and the SYN backlog was full.",
    },
    [FAM_TCP_RETRANS_FAIL] = {
        .name = "system_tcp_retrans_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "The TCP stack tries to deliver a retransmission packet to lower layers "
                "but the lower layers return an error."
    },
    [FAM_TCP_RCV_COALESCE] = {
        .name = "system_tcp_rcv_coalesce",
        .type = METRIC_TYPE_COUNTER,
        .help = "When packets are received by the TCP layer and are not be read by "
                "the application, the TCP layer will try to merge them. "
                "This counter indicate how many packets are merged in such situation."
    },
    [FAM_TCP_OUT_OF_ORDER_QUEUE] = {
        .name = "system_tcp_out_of_order_queue",
        .type = METRIC_TYPE_COUNTER,
        .help = "The TCP layer receives an out of order packet and has enough memory to queue it."
    },
    [FAM_TCP_OUT_OF_ORDER_DROP] = {
        .name = "system_tcp_out_of_order_drop",
        .type = METRIC_TYPE_COUNTER,
        .help = "The TCP layer receives an out of order packet but doesn’t have enough memory, "
                "so drops it."
    },
    [FAM_TCP_OUT_OF_ORDER_MERGE] = {
        .name = "system_tcp_out_of_order_merge",
        .type = METRIC_TYPE_COUNTER,
        .help = "The received out of order packet has an overlay with the previous packet. "
                "The overlay part will be dropped.",
    },
    [FAM_TCP_CHALLENGE_ACK] = {
        .name = "system_tcp_challenge_ack",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of challenge acks sent.",
    },
    [FAM_TCP_SYN_CHALLENGE] = {
        .name = "system_tcp_syn_challenge",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of challenge acks sent in response to SYN packets.",
    },
    [FAM_TCP_FAST_OPEN_ACTIVE] = {
        .name = "system_tcp_fast_open_active",
        .type = METRIC_TYPE_COUNTER,
        .help = "When the TCP stack receives an ACK packet in the SYN-SENT status, "
                "and the ACK packet acknowledges the data in the SYN packet, "
                "the TCP stack understand the TFO cookie is accepted by the other side, "
                "then it updates this counter.",
    },
    [FAM_TCP_FAST_OPEN_ACTIVE_FAIL] = {
        .name = "system_tcp_fast_open_active_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "This counter indicates that the TCP stack initiated a TCP Fast Open, "
                "but it failed.",
    },
    [FAM_TCP_FAST_OPEN_PASSIVE] = {
        .name = "system_tcp_fast_open_passive",
        .type = METRIC_TYPE_COUNTER,
        .help = "This counter indicates how many times the TCP stack accepts "
                "the fast open request.",
    },
    [FAM_TCP_FAST_OPEN_PASSIVE_FAIL] = {
        .name = "system_tcp_fast_open_passive_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "This counter indicates how many times the TCP stack rejects "
                "the fast open request."
    },
    [FAM_TCP_FAST_OPEN_LISTEN_OVERFLOW] = {
        .name = "system_tcp_fast_open_listen_overflow",
        .type = METRIC_TYPE_COUNTER,
        .help = "When the pending fast open request number is larger than "
                "fastopenq->max_qlen, the TCP stack will reject the fast open request "
                "and update this counter.",
    },
    [FAM_TCP_FAST_OPEN_COOKIE_REQUESTED] = {
        .name = "system_tcp_fast_open_cookie_requested",
        .type = METRIC_TYPE_COUNTER,
        .help = "This counter indicates how many times a client wants to request a TFO cookie.",
    },
    [FAM_TCP_FAST_OPEN_BLACK_HOLE] = {
        .name = "system_tcp_fast_open_black_hole",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
#if 0
Initial time period in second to disable Fastopen on active TCP sockets when a TFO firewall blackhole issue happens. This time period will grow exponentially when more blackhole issues get detected right after Fastopen is re-enabled and will reset to initial value when the blackhole issue goes away. 0 to disable the blackhole detection. By default, it is set to 1hr.
#endif
    },

    [FAM_TCP_SPURIOUS_RTX_HOST_QUEUES] = {
        .name = "system_tcp_spurious_rtx_host_queues",
        .type = METRIC_TYPE_COUNTER,
        .help = "When the TCP stack wants to retransmit a packet, and finds that packet "
                "is not lost in the network, but the packet is not sent yet, the TCP stack "
                "would give up the retransmission and update this counter.",
    },
    [FAM_TCP_BUSY_POLL_RX_PKTS] = {
        .name = "system_tcp_busy_poll_rx_pkts",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_TCP_AUTO_CORKING] = {
        .name = "system_tcp_auto_corking",
        .type = METRIC_TYPE_COUNTER,
        .help = "When sending packets, the TCP layer will try to merge small packets to a "
                "bigger one. This counter increase 1 for every packet merged in such situation."
    },
    [FAM_TCP_FROM_ZERO_WINDOW_ADV] = {
        .name = "system_tcp_from_zero_window_adv",
        .type = METRIC_TYPE_COUNTER,
        .help = "The TCP receive window is set to no-zero value from zero.",
    },
    [FAM_TCP_TO_ZERO_WINDOW_ADV] = {
        .name = "system_tcp_to_zero_window_adv",
        .type = METRIC_TYPE_COUNTER,
        .help = "The TCP receive window is set to zero from a no-zero value.",
    },
    [FAM_TCP_WANT_ZERO_WINDOW_ADV] = {
        .name = "system_tcp_want_zero_window_adv",
        .type = METRIC_TYPE_COUNTER,
        .help = "Depending on current memory usage, the TCP stack tries to set receive window "
                "to zero. But the receive window might still be a no-zero value.",
    },
    [FAM_TCP_SYN_RETRANS] = {
        .name = "system_tcp_syn_retrans",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of SYN and SYN/ACK retransmits to break down retransmissions into "
                "SYN, fast-retransmits, timeout retransmits, etc.",
    },
    [FAM_TCP_ORIG_DATA_SENT] = {
        .name = "system_tcp_orig_data_sent",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of outgoing packets with original data excluding "
                "retransmission but including data-in-SYN.",
    },
    [FAM_TCP_HYSTART_TRAIN_DETECT] = {
        .name = "system_tcp_hystart_train_detect",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many times the ACK train length threshold is detected",
    },
    [FAM_TCP_HYSTART_TRAIN_CWND] = {
        .name = "system_tcp_hystart_train_cwnd",
        .type = METRIC_TYPE_COUNTER,
        .help = "The sum of CWND detected by ACK train length.",
    },
    [FAM_TCP_HYSTART_DELAY_DETECT] = {
        .name = "system_tcp_hystart_delay_detect",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many times the packet delay threshold is detected.",
    },
    [FAM_TCP_HYSTART_DELAY_CWND] = {
        .name = "system_tcp_hystart_delay_cwnd",
        .type = METRIC_TYPE_COUNTER,
        .help = "The sum of CWND detected by packet delay.",
    },
    [FAM_TCP_ACK_SKIPPED_SYN_RECV] = {
        .name = "system_tcp_ack_skipped_syn_recv",
        .type = METRIC_TYPE_COUNTER,
        .help = "The ACK is skipped in Syn-Recv status.",
    },
    [FAM_TCP_ACK_SKIPPED_PAWD] = {
        .name = "system_tcp_ack_skipped_pawd",
        .type = METRIC_TYPE_COUNTER,
        .help = "The ACK is skipped due to PAWS (Protect Against Wrapped Sequence numbers) "
                "check fails.",
    },
    [FAM_TCP_ACK_SKIPPED_SEQ] = {
        .name = "system_tcp_ack_skipped_seq",
        .type = METRIC_TYPE_COUNTER,
        .help = "The sequence number is out of window and the timestamp passes the PAWS "
                "check and the TCP status is not Syn-Recv, Fin-Wait-2, and Time-Wait."
    },
    [FAM_TCP_ACK_SKIPPED_FIN_WAIT_2] = {
        .name = "system_tcp_ack_skipped_fin_wait_2",
        .type = METRIC_TYPE_COUNTER,
        .help = "The ACK is skipped in Fin-Wait-2 status, the reason would be either "
                "PAWS check fails or the received sequence number is out of window."
    },
    [FAM_TCP_ACK_SKIPPED_TIME_WAIT] = {
        .name = "system_tcp_ack_skipped_time_wait",
        .type = METRIC_TYPE_COUNTER,
        .help = "The ACK is skipped in Time-Wait status, the reason would be either "
                "PAWS check failed or the received sequence number is out of window."
    },
    [FAM_TCP_ACK_SKIPPED_CHALLENGE] = {
        .name = "system_tcp_ack_skipped_challenge",
        .type = METRIC_TYPE_COUNTER,
        .help = "The ACK is skipped if the ACK is a challenge ACK.",
    },
    [FAM_TCP_WIN_PROBLE] = {
        .name = "system_tcp_win_proble",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL
    },
    [FAM_TCP_KEEPALIVE] = {
        .name = "system_tcp_keepalive",
        .type = METRIC_TYPE_COUNTER,
        .help = "This counter indicates how many keepalive packets were sent.",
    },

    [FAM_TCP_MTUP_FAIL]= {
        .name = "system_tcp_mtup_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_TCP_MTUP_SUCCESS] = {
        .name = "system_tcp_mtup_success",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_TCP_DELIVERED] = {
        .name = "system_tcp_delivered",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_TCP_DELIVERED_CE] = {
        .name = "system_tcp_delivered_ce",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_TCP_ACK_COMPRESSED] = {
        .name = "system_tcp_ack_compressed",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_TCP_ZERO_WINDOW_DROP] = {
        .name = "system_tcp_zero_window_drop",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_TCP_RCV_QDROP] = {
        .name = "system_tcp_rcv_qdrop",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_TCP_WQUEUE_TOO_BIG] = {
        .name = "system_tcp_wqueue_too_big",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_TCP_FAST_OPEN_PASSIVE_ALT_KEY] = {
        .name = "system_tcp_fast_open_passive_alt_key",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_TCP_TIMEOUT_REHASH] = {
        .name = "system_tcp_timeout_rehash",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_TCP_DUPLICATE_DATA_REHASH] = {
        .name = "system_tcp_duplicate_data_rehash",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_TCP_DSACK_RECV_SEGS] = {
        .name = "system_tcp_dsack_recv_segs",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_TCP_SDACK_IGNORED_DUBIOUS] = {
        .name = "system_tcp_sdack_ignored_dubious",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_TCP_MIGRATE_REQ_SUCCESS] = {
        .name = "system_tcp_migrate_req_success",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_TCP_MIGRATE_REQ_FAILURE] = {
        .name = "system_tcp_migrate_req_failure",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_TCP_PLB_REHASH] = {
        .name = "system_tcp_plb_rehash",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },


    [FAM_IP_NO_ROUTES] = {
        .name = "system_ip_no_routes",
        .type = METRIC_TYPE_COUNTER,
        .help = "This counter means the packet is dropped when the IP stack receives a packet "
                "and can’t find a route for it from the route table.",
    },
    [FAM_IP_TRUNCATED_PKTS] = {
        .name = "system_ip_truncated_pkts",
        .type = METRIC_TYPE_COUNTER,
        .help = "For IPv4 packet, it means the actual data size is smaller "
                "than the “Total Length” field in the IPv4 header."
    },
    [FAM_IP_MCAST_PKTS] = {
        .name = "system_ip_mcast_pkts",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_OUT_MCAST_PKTS] = {
        .name = "system_ip_out_mcast_pkts",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_IN_BCAST_PKTS] = {
        .name = "system_ip_in_bcast_pkts",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_OUT_BCAST_PKTS] = {
        .name = "system_ip_out_bcast_pkts",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_IN_BYTES] = {
        .name = "system_ip_in_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_OUT_BYTES] = {
        .name = "system_ip_out_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_IN_MCAST_BYTES] = {
        .name = "system_ip_in_mcast_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_OUT_MCAST_BYTES] = {
        .name = "system_ip_out_mcast_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_IN_BCAST_BYTES] = {
        .name = "system_ip_in_bcast_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_OUT_BCAST_BYTES] = {
        .name = "system_ip_out_bcast_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_IN_CSUM_ERRORS] = {
        .name = "system_ip_in_csum_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_IN_NO_ECTP_PKTS] = {
        .name = "system_ip_in_no_ectp_pkts",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_IN_ECT1_PKTS] = {
        .name = "system_ip_in_ect1_pkts",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_IN_ECT0_PKTS] = {
        .name = "system_ip_in_ect0_pkts",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_IN_CE_PKTS] = {
        .name = "system_ip_in_ce_pkts",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_REASM_OVERLAPS] = {
        .name = "system_ip_reasm_overlaps",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
#if 0
They indicate the number of four kinds of ECN IP packets, please refer Explicit Congestion Notification for more details.
These 4 counters calculate how many packets received per ECN status. They count the real frame number regardless the LRO/GRO. So for the same packet, you might find that IpInReceives count 1, but IpExtInNoECTPkts counts 2 or more.
#endif
    [FAM_MPTCP_MP_CAPABLE_SYN_RX] = {
        .name = "system_mptcp_mp_capable_syn_rx",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received SYN with MP_CAPABLE.",
    },
    [FAM_MPTCP_MP_CAPABLE_SYN_TX] = {
        .name = "system_mptcp_mp_capable_syn_tx",
        .type = METRIC_TYPE_COUNTER,
        .help = "Sent SYN with MP_CAPABLE.",
    },
    [FAM_MPTCP_MP_CAPABLE_SYNC_ACK_RX] = {
        .name = "system_mptcp_mp_capable_sync_ack_rx",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received SYN/ACK with MP_CAPABLE.",
    },
    [FAM_MPTCP_MP_CAPABLE_ACK_RX] = {
        .name = "system_mptcp_mp_capable_ack_rx",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received third ACK with MP_CAPABLE.",
    },
    [FAM_MPTCP_MP_CAPABLE_FALLBACK_ACK] = {
        .name = "system_mptcp_mp_capable_fallback_ack",
        .type = METRIC_TYPE_COUNTER,
        .help = "Server-side fallback during 3-way handshake.",
    },
    [FAM_MPTCP_MP_CAPABLE_FALLBACK_SYN_ACK] = {
        .name = "system_mptcp_mp_capable_fallback_syn_ack",
        .type = METRIC_TYPE_COUNTER,
        .help = "Client-side fallback during 3-way handshake.",
    },
    [FAM_MPTCP_MP_FALLBACK_TOKEN_INIT] = {
        .name = "system_mptcp_mp_fallback_token_init",
        .type = METRIC_TYPE_COUNTER,
        .help = "Could not init/allocate token.",
    },
    [FAM_MPTCP_RETRANS] = {
        .name = "system_mptcp_retrans",
        .type = METRIC_TYPE_COUNTER,
        .help = "Segments retransmitted at the MPTCP-level.",
    },
    [FAM_MPTCP_MP_JOIN_NO_TOKEN_FOUND] = {
        .name = "system_mptcp_mp_join_no_token_found",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received MP_JOIN but the token was not found.",
    },
    [FAM_MPTCP_MP_JOIN_SYNC_RX] = {
        .name = "system_mptcp_mp_join_sync_rx",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received a SYN + MP_JOIN.",
    },
    [FAM_MPTCP_MP_JOIN_SYN_ACK_RX] = {
        .name = "system_mptcp_mp_join_syn_ack_rx",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received a SYN/ACK + MP_JOIN.",
    },
    [FAM_MPTCP_MP_JOIN_SYN_ACK_HMAC_FAILURE] = {
        .name = "system_mptcp_mp_join_syn_ack_hmac_failure",
        .type = METRIC_TYPE_COUNTER,
        .help = "HMAC was wrong on SYN/ACK + MP_JOIN.",
    },
    [FAM_MPTCP_MP_JOIN_ACK_RX] = {
        .name = "system_mptcp_mp_join_ack_rx",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received an ACK + MP_JOIN.",
    },
    [FAM_MPTCP_JOIN_ACK_HMAC_FAILURE] = {
        .name = "system_mptcp_join_ack_hmac_failure",
        .type = METRIC_TYPE_COUNTER,
        .help = "HMAC was wrong on ACK + MP_JOIN.",
    },
    [FAM_MPTCP_DSS_NOT_MATCHING] = {
        .name = "system_mptcp_dss_not_matching",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received a new mapping that did not match the previous one.",
    },
    [FAM_MPTCP_INFINITE_MAX_TX] = {
        .name = "system_mptcp_infinite_max_tx",
        .type = METRIC_TYPE_COUNTER,
        .help = "Sent an infinite mapping.",
    },
    [FAM_MPTCP_INFINITE_MAP_RX] = {
        .name = "system_mptcp_infinite_map_rx",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received an infinite mapping.",
    },
    [FAM_MPTCP_DSS_NO_MATCH_TCP] = {
        .name = "system_mptcp_dss_no_match_tcp",
        .type = METRIC_TYPE_COUNTER,
        .help = "DSS-mapping did not map with TCP's sequence numbers.",
    },
    [FAM_MPTCP_DATA_CSUM_ERR] = {
        .name = "system_mptcp_data_csum_err",
        .type = METRIC_TYPE_COUNTER,
        .help = "The data checksum fail.",
    },
    [FAM_MPTCP_OFO_QUEUE_TAIL] = {
        .name = "system_mptcp_ofo_queue_tail",
        .type = METRIC_TYPE_COUNTER,
        .help = "Segments inserted into OoO queue tail.",
    },
    [FAM_MPTCP_OFO_QUEUE] = {
        .name = "system_mptcp_ofo_queue",
        .type = METRIC_TYPE_COUNTER,
        .help = "Segments inserted into OoO queue.",
    },
    [FAM_MPTCP_OFO_MERGER] = {
        .name = "system_mptcp_ofo_merger",
        .type = METRIC_TYPE_COUNTER,
        .help = "Segments merged in OoO queue.",
    },
    [FAM_MPTCP_NO_DSS_IN_WINDOW] = {
        .name = "system_mptcp_no_dss_in_window",
        .type = METRIC_TYPE_COUNTER,
        .help = "Segments not in MPTCP windows.",
    },
    [FAM_MPTCP_DUPLICATE_DATA] = {
        .name = "system_mptcp_duplicate_data",
        .type = METRIC_TYPE_COUNTER,
        .help = "Segments discarded due to duplicate DSS.",
    },
    [FAM_MPTCP_ADD_ADDR] = {
        .name = "system_mptcp_add_addr",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received ADD_ADDR with echo-flag=0.",
    },
    [FAM_MPTCP_ECHO_ADD] = {
        .name = "system_mptcp_echo_add",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received ADD_ADDR with echo-flag=1.",
    },
    [FAM_MPTCP_PORT_ADD] = {
        .name = "system_mptcp_port_add",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received ADD_ADDR with a port-number.",
    },
    [FAM_MPTCP_ADD_ADDR_DROP] = {
        .name = "system_mptcp_add_addr_drop",
        .type = METRIC_TYPE_COUNTER,
        .help = "Dropped incoming ADD_ADDR.",
    },
    [FAM_MPTCP_MP_JOIN_PORT_SYN_RX] = {
        .name = "system_mptcp_mp_join_port_syn_rx",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received a SYN MP_JOIN with a different port-number.",
    },
    [FAM_MPTCP_MP_JOIN_PORT_SYN_ACK_RX] = {
        .name = "system_mptcp_mp_join_port_syn_ack_rx",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received a SYNACK MP_JOIN with a different port-number.",
    },
    [FAM_MPTCP_MP_JOIN_PORT_ACK_RX] = {
        .name = "system_mptcp_mp_join_port_ack_rx",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received an ACK MP_JOIN with a different port-number.",
    },
    [FAM_MPTCP_MISMATCH_PORT_SYN_RX] = {
        .name = "system_mptcp_mismatch_port_syn_rx",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received a SYN MP_JOIN with a mismatched port-number.",
    },
    [FAM_MPTCP_MISMATCH_PORT_ACK_RX] = {
        .name = "system_mptcp_mismatch_port_ack_rx",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received an ACK MP_JOIN with a mismatched port-number.",
    },
    [FAM_MPTCP_RM_ADDR] = {
        .name = "system_mptcp_rm_addr",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received RM_ADDR.",
    },
    [FAM_MPTCP_RM_ADDR_DROP] = {
        .name = "system_mptcp_rm_addr_drop",
        .type = METRIC_TYPE_COUNTER,
        .help = "Dropped incoming RM_ADDR.",
    },
    [FAM_MPTCP_RM_SUBFLOW] = {
        .name = "system_mptcp_rm_subflow",
        .type = METRIC_TYPE_COUNTER,
        .help = "Remove a subflow.",
    },
    [FAM_MPTCP_MP_PRIO_TX] = {
        .name = "system_mptcp_mp_prio_tx",
        .type = METRIC_TYPE_COUNTER,
        .help = "Transmit a MP_PRIO.",
    },
    [FAM_MPTCP_MP_PRIO_RX] = {
        .name = "system_mptcp_mp_prio_rx",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received a MP_PRIO.",
    },
    [FAM_MPTCP_MP_FAIL_TX] = {
        .name = "system_mptcp_mp_fail_tx",
        .type = METRIC_TYPE_COUNTER,
        .help = "Transmit a MP_FAIL.",
    },
    [FAM_MPTCP_MP_FAIL_RX] = {
        .name = "system_mptcp_mp_fail_rx",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received a MP_FAIL.",
    },
    [FAM_MPTCP_MP_FAST_CLOSE_TX] = {
        .name = "system_mptcp_mp_fast_close_tx",
        .type = METRIC_TYPE_COUNTER,
        .help = "Transmit a MP_FASTCLOSE.",
    },
    [FAM_MPTCP_MP_FAST_CLOSE_RX] = {
        .name = "system_mptcp_mp_fast_close_rx",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received a MP_FASTCLOSE.",
    },
    [FAM_MPTCP_MP_RST_TX] = {
        .name = "system_mptcp_mp_rst_tx",
        .type = METRIC_TYPE_COUNTER,
        .help = "Transmit a MP_RST.",
    },
    [FAM_MPTCP_MP_RST_RX] = {
        .name = "system_mptcp_mp_rst_rx",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received a MP_RST.",
    },
    [FAM_MPTCP_RCV_PRUNED] = {
        .name = "system_mptcp_rcv_pruned",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incoming packet dropped due to memory limit.",
    },
    [FAM_MPTCP_SUBFLOW_STALE] = {
        .name = "system_mptcp_subflow_stale",
        .type = METRIC_TYPE_COUNTER,
        .help = "Subflows entered 'stale' status.",
    },
    [FAM_MPTCP_SUBFLOW_RECOVER] = {
        .name = "system_mptcp_subflow_recover",
        .type = METRIC_TYPE_COUNTER,
        .help = "Subflows returned to active status after being stale.",
    },
    [FAM_MPTCP_SND_WND_SHARED] = {
        .name = "system_mptcp_snd_wnd_shared",
        .type = METRIC_TYPE_COUNTER,
        .help = "Subflow snd wnd is overridden by msk's one.",
    },
    [FAM_MPTCP_RCV_WND_SHARED] = {
        .name = "system_mptcp_rcv_wnd_shared",
        .type = METRIC_TYPE_COUNTER,
        .help = "Subflow rcv wnd is overridden by msk's one.",
    },
    [FAM_MPTCP_RCV_WND_CONFLICT_UPDATE] = {
        .name = "system_mptcp_rcv_wnd_conflict_update",
        .type = METRIC_TYPE_COUNTER,
        .help = "Subflow rcv wnd is overridden by msk's one due to conflict "
                "with another subflow while updating msk rcv wnd.",
    },
    [FAM_MPTCP_RCV_WND_CONFLICT] = {
        .name = "system_mptcp_rcv_wnd_conflict",
        .type = METRIC_TYPE_COUNTER,
        .help = "Conflict with while updating msk rcv wnd.",
    },
};

struct netstat_metric {
    char *key;
    uint64_t flags;
    int fam;
};

const struct netstat_metric *netstat_get_key (register const char *str, register size_t len);

static char *path_proc_netstat;
static bool proc_netstat_found = false;

int netstat_init(void)
{
    path_proc_netstat = plugin_procpath("net/netstat");
    if (path_proc_netstat == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    if (access(path_proc_netstat, R_OK) != 0) {
        PLUGIN_ERROR("Cannot access %s: %s", path_proc_netstat, STRERRNO);
        return -1;
    }

    proc_netstat_found = true;

    return 0;
}

int netstat_shutdown(void)
{
    free(path_proc_netstat);
    return 0;
}

int netstat_read(uint64_t flags, exclist_t *excl_value)
{
    if (!proc_netstat_found)
        return 0;

    if (!(flags & (COLLECT_IP | COLLECT_MPTCP | COLLECT_TCP)))
        return 0;

    FILE *fh = fopen(path_proc_netstat, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("fopen (%s) failed: %s.", path_proc_netstat, STRERRNO);
        return -1;
    }

    char key_buffer[4096];
    char value_buffer[4096];
    char *key_fields[256];
    char *value_fields[256];

    int status = -1;
    while (true) {
        clearerr(fh);
        char *key_ptr = fgets(key_buffer, sizeof(key_buffer), fh);
        if (key_ptr == NULL) {
            if (feof(fh) != 0) {
                status = 0;
                break;
            } else if (ferror(fh) != 0) {
                PLUGIN_ERROR("Reading from %s failed.", path_proc_netstat);
                break;
            } else {
                PLUGIN_ERROR("fgets failed for an unknown reason.");
                break;
            }
        }

        char *value_ptr = fgets(value_buffer, sizeof(value_buffer), fh);
        if (value_ptr == NULL) {
            PLUGIN_ERROR("read_file (%s): Could not read values line.", path_proc_netstat);
            break;
        }

        key_ptr = strchr(key_buffer, ':');
        if (key_ptr == NULL) {
            PLUGIN_ERROR("Could not find protocol name in keys line.");
            break;
        }
        *key_ptr = 0;
        key_ptr++;

        value_ptr = strchr(value_buffer, ':');
        if (value_ptr == NULL) {
            PLUGIN_ERROR("Could not find protocol name in values line.");
            break;
        }
        *value_ptr = 0;
        value_ptr++;

        if (strcmp(key_buffer, value_buffer) != 0) {
            PLUGIN_ERROR("Protocol names in keys and values lines don't match: `%s' vs. `%s'.",
                         key_buffer, value_buffer);
            break;
        }

        int key_fields_num = strsplit(key_ptr, key_fields, STATIC_ARRAY_SIZE(key_fields));
        int value_fields_num = strsplit(value_ptr, value_fields, STATIC_ARRAY_SIZE(value_fields));

        if (key_fields_num != value_fields_num) {
            PLUGIN_ERROR("Number of fields in keys and values lines don't match: %i vs %i.",
                         key_fields_num, value_fields_num);
            break;
        }

        char name[256];
        size_t name_key_len = strlen(key_buffer);

        if (name_key_len >= sizeof(name))
            continue;

        size_t name_len = sizeof(name) - name_key_len;
        sstrncpy(name, key_buffer, sizeof(name));

        for (int i = 0; i < key_fields_num; i++) {
            sstrncpy(name + name_key_len, key_fields[i], name_len);

            if (!exclist_match(excl_value, name))
                continue;

            const struct netstat_metric *m = netstat_get_key (name, strlen(name));
            if (m == NULL)
                continue;

            if (!(m->flags & flags))
                continue;

            if (fams[m->fam].type == METRIC_TYPE_GAUGE) {
                double value = atof(value_fields[i]);
                if (!isfinite(value))
                    continue;
                metric_family_append(&fams[m->fam], VALUE_GAUGE(value), NULL, NULL);
            } else if (fams[m->fam].type == METRIC_TYPE_COUNTER) {
                uint64_t value = atol(value_fields[i]);
                metric_family_append(&fams[m->fam], VALUE_COUNTER(value), NULL, NULL);
            }
        }
    }

    fclose(fh);

    plugin_dispatch_metric_family_array(fams, FAM_NETSTAT_MAX, 0);

    return status;
}
