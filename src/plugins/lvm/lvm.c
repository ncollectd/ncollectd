// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2013 Chad Malfait
// SPDX-FileCopyrightText: Copyright (C) 2014 Carnegie Mellon University
// SPDX-FileCopyrightText: Copyright (C) 2020 Joseph Nahmias
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Chad Malfait <malfaitc at yahoo.com>
// SPDX-FileContributor: Benjamin Gilbert <bgilbert at backtick.net>
// SPDX-FileContributor: Joseph Nahmias <joe at nahmias.net>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exec.h"
#include "libxson/json_parse.h"

#include <poll.h>

#ifdef HAVE_SYS_CAPABILITY_H
#include <sys/capability.h>
#endif

#define NO_VALUE UINT64_MAX
#define PERCENT_SCALE_FACTOR 1e-8

#ifndef DM_NAME_LEN
#define DM_NAME_LEN 128
#endif

enum {
    FAM_LVM_VG_SIZE_BYTES,
    FAM_LVM_VG_FREE_BYTES,
    FAM_LVM_VG_SNAP_COUNT,
    FAM_LVM_VG_LV_COUNT,
    FAM_LVM_LV_SIZE_BYTES,
    FAM_LVM_LV_DATA_USED_BYTES,
    FAM_LVM_LV_DATA_FREE_BYTES,
    FAM_LVM_LV_METADATA_USED_BYTES,
    FAM_LVM_LV_METADATA_FREE_BYTES,
    FAM_LVM_MAX,
};

