// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com

#include "plugin.h"
#include "libutils/common.h"

enum {
    FAM_XFRM_IN_ERROR,
    FAM_XFRM_IN_BUFFER_ERROR,
    FAM_XFRM_IN_HEADER_ERROR,
    FAM_XFRM_IN_NO_STATES,
    FAM_XFRM_IN_STATE_PROTOCOL_ERROR,
    FAM_XFRM_IN_STATE_MODE_ERROR,
    FAM_XFRM_IN_STATE_SEQUENCE_ERROR,
    FAM_XFRM_IN_STATE_EXPIRED,
    FAM_XFRM_IN_STATE_MISMATCH,
    FAM_XFRM_IN_STATE_INVALID,
    FAM_XFRM_IN_TEMPLATE_MISMATCH,
    FAM_XFRM_IN_NO_POLICY,
    FAM_XFRM_IN_POLICY_BLOCK,
    FAM_XFRM_IN_POLICY_ERROR,
    FAM_XFRM_ACQUIRE_ERROR,
    FAM_XFRM_FORWARD,
    FAM_XFRM_OUT_ERROR,
    FAM_XFRM_OUT_BUNDLE_GENERATION_ERROR,
    FAM_XFRM_OUT_BUNDLE_CHECK_ERROR,
    FAM_XFRM_OUT_NO_STATES,
    FAM_XFRM_OUT_STATE_PROTOCOL_ERROR,
    FAM_XFRM_OUT_STATE_MODE_ERROR,
    FAM_XFRM_OUT_STATE_SEQUENCE_ERROR,
    FAM_XFRM_OUT_STATE_EXPIRED,
    FAM_XFRM_OUT_POLICY_BLOCK,
    FAM_XFRM_OUT_POLICY_DEAD,
    FAM_XFRM_OUT_POLICY_ERROR,
    FAM_XFRM_OUT_STATE_INVALID,
    FAM_XFRM_MAX,
};

#include "plugins/xfrm/xfrm_stat.h"

