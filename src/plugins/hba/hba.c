// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2012-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#include <sys/scsi.h>
#include <sys/scsi_buf.h>
#include <odmi.h>
#include <sys/cfgodm.h>

struct hba_device {
    char *adapter;
    char *device;
};

static unsigned int refresh = 30;
static unsigned int cnt_read_loop;
static struct hba_device *hba_list;

static exclist_t excl_hba = {0};

enum {
    FAM_HBA_RX_FRAMES,
    FAM_HBA_TX_FRAMES,
    FAM_HBA_INPUT_REQUEST,
    FAM_HBA_OUTPUT_REQUEST,
    FAM_HBA_CONTROL_REQUEST,
    FAM_HBA_INPUT_BYTES,
    FAM_HBA_OUTPUT_BYTES,
    FAM_HBA_LIP,
    FAM_HBA_NOS,
    FAM_HBA_ERROR_FRAMES,
    FAM_HBA_DUMPED_FRAMES,
    FAM_HBA_LINK_FAILURE,
    FAM_HBA_LOST_OF_SYNC,
    FAM_HBA_LOST_OF_SIGNAL,
    FAM_HBA_INVALID_TX_WORD,
    FAM_HBA_INVALID_CRC,
    FAM_HBA_MAX
};

static metric_family_t fams[FAM_HBA_MAX] = {
    [FAM_HBA_RX_FRAMES] = {
        .name = "system_hba_rx_frames",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of frames received.",
    },
    [FAM_HBA_TX_FRAMES] = {
        .name = "system_hba_tx_frames",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of frames transmitted.",
    },
    [FAM_HBA_INPUT_REQUEST] = {
        .name = "system_hba_input_request",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of input requests.",
    },
    [FAM_HBA_OUTPUT_REQUEST] = {
        .name = "system_hba_output_request",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of output requests.",
    },
    [FAM_HBA_CONTROL_REQUEST] = {
        .name = "system_hba_control_request",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of control requests.",
    },
    [FAM_HBA_INPUT_BYTES] = {
        .name = "system_hba_input_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of input bytes.",
    },
    [FAM_HBA_OUTPUT_BYTES] = {
        .name = "system_hba_output_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of output bytes.",
    },
    [FAM_HBA_LIP] = {
        .name = "system_hba_lip",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of LIP events on FC-AL.",
    },
    [FAM_HBA_NOS] = {
        .name = "system_hba_nos",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NOS events.",
    },
    [FAM_HBA_ERROR_FRAMES] = {
        .name = "system_hba_error_frames",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of frames received with the CRC error.",
    },
    [FAM_HBA_DUMPED_FRAMES] = {
        .name = "system_hba_dumped_frames",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total nimber of number of lost frames.",
    },
    [FAM_HBA_LINK_FAILURE] = {
        .name = "system_hba_link_failure",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of link failures.",
    },
    [FAM_HBA_LOST_OF_SYNC] = {
        .name = "system_hba_lost_of_sync",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of loss of sync.",
    },
    [FAM_HBA_LOST_OF_SIGNAL] = {
        .name = "system_hba_lost_of_signal",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of loss of signal."
    },
    [FAM_HBA_INVALID_TX_WORD] = {
        .name = "system_hba_invalid_tx_word",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of invalid transmission words received.",
    },
    [FAM_HBA_INVALID_CRC] = {
        .name = "system_hba_invalid_crc",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of CRC errors in a received frame.",
    },
};

static void hba_list_free(void)
{
    if (hba_list == NULL)
        return;

    for (int i=0; hba_list[i].adapter!=NULL; i++) {
        free(hba_list[i].adapter);
        free(hba_list[i].device);
    }
    free(hba_list);

    hba_list = NULL;
}