static metric_family_t fams[FAM_LVM_MAX] = {
    [FAM_LVM_VG_SIZE_BYTES] = {
        .name = "system_lvm_vg_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Size of this Volume Group (VG) in bytes.",
    },
    [FAM_LVM_VG_FREE_BYTES] = {
        .name = "system_lvm_vg_free_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Free space in this Volume Group (VG) in bytes.",
    },
    [FAM_LVM_VG_SNAP_COUNT] = {
        .name = "system_lvm_vg_snap_count",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of snapshots in this Volume Group (VG).",
    },
    [FAM_LVM_VG_LV_COUNT] = {
        .name = "system_lvm_vg_lv_count",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of Logical Volumes (LVs) in this Volume Group (VG).",
    },
    [FAM_LVM_LV_SIZE_BYTES] = {
        .name = "system_lvm_lv_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Size of this Logical Volume (LV) in bytes.",
    },
    [FAM_LVM_LV_DATA_USED_BYTES] = {
        .name = "system_lvm_lv_data_used_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_LVM_LV_DATA_FREE_BYTES] = {
        .name = "system_lvm_lv_data_free_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_LVM_LV_METADATA_USED_BYTES] = {
        .name = "system_lvm_lv_metadata_used_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_LVM_LV_METADATA_FREE_BYTES] = {
        .name = "system_lvm_lv_metadata_free_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    }
};

typedef enum {
    LVM_JSON_NONE,
    LVM_JSON_REPORT,
    LVM_JSON_REPORT_VG,
    LVM_JSON_REPORT_VG_VG_NAME,
    LVM_JSON_REPORT_VG_VG_SIZE,
    LVM_JSON_REPORT_VG_VG_FREE,
    LVM_JSON_REPORT_VG_SNAP_COUNT,
    LVM_JSON_REPORT_VG_LV_COUNT,
    LVM_JSON_REPORT_LV,
    LVM_JSON_REPORT_LV_VG_NAME,
    LVM_JSON_REPORT_LV_LV_NAME,
    LVM_JSON_REPORT_LV_LV_ATTR,
    LVM_JSON_REPORT_LV_LV_SIZE,
    LVM_JSON_REPORT_LV_LV_METADATA_SIZE,
    LVM_JSON_REPORT_LV_DATA_LV,
    LVM_JSON_REPORT_LV_METADATA_LV,
    LVM_JSON_REPORT_LV_DATA_PERCENT,
    LVM_JSON_REPORT_LV_METADATA_PERCENT,
} lvm_json_key_t;

typedef struct {
    lvm_json_key_t stack[JSON_MAX_DEPTH];
    size_t depth;
    union {
        struct {
            char vg_name[DM_NAME_LEN];
            uint64_t vg_free;
            uint64_t vg_size;
            uint64_t snap_count;
            uint64_t lv_count;
        } vg;
        struct {
            char vg_name[DM_NAME_LEN];
            char lv_name[DM_NAME_LEN];
            char data_lv[DM_NAME_LEN];
            char metadata_lv[DM_NAME_LEN];
            char lv_attr[32];
            uint64_t lv_size;
            uint64_t lv_metadata_size;
            uint64_t data_percent;
            uint64_t metadata_percent;
        } lv;
    };
} lvm_json_ctx_t;


static int lvm_metrics_vg(lvm_json_ctx_t *sctx)
{
    if (sctx->vg.vg_name[0] == '\0')
        return -1;

    if (sctx->vg.vg_free != UINT64_MAX)
        metric_family_append(&fams[FAM_LVM_VG_FREE_BYTES],
                             VALUE_GAUGE(sctx->vg.vg_free), NULL,
                             &(label_pair_const_t){.name="vg_name", .value=sctx->vg.vg_name},
                             NULL);
    if (sctx->vg.vg_size != UINT64_MAX)
        metric_family_append(&fams[FAM_LVM_VG_SIZE_BYTES],
                             VALUE_GAUGE(sctx->vg.vg_size), NULL,
                             &(label_pair_const_t){.name="vg_name", .value=sctx->vg.vg_name},
                             NULL);
    if (sctx->vg.lv_count != UINT64_MAX)
        metric_family_append(&fams[FAM_LVM_VG_LV_COUNT],
                             VALUE_GAUGE(sctx->vg.lv_count), NULL,
                             &(label_pair_const_t){.name="vg_name", .value=sctx->vg.vg_name},
                             NULL);
    if (sctx->vg.snap_count != UINT64_MAX)
        metric_family_append(&fams[FAM_LVM_VG_SNAP_COUNT],
                             VALUE_GAUGE(sctx->vg.snap_count), NULL,
                             &(label_pair_const_t){.name="vg_name", .value=sctx->vg.vg_name},
                             NULL);
    return 0;
}


static int lvm_metrics_lv(lvm_json_ctx_t *sctx)
{
    if ((sctx->lv.vg_name[0] == '\0') || (sctx->lv.lv_name[0] == '\0') ||
        (sctx->lv.lv_attr[0] == '\0') || (sctx->lv.lv_size == UINT64_MAX))
        return -1;

    switch (sctx->lv.lv_attr[0]) {
    case 's':
    case 'S': {
        /* Snapshot.  Also report used/free space. */
        if (sctx->lv.data_percent == UINT64_MAX)
            break;

        uint64_t used_bytes = sctx->lv.lv_size *
                              (sctx->lv.data_percent * PERCENT_SCALE_FACTOR);

        metric_family_append(&fams[FAM_LVM_LV_DATA_USED_BYTES],
                             VALUE_GAUGE(used_bytes), NULL,
                             &(label_pair_const_t){.name="vg_name", .value=sctx->lv.vg_name},
                             &(label_pair_const_t){.name="lv_name", .value=sctx->lv.lv_name},
                             NULL);
        metric_family_append(&fams[FAM_LVM_LV_DATA_FREE_BYTES],
                             VALUE_GAUGE(sctx->lv.lv_size - used_bytes), NULL,
                             &(label_pair_const_t){.name="vg_name", .value=sctx->lv.vg_name},
                             &(label_pair_const_t){.name="lv_name", .value=sctx->lv.lv_name},
                             NULL);
    }   break;
    case 't': {
        /* Thin pool virtual volume.    We report the underlying data
             and metadata volumes, not this one.    Report used/free
             space, then ignore. */

        if ((sctx->lv.data_percent == UINT64_MAX) ||
            (sctx->lv.metadata_percent == UINT64_MAX) ||
            (sctx->lv.lv_metadata_size == UINT64_MAX) ||
            (sctx->lv.data_lv[0] == '\0') ||
            (sctx->lv.metadata_lv[0] == '\0'))
            return -1;

        uint64_t used_bytes = sctx->lv.lv_size *
                              (sctx->lv.data_percent * PERCENT_SCALE_FACTOR);

        metric_family_append(&fams[FAM_LVM_LV_DATA_USED_BYTES],
                             VALUE_GAUGE(used_bytes), NULL,
                             &(label_pair_const_t){.name="vg_name", .value=sctx->lv.vg_name},
                             &(label_pair_const_t){.name="lv_name", .value=sctx->lv.data_lv},
                             NULL);
        metric_family_append(&fams[FAM_LVM_LV_DATA_FREE_BYTES],
                             VALUE_GAUGE(sctx->lv.lv_size - used_bytes), NULL,
                             &(label_pair_const_t){.name="vg_name", .value=sctx->lv.vg_name},
                             &(label_pair_const_t){.name="lv_name", .value=sctx->lv.data_lv},
                             NULL);


        used_bytes = sctx->lv.lv_metadata_size *
                     (sctx->lv.metadata_percent * PERCENT_SCALE_FACTOR);

        metric_family_append(&fams[FAM_LVM_LV_METADATA_USED_BYTES],
                             VALUE_GAUGE(used_bytes), NULL,
                             &(label_pair_const_t){.name="vg_name", .value=sctx->lv.vg_name},
                             &(label_pair_const_t){.name="lv_name", .value=sctx->lv.metadata_lv},
                             NULL);
        metric_family_append(&fams[FAM_LVM_LV_METADATA_FREE_BYTES],
                             VALUE_GAUGE(sctx->lv.lv_metadata_size - used_bytes), NULL,
                             &(label_pair_const_t){.name="vg_name", .value=sctx->lv.vg_name},
                             &(label_pair_const_t){.name="lv_name", .value=sctx->lv.metadata_lv},
                             NULL);
        return 0;
    }   break;
    case 'v':
        /* Virtual volume. Ignore. */
        return 0;
        break;
    case 'V':
        return 0;
        /* Thin volume or thin snapshot. Ignore. */
        break;
    }

    metric_family_append(&fams[FAM_LVM_LV_SIZE_BYTES],
                         VALUE_GAUGE(sctx->lv.lv_size), NULL,
                         &(label_pair_const_t){.name="vg_name", .value=sctx->lv.vg_name},
                         &(label_pair_const_t){.name="lv_name", .value=sctx->lv.lv_name},
                         NULL);
    return 0;
}

static bool lvm_parse_number(const char *string, size_t string_len, uint64_t *value)
{
    char number[64];
    sstrnncpy(number, sizeof(number), string, string_len);

    if (parse_uinteger(number, value) != 0)
        return false;
    return true;
}

static bool lvm_json_number(void *ctx, const char *string, size_t string_len)
{
    lvm_json_ctx_t *sctx = ctx;

    if (string_len == 0)
        return true;

    if (sctx->depth == 3) {
        switch (sctx->stack[2]) {
        case LVM_JSON_REPORT_VG_VG_SIZE:
            return lvm_parse_number(string, string_len, &sctx->vg.vg_size);
            break;
        case LVM_JSON_REPORT_VG_VG_FREE:
            return lvm_parse_number(string, string_len, &sctx->vg.vg_free);
            break;
        case LVM_JSON_REPORT_VG_SNAP_COUNT:
            return lvm_parse_number(string, string_len, &sctx->vg.snap_count);
            break;
        case LVM_JSON_REPORT_VG_LV_COUNT:
            return lvm_parse_number(string, string_len, &sctx->vg.lv_count);
            break;
        case LVM_JSON_REPORT_LV_LV_SIZE:
            return lvm_parse_number(string, string_len, &sctx->lv.lv_size);
            break;
        case LVM_JSON_REPORT_LV_DATA_PERCENT:
            return lvm_parse_number(string, string_len, &sctx->lv.data_percent);
            break;
        case LVM_JSON_REPORT_LV_LV_METADATA_SIZE:
            return lvm_parse_number(string, string_len, &sctx->lv.lv_metadata_size);
            break;
        case LVM_JSON_REPORT_LV_METADATA_PERCENT:
            return lvm_parse_number(string, string_len, &sctx->lv.metadata_percent);
            break;
        default:
            break;
        }
    }

    return true;
}

static bool lvm_json_string(void *ctx, const char *string, size_t string_len)
{
    lvm_json_ctx_t *sctx = ctx;

    if (string_len == 0)
        return true;

    if (sctx->depth == 3) {
        switch (sctx->stack[2]) {
        case LVM_JSON_REPORT_VG_VG_NAME:
            sstrnncpy(sctx->vg.vg_name, sizeof(sctx->vg.vg_name), string, string_len);
            break;
        case LVM_JSON_REPORT_VG_VG_SIZE:
           return  lvm_parse_number(string, string_len, &sctx->vg.vg_size);
            break;
        case LVM_JSON_REPORT_VG_VG_FREE:
            return lvm_parse_number(string, string_len, &sctx->vg.vg_free);
            break;
        case LVM_JSON_REPORT_LV_VG_NAME:
            sstrnncpy(sctx->lv.vg_name, sizeof(sctx->lv.vg_name), string, string_len);
            break;
        case LVM_JSON_REPORT_VG_SNAP_COUNT:
            return lvm_parse_number(string, string_len, &sctx->vg.snap_count);
            break;
        case LVM_JSON_REPORT_VG_LV_COUNT:
            return lvm_parse_number(string, string_len, &sctx->vg.lv_count);
            break;
        case LVM_JSON_REPORT_LV_LV_NAME:
            sstrnncpy(sctx->lv.lv_name, sizeof(sctx->lv.lv_name), string, string_len);
            break;
        case LVM_JSON_REPORT_LV_LV_ATTR:
            sstrnncpy(sctx->lv.lv_attr, sizeof(sctx->lv.lv_attr), string, string_len);
            break;
        case LVM_JSON_REPORT_LV_LV_SIZE:
            return lvm_parse_number(string, string_len, &sctx->lv.lv_size);
            break;
        case LVM_JSON_REPORT_LV_DATA_LV:
            sstrnncpy(sctx->lv.data_lv, sizeof(sctx->lv.data_lv), string, string_len);
            break;
        case LVM_JSON_REPORT_LV_METADATA_LV:
            sstrnncpy(sctx->lv.metadata_lv, sizeof(sctx->lv.metadata_lv), string, string_len);
            break;
        case LVM_JSON_REPORT_LV_DATA_PERCENT:
            return lvm_parse_number(string, string_len, &sctx->lv.data_percent);
            break;
        case LVM_JSON_REPORT_LV_LV_METADATA_SIZE:
            return lvm_parse_number(string, string_len, &sctx->lv.lv_metadata_size);
            break;
        case LVM_JSON_REPORT_LV_METADATA_PERCENT:
            return lvm_parse_number(string, string_len, &sctx->lv.metadata_percent);
            break;
        default:
            break;
        }
    }

    return true;
}

static bool lvm_json_start_map(void *ctx)
{
    lvm_json_ctx_t *sctx = ctx;

    if (sctx->depth == 2) {
        switch (sctx->stack[1]) {
        case LVM_JSON_REPORT_VG:
            sctx->vg.vg_name[0] = '\0';
            sctx->vg.vg_free = UINT64_MAX;
            sctx->vg.vg_size = UINT64_MAX;
            sctx->vg.snap_count = UINT64_MAX;
            sctx->vg.lv_count = UINT64_MAX;
            break;
        case LVM_JSON_REPORT_LV:
            sctx->lv.vg_name[0] = '\0';
            sctx->lv.lv_name[0] = '\0';
            sctx->lv.data_lv[0] = '\0';
            sctx->lv.metadata_lv[0] = '\0';
            sctx->lv.lv_attr[0] = '\0';
            sctx->lv.lv_size = UINT64_MAX;
            sctx->lv.lv_metadata_size = UINT64_MAX;
            sctx->lv.data_percent = UINT64_MAX;
            sctx->lv.metadata_percent = UINT64_MAX;
            break;
        default:
            break;
        }
    }

    sctx->depth++;
    if (sctx->depth > JSON_MAX_DEPTH)
        return false;

    sctx->stack[sctx->depth-1] = LVM_JSON_NONE;

    return true;
}

static bool lvm_json_map_key(void * ctx, const char *key, size_t key_len)
{
    lvm_json_ctx_t *sctx = ctx;

    switch (sctx->depth) {
    case 1:
        if ((key_len == 6) && (strncmp((const char *)key, "report", key_len) == 0)) {
            sctx->stack[0] = LVM_JSON_REPORT;
            return 1;
        }
        sctx->stack[0] = LVM_JSON_NONE;
        break;
    case 2:
        if (sctx->stack[0] == LVM_JSON_REPORT) {
            if (key_len == 2) {
                if (strncmp((const char *)key, "vg", key_len) == 0) {
                    sctx->stack[1] = LVM_JSON_REPORT_VG;
                    return 1;
                } else if (strncmp((const char *)key, "lv", key_len) == 0) {
                    sctx->stack[1] = LVM_JSON_REPORT_LV;
                    return 1;
                }
            }
        }
        sctx->stack[1] = LVM_JSON_NONE;
        break;
    case 3:
        switch(sctx->stack[1]) {
        case LVM_JSON_REPORT_VG:
            switch(key_len) {
            case 7:
                if (strncmp((const char *)key, "vg_name", key_len) == 0) {
                    sctx->stack[2] = LVM_JSON_REPORT_VG_VG_NAME;
                    return 1;
                } else if (strncmp((const char *)key, "vg_size", key_len) == 0) {
                    sctx->stack[2] = LVM_JSON_REPORT_VG_VG_SIZE;
                    return 1;
                } else if (strncmp((const char *)key, "vg_free", key_len) == 0) {
                    sctx->stack[2] = LVM_JSON_REPORT_VG_VG_FREE;
                    return 1;
                }
                break;
            case 8:
                if (strncmp((const char *)key, "lv_count", key_len) == 0) {
                    sctx->stack[2] = LVM_JSON_REPORT_VG_LV_COUNT;
                    return 1;
                }
                break;
            case 10:
                if (strncmp((const char *)key, "snap_count", key_len) == 0) {
                    sctx->stack[2] = LVM_JSON_REPORT_VG_SNAP_COUNT;
                    return 1;
                }
                break;
            }
            break;
        case LVM_JSON_REPORT_LV:
            switch(key_len) {
            case 7:
                if (strncmp((const char *)key, "vg_name", key_len) == 0) {
                    sctx->stack[2] = LVM_JSON_REPORT_LV_VG_NAME;
                    return 1;
                } else if (strncmp((const char *)key, "lv_name", key_len) == 0) {
                    sctx->stack[2] = LVM_JSON_REPORT_LV_LV_NAME;
                    return 1;
                } else if (strncmp((const char *)key, "lv_size", key_len) == 0) {
                    sctx->stack[2] = LVM_JSON_REPORT_LV_LV_SIZE;
                    return 1;
                } else if (strncmp((const char *)key, "lv_attr", key_len) == 0) {
                    sctx->stack[2] = LVM_JSON_REPORT_LV_LV_ATTR;
                    return 1;
                } else if (strncmp((const char *)key, "data_lv", key_len) == 0) {
                    sctx->stack[2] = LVM_JSON_REPORT_LV_DATA_LV;
                    return 1;
                }
                break;
            case 11:
                if (strncmp((const char *)key, "metadata_lv", key_len) == 0) {
                    sctx->stack[2] = LVM_JSON_REPORT_LV_METADATA_LV;
                    return 1;
                }
                break;
            case 12:
                if (strncmp((const char *)key, "data_percent", key_len) == 0) {
                    sctx->stack[2] = LVM_JSON_REPORT_LV_DATA_PERCENT;
                    return 1;
                }
                break;
            case 16:
                if (strncmp((const char *)key, "lv_metadata_size", key_len) == 0) {
                    sctx->stack[2] = LVM_JSON_REPORT_LV_LV_METADATA_SIZE;
                    return 1;
                } else if (strncmp((const char *)key, "metadata_percent", key_len) == 0) {
                    sctx->stack[2] = LVM_JSON_REPORT_LV_METADATA_PERCENT;
                    return 1;
                }
                break;
            }
            break;
        default:
            break;
        }
        sctx->stack[2] = LVM_JSON_NONE;
        break;
    default:
        break;
    }

    return true;
}

static bool lvm_json_end_map(void *ctx)
{
    lvm_json_ctx_t *sctx = ctx;

    if (sctx->depth == 3) {
        switch (sctx->stack[1]) {
        case LVM_JSON_REPORT_VG:
            lvm_metrics_vg(sctx);
            break;
        case LVM_JSON_REPORT_LV:
            lvm_metrics_lv(sctx);
            break;
        default:
            break;
        }
    }

    if (sctx->depth > 0) {
        sctx->stack[sctx->depth] = LVM_JSON_NONE;
        sctx->depth--;
    } else {
        sctx->depth = 0;
    }

    return true;
}

static xson_callbacks_t lvm_json_callbacks = {
    .xson_null        = NULL,
    .xson_boolean     = NULL,
    .xson_integer     = NULL,
    .xson_double      = NULL,
    .xson_number      = lvm_json_number,
    .xson_string      = lvm_json_string,
    .xson_start_map   = lvm_json_start_map,
    .xson_map_key     = lvm_json_map_key,
    .xson_end_map     = lvm_json_end_map,
    .xson_start_array = NULL,
    .xson_end_array   = NULL,
};

static int lvm_read(void)
{
    char *argv[] = {
        "/usr/sbin/lvm", "fullreport",
        "--all",
        "--units=b",
        "--nosuffix",
        "--unbuffered",
        "--noheadings",
        "--reportformat", "json_std",
        "--configreport", "vg",
        "-o", "vg_name,vg_free,vg_size,snap_count,lv_count",
        "--configreport", "pv", "-S", "pv_uuid=",
        "--configreport", "lv",
        "-o", "vg_name,lv_name,lv_size,lv_attr,data_lv,metadata_lv,"
              "lv_metadata_size,data_percent,metadata_percent",
        "--configreport", "pvseg", "-S", "pv_uuid=",
        "--configreport", "seg", "-S", "lv_uuid=",
        NULL
    };

    cexec_t cexec = {
        .user = NULL,
        .group = NULL,
        .exec = argv[0],
        .argv = argv,
        .envp = NULL,
    };

    int fd_out, fd_err;

    int pid = exec_fork_child(&cexec, true, NULL, &fd_out, &fd_err);
    if (pid < 0)
        return -1;

    struct pollfd fds[2] = {{0}};
    fds[0].fd = fd_out;
    fds[0].events = POLLIN;
    fds[1].fd = fd_err;
    fds[1].events = POLLIN;

    lvm_json_ctx_t ctx = {0};
    json_parser_t handle = {0};

    json_parser_init(&handle, 0, &lvm_json_callbacks, &ctx);

    char buffer[4096];
    char buffer_err[4096];
    char *pbuffer_err = buffer_err;

    while (true) {
        int status = poll(fds, STATIC_ARRAY_SIZE(fds), -1);
        if (status < 0) {
            if (errno == EINTR)
                continue;
            break;
        }

        if (fds[0].revents & (POLLIN | POLLHUP)) {
            int len = read(fd_out, buffer, sizeof(buffer));
            if (len < 0) {
                if (errno == EAGAIN || errno == EINTR)
                    continue;
                break;
            } else if (len == 0) {
                break; /* We've reached EOF */
            }

            json_status_t jstatus = json_parser_parse(&handle, (unsigned char *)buffer, len);
            if (jstatus != JSON_STATUS_OK) {
                unsigned char *errmsg = json_parser_get_error(&handle, 0, NULL, 0);
                PLUGIN_ERROR("json_parse_complete failed: %s", (const char *)errmsg);
                json_parser_free_error(errmsg);

                if (fd_out >= 0)
                    close(fd_out);
                fd_out = -1;
                if (fd_err >= 0)
                    close(fd_err);
                fd_err = -1;
                break;
            }
        } else if (fds[0].revents & (POLLERR | POLLNVAL)) {
            PLUGIN_ERROR("Failed to read pipe from '%s'.", cexec.exec);
            break;
        } else if (fds[1].revents & (POLLIN | POLLHUP)) {
            int len = read(fd_err, pbuffer_err,
                                   sizeof(buffer_err) - 1 - (pbuffer_err - buffer_err));

            if (len < 0) {
                if (errno == EAGAIN || errno == EINTR)
                    continue;
                break;
            } else if (len == 0) {
                /* We've reached EOF */
                PLUGIN_DEBUG("Program '%s' has closed STDERR.", cexec.exec);

                /* Clean up file descriptor */
                if (fd_err >= 0)
                    close(fd_err);
                fd_err = -1;
                fds[1].fd = -1;
                fds[1].events = 0;
                continue;
            }

            pbuffer_err[len] = '\0';

            len += pbuffer_err - buffer_err;
            pbuffer_err = buffer_err;

            char *pnl;
            while ((pnl = strchr(pbuffer_err, '\n'))) {
                *pnl = '\0';
                if (*(pnl - 1) == '\r')
                    *(pnl - 1) = '\0';

                PLUGIN_ERROR("stderr: %s", pbuffer_err);

                pbuffer_err = ++pnl;
            }
            /* not completely read ? */
            if (pbuffer_err - buffer_err < len) {
                len -= pbuffer_err - buffer_err;
                memmove(buffer_err, pbuffer_err, len);
                pbuffer_err = buffer_err + len;
            } else {
                pbuffer_err = buffer_err;
            }
        } else if (fds[1].revents & (POLLERR | POLLNVAL)) {
            PLUGIN_WARNING("Ignoring STDERR for program '%s'.", cexec.exec);
            /* Clean up file descriptor */
            if ((fds[1].revents & POLLNVAL) == 0) {
                if (fd_err >= 0)
                    close(fd_err);
                fd_err = -1;
            }
            fds[1].fd = -1;
            fds[1].events = 0;
        }
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        PLUGIN_DEBUG("waitpid failed: %s", STRERRNO);
    }
    PLUGIN_DEBUG("Child %i exited with status %i.", (int)pid, status);

    if (fd_out >= 0)
        close(fd_out);
    if (fd_err >= 0)
        close(fd_err);

    json_status_t jstatus = json_parser_complete(&handle);
    if (jstatus != JSON_STATUS_OK) {
        unsigned char *errmsg = json_parser_get_error(&handle, 0, NULL, 0);
        PLUGIN_ERROR("json_parse_complete failed: %s", (const char *)errmsg);
        json_parser_free_error(errmsg);
        json_parser_free(&handle);
        return -1;
    }
    json_parser_free(&handle);

    plugin_dispatch_metric_family_array(fams, FAM_LVM_MAX, 0);

    return 0;
}

static int lvm_init(void)
{
#if defined(HAVE_SYS_CAPABILITY_H) && defined(CAP_SYS_ADMIN)
    if (plugin_check_capability(CAP_SYS_ADMIN) != 0) {
        if (getuid() == 0)
            PLUGIN_WARNING("lvm plugin: Running ncollectd as root, but the "
                           "CAP_SYS_ADMIN capability is missing. The plugin's read "
                           "function will probably fail. Is your init system dropping "
                           "capabilities?");
        else
            PLUGIN_WARNING("ncollectd doesn't have the CAP_SYS_ADMIN "
                           "capability. If you don't want to run collectd as root, try "
                           "running \"setcap cap_sys_admin=ep\" on the collectd binary.");
    }
#endif
    return 0;
}

void module_register(void)
{
    plugin_register_init("lvm", lvm_init);
    plugin_register_read("lvm", lvm_read);
}