static metric_family_t fams[FAM_XFRM_MAX] = {
    [FAM_XFRM_IN_ERROR] = {
        .name = "system_xfrm_in_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "All errors which is not matched others.",
    },
    [FAM_XFRM_IN_BUFFER_ERROR] = {
        .name = "system_xfrm_in_buffer_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "No buffer is left.",
    },
    [FAM_XFRM_IN_HEADER_ERROR] = {
        .name = "system_xfrm_in_header_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Header error.",
    },
    [FAM_XFRM_IN_NO_STATES] = {
        .name = "system_xfrm_in_no_states",
        .type = METRIC_TYPE_COUNTER,
        .help = "No state is found i.e. Either inbound SPI, address, "
                "or IPsec protocol at SA is wrong.",
    },
    [FAM_XFRM_IN_STATE_PROTOCOL_ERROR] = {
        .name = "system_xfrm_in_state_protocol_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Transformation protocol specific error e.g. SA key is wrong.",
    },
    [FAM_XFRM_IN_STATE_MODE_ERROR] = {
        .name = "system_xfrm_in_state_mode_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Transformation mode specific error.",
    },
    [FAM_XFRM_IN_STATE_SEQUENCE_ERROR] = {
        .name = "system_xfrm_in_state_sequence_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Sequence error i.e. Sequence number is out of window.",
    },
    [FAM_XFRM_IN_STATE_EXPIRED] = {
        .name = "system_xfrm_in_state_expired",
        .type = METRIC_TYPE_COUNTER,
        .help = "State is expired.",
    },
    [FAM_XFRM_IN_STATE_MISMATCH] = {
        .name = "system_xfrm_in_state_mismatch",
        .type = METRIC_TYPE_COUNTER,
        .help = "State has mismatch option e.g. UDP encapsulation type is mismatch.",
    },
    [FAM_XFRM_IN_STATE_INVALID] = {
        .name = "system_xfrm_in_state_invalid",
        .type = METRIC_TYPE_COUNTER,
        .help = "State is invalid.",
    },
    [FAM_XFRM_IN_TEMPLATE_MISMATCH] = {
        .name = "system_xfrm_in_template_mismatch",
        .type = METRIC_TYPE_COUNTER,
        .help = "No matching template for states e.g. "
                "Inbound SAs are correct but SP rule is wrong.",
    },
    [FAM_XFRM_IN_NO_POLICY] = {
        .name = "system_xfrm_in_no_policy",
        .type = METRIC_TYPE_COUNTER,
        .help = "No policy is found for states e.g. "
                "Inbound SAs are correct but no SP is found.",
    },
    [FAM_XFRM_IN_POLICY_BLOCK] = {
        .name = "system_xfrm_in_policy_block",
        .type = METRIC_TYPE_COUNTER,
        .help = "Policy discards.",
    },
    [FAM_XFRM_IN_POLICY_ERROR] = {
        .name = "system_xfrm_in_policy_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Policy error.",
    },
    [FAM_XFRM_ACQUIRE_ERROR] = {
        .name = "system_xfrm_acquire_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "State hasn’t been fully acquired before use.",
    },
    [FAM_XFRM_FORWARD] = {
        .name = "system_xfrm_forward",
        .type = METRIC_TYPE_COUNTER,
        .help = "Forward routing of a packet is not allowed.",
    },
    [FAM_XFRM_OUT_ERROR] = {
        .name = "system_xfrm_out_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "All errors which is not matched others.",
    },
    [FAM_XFRM_OUT_BUNDLE_GENERATION_ERROR] = {
        .name = "system_xfrm_out_bundle_generation_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Bundle generation error.",
    },
    [FAM_XFRM_OUT_BUNDLE_CHECK_ERROR] = {
        .name = "system_xfrm_out_bundle_check_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Bundle check error.",
    },
    [FAM_XFRM_OUT_NO_STATES] = {
        .name = "system_xfrm_out_no_states",
        .type = METRIC_TYPE_COUNTER,
        .help = "No state is found.",
    },
    [FAM_XFRM_OUT_STATE_PROTOCOL_ERROR] = {
        .name = "system_xfrm_out_state_protocol_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Transformation protocol specific error.",
    },
    [FAM_XFRM_OUT_STATE_MODE_ERROR] = {
        .name = "system_xfrm_out_state_mode_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Transformation mode specific error.",
    },
    [FAM_XFRM_OUT_STATE_SEQUENCE_ERROR] = {
        .name = "system_xfrm_out_state_sequence_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Sequence error i.e. Sequence number overflow.",
    },
    [FAM_XFRM_OUT_STATE_EXPIRED] = {
        .name = "system_xfrm_out_state_expired",
        .type = METRIC_TYPE_COUNTER,
        .help = "State is expired.",
    },
    [FAM_XFRM_OUT_POLICY_BLOCK] = {
        .name = "system_xfrm_out_policy_block",
        .type = METRIC_TYPE_COUNTER,
        .help = "Policy discards.",
    },
    [FAM_XFRM_OUT_POLICY_DEAD] = {
        .name = "system_xfrm_out_policy_dead",
        .type = METRIC_TYPE_COUNTER,
        .help = "Policy is dead.",
    },
    [FAM_XFRM_OUT_POLICY_ERROR] = {
        .name = "system_xfrm_out_policy_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Policy error.",
    },
    [FAM_XFRM_OUT_STATE_INVALID] = {
        .name = "system_xfrm_out_state_invalid",
        .type = METRIC_TYPE_COUNTER,
        .help = "State is invalid, perhaps expired.",
    },
};

static char *path_proc_xfrm;

static int xfrm_read(void)
{
    FILE *fh = fopen(path_proc_xfrm, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Unable to open '%s': %s", path_proc_xfrm, STRERRNO);
        return -1;
    }

    char buffer[256];
    while(fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *fields[2];
        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (fields_num != 2)
            continue;

        const struct xfrm_stat *xfrm_stat = xfrm_stat_get_key (fields[0], strlen(fields[0]));
        if (xfrm_stat == NULL)
            continue;

        if (fams[xfrm_stat->fam].type == METRIC_TYPE_COUNTER) {
            uint64_t value = (uint64_t)strtoull(fields[1], NULL, 10);
            metric_family_append(&fams[xfrm_stat->fam], VALUE_COUNTER(value), NULL, NULL);
        }
    }

    fclose(fh);

    plugin_dispatch_metric_family_array(fams, FAM_XFRM_MAX, 0);
    return 0;
}

static int xfrm_init(void)
{
    path_proc_xfrm = plugin_procpath("net/xfrm_stat");
    if (path_proc_xfrm == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int xfrm_shutdown(void)
{
    free(path_proc_xfrm);
    return 0;
}

void module_register(void)
{
    plugin_register_init("xfrm", xfrm_init);
    plugin_register_read("xfrm", xfrm_read);
    plugin_register_shutdown("xfrm", xfrm_shutdown);
}