static void hba_odm_list(char *criteria)
{
    hba_list_free();

    int status = odm_initialize();
    if (status < 0) {
        char *errmsg;
        status = odm_err_msg (odmerrno, &errmsg);
        if (status < 0)
            PLUGIN_ERROR("Could not initialize the ODM database: %d", odmerrno);
        else
            PLUGIN_ERROR("Could not initialize the ODM database: %s", errmsg);

        return;
    }

    struct listinfo info;
    struct CuDv *cudv = (struct CuDv *) odm_get_list(CuDv_CLASS, criteria, &info, 256, 1);
    if (*(int *)cudv == -1 || !cudv) {
        odm_terminate();
        return;
    }

    hba_list = calloc(info.num+1, sizeof(struct hba_device));
    if (hba_list == NULL) {
        PLUGIN_ERROR("malloc failed");
        odm_terminate();
        return;
    }

    for (int i=0; i<info.num; i++) {
        hba_list[i].adapter = sstrdup(cudv[i].parent);
        hba_list[i].device = sstrdup(cudv[i].name);
        if((hba_list[i].adapter == NULL) || (hba_list[i].device == NULL)) {
            PLUGIN_ERROR("strdup failed");
            hba_list_free();
            odm_terminate();
            return;
        }
    }

    odm_free_list(cudv, &info);

    if (odm_terminate() < 0) {
        char *errmsg;

        if (odm_err_msg (odmerrno, &errmsg)  < 0)
            PLUGIN_WARNING("Could not terminate using the ODM database: %d", odmerrno);
        else
            PLUGIN_WARNING("Could not terminate using the ODM database: %s", errmsg);
    }

    return;
}

