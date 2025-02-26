// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

enum {
    FAM_ISCSI_LUN_IOPS,
    FAM_ISCSI_LUN_READ_BYTES,
    FAM_ISCSI_LUN_WRITE_BYTES,
    FAM_ISCSI_LUN_SIZE_BYTES,


    FAM_ISCSI_MAX,
};

static metric_family_t fams[FAM_ISCSI_MAX] = {
    [FAM_ISCSI_LUN_IOPS] = {
        .name = "system_iscsi_lun_iops",
        .type = METRIC_TYPE_COUNTER,
        .help = "R/W IOPS",
    },
    [FAM_ISCSI_LUN_READ_BYTES] = {
        .name = "system_iscsi_lun_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Read MB",
    },
    [FAM_ISCSI_LUN_WRITE_BYTES] = {
        .name = "system_iscsi_lun_write_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Write MB",
    },
    [FAM_ISCSI_LUN_SIZE_BYTES] = {
        .name = "system_iscsi_lun_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "LUN Size (GB)",
    },


};

typedef struct {
    const char *iqn;
    const char *tpgt;
} iscsi_labels_t;

static char *path_sys_target_scsi;

static int iscsi_read_lun(int dir_fd, __attribute__((unused)) const char *path,
                        const char *entry, void *ud)
{
    if (strncmp(entry, "lun_", strlen("lun_")) != 0)
        return 0;

    iscsi_labels_t *ll = ud;
    const char *lun = entry + strlen("lun_");
    uint64_t value = 0;
    int status = 0;
    char fpath[PATH_MAX];

    ssnprintf(fpath, sizeof(fpath), "%s/%s", entry, "statistics/scsi_tgt_port/in_cmds");
    status = filetouint_at(dir_fd, fpath, &value);
    if (likely(status == 0))
        metric_family_append(&fams[FAM_ISCSI_LUN_IOPS], VALUE_COUNTER(value), NULL,
                             &(label_pair_const_t){.name="iqn", .value=ll->iqn},
                             &(label_pair_const_t){.name="tpgt", .value=ll->tpgt},
                             &(label_pair_const_t){.name="lun", .value=lun},
                             NULL);

    ssnprintf(fpath, sizeof(fpath), "%s/%s", entry, "statistics/scsi_tgt_port/read_mbytes");
    status = filetouint_at(dir_fd, fpath, &value);
    if (likely(status == 0))
        metric_family_append(&fams[FAM_ISCSI_LUN_READ_BYTES], VALUE_COUNTER(value * 1024 * 1024), NULL,
                             &(label_pair_const_t){.name="iqn", .value=ll->iqn},
                             &(label_pair_const_t){.name="tpgt", .value=ll->tpgt},
                             &(label_pair_const_t){.name="lun", .value=lun},
                             NULL);
    
    ssnprintf(fpath, sizeof(fpath), "%s/%s", entry, "statistics/scsi_tgt_port/write_mbytes");
    status = filetouint_at(dir_fd, fpath, &value);
    if (likely(status == 0))
        metric_family_append(&fams[FAM_ISCSI_LUN_WRITE_BYTES], VALUE_COUNTER(value * 1024 * 1024), NULL,
                             &(label_pair_const_t){.name="iqn", .value=ll->iqn},
                             &(label_pair_const_t){.name="tpgt", .value=ll->tpgt},
                             &(label_pair_const_t){.name="lun", .value=lun},
                             NULL);
    
    return 0;
}

static int iscsi_read_tpgt(int dir_fd, __attribute__((unused)) const char *path,
                         const char *entry, void *ud)
{
    if (strncmp(entry, "tpgt_", strlen("tpgt_")) != 0)
        return 0;

    iscsi_labels_t *ll = ud;
    ll->tpgt = entry + strlen("tpgt_");

    char fpath[PATH_MAX];

    ssnprintf(fpath, sizeof(fpath), "%s/enable", entry);
    uint64_t value = 0;
    int status = filetouint_at(dir_fd, fpath, &value);
    if (status != 0)
        return 0;

    if (value != 1)
        return 0;

    ssnprintf(fpath, sizeof(fpath), "%s/lun", entry);
    walk_directory_at(dir_fd, fpath, iscsi_read_lun, ll, 0);

    return 0;
}

static int iscsi_read_iqn(int dir_fd, __attribute__((unused)) const char *path,
                        const char *entry, __attribute__((unused))  void *ud)
{
    if (strncmp(entry, "iqn", strlen("iqn")) != 0)
        return 0;

    iscsi_labels_t ll = {0};
    ll.iqn = entry;

    walk_directory_at(dir_fd, entry, iscsi_read_tpgt, &ll, 0);

    return 0;
}

static int iscsi_read(void)
{
    walk_directory(path_sys_target_scsi, iscsi_read_iqn, NULL, 0);
    plugin_dispatch_metric_family_array(fams, FAM_ISCSI_MAX, 0);
    return 0;
}

static int iscsi_init(void)
{
    path_sys_target_scsi = plugin_syspath("kernel/config/target/iscsi");
    if (path_sys_target_scsi == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    return 0;
}

static int iscsi_shutdown(void)
{
    free(path_sys_target_scsi);
    return 0;
}

void module_register(void)
{
    plugin_register_init("iscsi", iscsi_init);
    plugin_register_read("iscsi", iscsi_read);
    plugin_register_shutdown("iscsi", iscsi_shutdown);
}
