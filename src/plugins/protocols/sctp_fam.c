// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "plugins/protocols/sctp_fam.h"
#include "plugins/protocols/flags.h"

static metric_family_t fams[FAM_SCTP_MAX] = {
    [FAM_SCTP_CURRENT_ESTABLISHED] = {
        .name = "system_sctp_current_established",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of associations for which the current state is either "
                "ESTABLISHED, SHUTDOWN-RECEIVED or SHUTDOWN-PENDING.",
    },
    [FAM_SCTP_ACTIVE_ESTABLISHED] = {
        .name = "system_sctp_active_established",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times that associations have made a direct transition to "
                "the ESTABLISHED state from the COOKIE-ECHOED state.",
    },
    [FAM_SCTP_PASSIVE_ESTABLISHED] = {
        .name = "system_sctp_passive_established",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times that associations have made a direct transition to "
                "the ESTABLISHED state from the CLOSED state.",
    },
    [FAM_SCTP_ABORTEDS] = {
        .name = "system_sctp_aborteds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times that associations have made a direct transition to "
                "the CLOSED state from any state using the primitive 'ABORT'.",
    },
    [FAM_SCTP_SHUTDOWNS] = {
        .name = "system_sctp_shutdowns",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times that associations have made a direct transition to "
                "the CLOSED state from either the SHUTDOWN-SENT state or "
                "the SHUTDOWN-ACK-SENT state.",
    },
    [FAM_SCTP_OUT_OF_BLUES] = {
        .name = "system_sctp_out_of_blues",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of out of the blue packets received by the host.",
    },
    [FAM_SCTP_CHECKSUM_ERRORS] = {
        .name = "system_sctp_checksum_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of SCTP packets received with an invalid checksum.",
    },
    [FAM_SCTP_OUT_CTRL_CHUNKS] = {
        .name = "system_sctp_out_ctrl_chunks",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of SCTP control chunks sent (retransmissions are not included).",
    },
    [FAM_SCTP_OUT_ORDER_CHUNKS] = {
        .name = "system_sctp_out_order_chunks",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of SCTP ordered data chunks sent (retransmissions are not included).",
    },
    [FAM_SCTP_OUT_UNORDER_CHUNKS] = {
        .name = "system_sctp_out_unorder_chunks",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of SCTP unordered chunks (data chunks in which the U bit is set to 1) "
                "sent (retransmissions are not included).",
    },
    [FAM_SCTP_IN_CTRL_CHUNKS] = {
        .name = "system_sctp_in_ctrl_chunks",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of SCTP control chunks received (no duplicate chunks included).",
    },
    [FAM_SCTP_IN_ORDER_CHUNKS] = {
        .name = "system_sctp_in_order_chunks",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of SCTP ordered data chunks received (no duplicate chunks included).",
    },
    [FAM_SCTP_IN_UNORDER_CHUNKS] = {
        .name = "system_sctp_in_unorder_chunks",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of SCTP unordered chunks (data chunks in which the U bit is set to 1) "
                "received (no duplicate chunks included).",
    },
    [FAM_SCTP_FRAGMENTED_USER_MSGS] = {
        .name = "system_sctp_fragmented_user_msgs",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of user messages that have to be fragmented because of the MTU.",
    },
    [FAM_SCTP_OUT_PACKETS] = {
        .name = "system_sctp_out_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of SCTP packets sent. Retransmitted DATA chunks are included.",
    },
    [FAM_SCTP_REASSEMBLED_USER_MSGS] = {
        .name = "system_sctp_reassembled_user_msgs",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of user messages reassembled, after conversion into DATA chunks.",
    },
    [FAM_SCTP_IN_PACKETS] = {
        .name = "system_sctp_in_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of SCTP packets received. Duplicates are included.",
    },
    [FAM_SCTP_T1_INIT_EXPIREDS] = {
        .name = "system_sctp_t1_init_expireds",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_SCTP_T1_COOKIE_EXPIREDS] = {
        .name = "system_sctp_t1_cookie_expireds",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_SCTP_T2_SHUTDOWN_EXPIREDS] = {
        .name = "system_sctp_t2_shutdown_expireds",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_SCTP_T3_RTX_EXPIREDS] = {
        .name = "system_sctp_t3_rtx_expireds",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_SCTP_T4_RTO_EXPIREDS] = {
        .name = "system_sctp_t4_rto_expireds",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_SCTP_T5_SHUTDOWN_GUARD_EXPIREDS] = {
        .name = "system_sctp_t5_shutdown_guard_expireds",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_SCTP_DELAY_SACK_EXPIREDS] = {
        .name = "system_sctp_delay_sack_expireds",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_SCTP_AUTOCLOSE_EXPIREDS] = {
        .name = "system_sctp_autoclose_expireds",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_SCTP_T3_RETRANSMITS] = {
        .name = "system_sctp_t3_retransmits",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_SCTP_PMTUD_RETRANSMITS] = {
        .name = "system_sctp_pmtud_retransmits",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_SCTP_FAST_RETRANSMITS] = {
        .name = "system_sctp_fast_retransmits",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_SCTP_IN_PKT_SOFTIRQ] = {
        .name = "system_sctp_in_pkt_softirq",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_SCTP_IN_PKT_BACKLOG] = {
        .name = "system_sctp_in_pkt_backlog",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_SCTP_IN_PKT_DISCARDS] = {
        .name = "system_sctp_in_pkt_discards",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_SCTP_IN_DATA_CHUNK_DISCARDS] = {
        .name = "system_sctp_in_data_chunk_discards",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
};

struct sctp_metric {
    char *key;
    uint64_t flags;
    int fam;
};

const struct sctp_metric *sctp_get_key (register const char *str, register size_t len);

static char *path_proc_sctp;
static bool proc_sctp_found = false;

int sctp_init(void)
{
    path_proc_sctp = plugin_procpath("net/sctp/snmp");
    if (path_proc_sctp == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    if (access(path_proc_sctp, R_OK) != 0) {
        PLUGIN_ERROR("Cannot access %s: %s", path_proc_sctp, STRERRNO);
        return -1;
    }

    proc_sctp_found = true;

    return 0;
}

int sctp_shutdown(void)
{
    free(path_proc_sctp);
    return 0;
}

int sctp_read(uint64_t flags, plugin_filter_t *filter)
{
    if (!proc_sctp_found)
        return 0;

    if (!(flags & COLLECT_SCTP))
        return 0;

    FILE *fh = fopen(path_proc_sctp, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Open '%s' failed: %s", path_proc_sctp, STRERRNO);
        return -1;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *fields[2] = {0};

        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (fields_num < 2)
            continue;

        const struct sctp_metric *m = sctp_get_key(fields[0], strlen(fields[0]));
        if (m == NULL)
            continue;

        if (!(m->flags & flags))
            continue;

        if (fams[m->fam].type == METRIC_TYPE_GAUGE) {
            double value = atof(fields[1]);
            if (!isfinite(value))
                continue;
            metric_family_append(&fams[m->fam], VALUE_GAUGE(value), NULL, NULL);
        } else if (fams[m->fam].type == METRIC_TYPE_COUNTER) {
            uint64_t value = atol(fields[1]);
            metric_family_append(&fams[m->fam], VALUE_COUNTER(value), NULL, NULL);
        }

    }

    fclose(fh);

    plugin_dispatch_metric_family_array_filtered(fams, FAM_SCTP_MAX, filter, 0);

    return 0;
}