int hba_get_stats (char *adapter, char *device)
{
    struct scsi_chba scsi_chba;

    int fd = open(device, O_RDONLY);
    if (fd < 0) {
        PLUGIN_ERROR("open %s failed: %s", device, STRERRNO);
        close (fd);
        return -1;
    }

    memset(&scsi_chba, 0, sizeof(struct scsi_chba));
    scsi_chba.flags = 0;
    scsi_chba.cmd = FC_SCSI_ADAP_STAT;

    int status = ioctl(fd, SCIOLCHBA, &scsi_chba);
    if (status < 0) {
        PLUGIN_ERROR("hba_get_stats ioctl %s failed: %s", device, STRERRNO);
        close(fd);
        return -1;
    }

    metric_family_append(&fams[FAM_HBA_RX_FRAMES],
                         VALUE_COUNTER(scsi_chba.un.adap_stat.RxFrames), NULL,
                         &(label_pair_const_t){.name="adapter", .value=adapter}, NULL);
    metric_family_append(&fams[FAM_HBA_TX_FRAMES],
                         VALUE_COUNTER(scsi_chba.un.adap_stat.TxFrames), NULL,
                         &(label_pair_const_t){.name="adapter", .value=adapter}, NULL);
    metric_family_append(&fams[FAM_HBA_LIP],
                         VALUE_COUNTER(scsi_chba.un.adap_stat.LIPCount), NULL,
                         &(label_pair_const_t){.name="adapter", .value=adapter}, NULL);
    metric_family_append(&fams[FAM_HBA_NOS],
                         VALUE_COUNTER(scsi_chba.un.adap_stat.NOSCount), NULL,
                         &(label_pair_const_t){.name="adapter", .value=adapter}, NULL);
    metric_family_append(&fams[FAM_HBA_ERROR_FRAMES],
                         VALUE_COUNTER(scsi_chba.un.adap_stat.ErrorFrames), NULL,
                         &(label_pair_const_t){.name="adapter", .value=adapter}, NULL);
    metric_family_append(&fams[FAM_HBA_DUMPED_FRAMES],
                         VALUE_COUNTER(scsi_chba.un.adap_stat.DumpedFrames), NULL,
                         &(label_pair_const_t){.name="adapter", .value=adapter}, NULL);
    metric_family_append(&fams[FAM_HBA_LINK_FAILURE],
                         VALUE_COUNTER(scsi_chba.un.adap_stat.LinkFailureCount), NULL,
                         &(label_pair_const_t){.name="adapter", .value=adapter}, NULL);
    metric_family_append(&fams[FAM_HBA_LOST_OF_SYNC],
                         VALUE_COUNTER(scsi_chba.un.adap_stat.LossOfSyncCount), NULL,
                         &(label_pair_const_t){.name="adapter", .value=adapter}, NULL);
    metric_family_append(&fams[FAM_HBA_LOST_OF_SIGNAL],
                         VALUE_COUNTER(scsi_chba.un.adap_stat.LossOfSignalCount), NULL,
                         &(label_pair_const_t){.name="adapter", .value=adapter}, NULL);
    metric_family_append(&fams[FAM_HBA_INVALID_TX_WORD],
                         VALUE_COUNTER(scsi_chba.un.adap_stat.InvalidTxWordCount), NULL,
                         &(label_pair_const_t){.name="adapter", .value=adapter}, NULL);
    metric_family_append(&fams[FAM_HBA_INVALID_CRC],
                         VALUE_COUNTER(scsi_chba.un.adap_stat.InvalidCRCCount), NULL,
                         &(label_pair_const_t){.name="adapter", .value=adapter}, NULL);

    memset(&scsi_chba, 0, sizeof(struct scsi_chba));
    scsi_chba.flags = 0;
    scsi_chba.cmd = FC_TRAFFIC_STAT;
    scsi_chba.un.traffic_info.trans_type = FC_SCSI_FCP_TYPE;

    status = ioctl(fd, SCIOLCHBA, &scsi_chba);
    if (status < 0) {
        PLUGIN_ERROR("hba_get_stats ioctl %s failed: %s", device, STRERRNO);
        close(fd);
        return -1;
    }

    metric_family_append(&fams[FAM_HBA_INPUT_REQUEST],
                         VALUE_COUNTER(scsi_chba.un.traffic_info.traffic_stat.inp_reqs), NULL,
                         &(label_pair_const_t){.name="adapter", .value=adapter}, NULL);
    metric_family_append(&fams[FAM_HBA_OUTPUT_REQUEST],
                         VALUE_COUNTER(scsi_chba.un.traffic_info.traffic_stat.out_reqs), NULL,
                         &(label_pair_const_t){.name="adapter", .value=adapter}, NULL);
    metric_family_append(&fams[FAM_HBA_CONTROL_REQUEST],
                         VALUE_COUNTER(scsi_chba.un.traffic_info.traffic_stat.ctrl_reqs), NULL,
                         &(label_pair_const_t){.name="adapter", .value=adapter}, NULL);
    metric_family_append(&fams[FAM_HBA_INPUT_BYTES],
                         VALUE_COUNTER(scsi_chba.un.traffic_info.traffic_stat.inp_bytes), NULL,
                         &(label_pair_const_t){.name="adapter", .value=adapter}, NULL);
    metric_family_append(&fams[FAM_HBA_OUTPUT_BYTES],
                         VALUE_COUNTER(scsi_chba.un.traffic_info.traffic_stat.out_bytes), NULL,
                         &(label_pair_const_t){.name="adapter", .value=adapter}, NULL);

    close(fd);

    return 0;
}

static int hba_read (void)
{
    if (!(cnt_read_loop % 30) || (hba_list == NULL)) {
        hba_odm_list("parent LIKE fcs* AND status=1");
        cnt_read_loop = 0;
    }

    if (hba_list == NULL)
        return 0;

    for (int i=0; hba_list[i].adapter!=NULL; i++) {
        if (!exclist_match(&excl_hba, hba_list[i].adapter))
            continue;

        char device[256];
        ssnprintf(device, sizeof(device),"/dev/%s", hba_list[i].device);
        hba_get_stats (hba_list[i].adapter, device);
    }

    cnt_read_loop++;

    return 0;
}

static int hba_config (config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp ("refresh", child->key) == 0) {
            status = cf_util_get_unsigned_int(child, &refresh);
        } else if (strcasecmp ("hba", child->key) == 0) {
            status = cf_util_exclist(child, &excl_hba);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int hba_shutdown (void)
{
    exclist_reset(&excl_hba);
    hba_list_free();
    return 0;
}

void module_register (void)
{
    plugin_register_config ("hba", hba_config);
    plugin_register_shutdown ("hba", hba_shutdown);
    plugin_register_read ("hba", hba_read);
}
