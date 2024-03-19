// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2014 Tim Laszlo
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Tim Laszlo <tim.laszlo at gmail.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

/*
 See: http://www.drbd.org/users-guide/ch-admin.html#s-performance-indicators

 version: 8.3.11 (api:88/proto:86-96)
 srcversion: 71955441799F513ACA6DA60
    0: cs:Connected ro:Primary/Secondary ds:UpToDate/UpToDate B r-----
    ns:64363752 nr:0 dw:357799284 dr:846902273 al:34987022 bm:18062 lo:0 \
    pe:0 ua:0 ap:0 ep:1 wo:f oos:0
 */

#include "plugin.h"
#include "libutils/common.h"

static char *path_proc_drbd;

enum {
    FAM_DRBD_CONNECTED = 0,
    FAM_DRDB_NODE_ROLE_IS_PRIMARY,
    FAM_DRDB_DISK_STATE_IS_UP_TO_DATE,
    FAM_DRDB_NETWORK_SENT_BYTES,
    FAM_DRDB_NETWORK_RECEIVED_BYTES,
    FAM_DRDB_DISK_WRITTEN_BYTES,
    FAM_DRDB_DISK_READ_BYTES,
    FAM_DRDB_ACTIVITYLOG_WRITES,
    FAM_DRDB_BITMAP_WRITES,
    FAM_DRDB_LOCAL_PENDING,
    FAM_DRDB_REMOTE_PENDING,
    FAM_DRDB_REMOTE_UNACKNOWLEDGED,
    FAM_DRDB_APPLICATION_PENDING,
    FAM_DRDB_EPOCHS,
    FAM_DRDB_OUT_OF_SYNC_BYTES,
    FAM_DRDB_MAX,
};

static metric_family_t fams[FAM_DRDB_MAX] = {
    [FAM_DRBD_CONNECTED] = {
        .name = "system_drbd_connected",
        .type = METRIC_TYPE_GAUGE,
        .help = "Whether DRBD is connected to the peer.",
    },
    [FAM_DRDB_NODE_ROLE_IS_PRIMARY] = {
        .name = "system_drdb_node_role_is_primary",
        .type = METRIC_TYPE_GAUGE,
        .help = "Whether the role of the node is in the primary state.",
    },
    [FAM_DRDB_DISK_STATE_IS_UP_TO_DATE] = {
        .name = "system_drdb_disk_state_is_up_to_date",
        .type = METRIC_TYPE_GAUGE,
        .help = "Whether the disk of the node is up to date.",
    },
    [FAM_DRDB_NETWORK_SENT_BYTES] = {
        .name = "system_drdb_network_sent_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of bytes sent via the network.",
    },
    [FAM_DRDB_NETWORK_RECEIVED_BYTES] = {
        .name = "system_drdb_network_received_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of bytes received via the network.",
    },
    [FAM_DRDB_DISK_WRITTEN_BYTES] = {
        .name = "system_drdb_disk_written_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Net data written on local hard disk; in bytes.",
    },
    [FAM_DRDB_DISK_READ_BYTES] = {
        .name = "system_drdb_disk_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Net data read from local hard disk; in bytes.",
    },
    [FAM_DRDB_ACTIVITYLOG_WRITES] = {
        .name = "system_drdb_activitylog_writes",
        .help = "Number of updates of the activity log area of the meta data.",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_DRDB_BITMAP_WRITES] = {
        .name = "system_drdb_bitmap_writes",
        .help = "Number of updates of the bitmap area of the meta data.",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_DRDB_LOCAL_PENDING] = {
        .name = "system_drdb_local_pending",
        .help = "Number of open requests to the local I/O sub-system.",
        .type = METRIC_TYPE_GAUGE,
    },
    [FAM_DRDB_REMOTE_PENDING] = {
        .name = "system_drdb_remote_pending",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of requests sent to the peer, "
                "but that have not yet been answered by the latter.",
    },
    [FAM_DRDB_REMOTE_UNACKNOWLEDGED] = {
        .name = "system_drdb_remote_unacknowledged",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of requests received by the peer via the network connection, "
                "but that have not yet been answered.",
    },
    [FAM_DRDB_APPLICATION_PENDING] = {
        .name = "system_drdb_application_pending",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of block I/O requests forwarded to DRBD, but not yet answered by DRBD.",
    },
    [FAM_DRDB_EPOCHS] = {
        .name = "system_drdb_epochs",
        .help = "Number of Epochs currently on the fly.",
        .type = METRIC_TYPE_GAUGE,
    },
    [FAM_DRDB_OUT_OF_SYNC_BYTES] = {
        .name = "system_drdb_out_of_sync_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of data known to be out of sync; in bytes.",
    },
};

typedef struct {
    char *field;
    size_t field_size;
    int fam_num;
} drbd_fam_t;

static drbd_fam_t drbd_fams[] = {
    { "ns:",  3, FAM_DRDB_NETWORK_SENT_BYTES     },
    { "nr:",  3, FAM_DRDB_NETWORK_RECEIVED_BYTES },
    { "dw:",  3, FAM_DRDB_DISK_WRITTEN_BYTES     },
    { "dr:",  3, FAM_DRDB_DISK_READ_BYTES        },
    { "al:",  3, FAM_DRDB_ACTIVITYLOG_WRITES     },
    { "bm:",  3, FAM_DRDB_BITMAP_WRITES          },
    { "lo:",  3, FAM_DRDB_LOCAL_PENDING          },
    { "pe:",  3, FAM_DRDB_REMOTE_PENDING         },
    { "ua:",  3, FAM_DRDB_REMOTE_UNACKNOWLEDGED  },
    { "ap:",  3, FAM_DRDB_APPLICATION_PENDING    },
    { "ep:",  3, FAM_DRDB_EPOCHS                 },
    { "oos:", 4, FAM_DRDB_OUT_OF_SYNC_BYTES      },
};

static size_t drbd_fams_num = STATIC_ARRAY_SIZE(drbd_fams);

static int drbd_metrics(long int resource, char **fields, size_t fields_num)
{
    if (resource < 0) {
        PLUGIN_WARNING("Unable to parse resource");
        return -1;
    }

    char device[64];
    ssnprintf(device, sizeof(device), "r%ld", resource);

    for (size_t i = 0; i < fields_num; i++) {
        char *field = fields[i];
        size_t field_size = strlen(fields[i]);

        metric_family_t *fam = NULL;
        uint64_t value = 0;

        for (size_t j = 0; j < drbd_fams_num; j++) {
            if (drbd_fams[j].field == NULL)
                continue;
            if (drbd_fams[j].field_size >= field_size)
                continue;
            if (strncmp(drbd_fams[j].field, field, drbd_fams[j].field_size) != 0)
                continue;

            char *data = field + drbd_fams[j].field_size;
            if (*data == '\0')
                break;

            (void)parse_uinteger(data, &value);
            fam = &fams[drbd_fams[j].fam_num];
            break;
        }

        if (fam != NULL) {
            value_t val = {0};
            if (fam->type == METRIC_TYPE_COUNTER) {
                val = VALUE_COUNTER(value);
            } else {
                val = VALUE_GAUGE(value);
            }

            metric_family_append(fam, val, NULL,
                                 &(label_pair_const_t){.name="device", .value=device}, NULL);
        }
    }

    return 0;
}

static int drbd_status(long int resource, char **fields, size_t fields_num)
{
    if (resource < 0) {
        PLUGIN_WARNING("Unable to parse resource");
        return -1;
    }

    if (fields_num < 4) {
        PLUGIN_WARNING("Wrong number of fields");
        return -1;
    }

    char device[64];
    ssnprintf(device, sizeof(device), "r%ld", resource);

    if (strncmp("cs", fields[1], 2) == 0) {
        char *data = strchr(fields[1], ':');
        if (data != NULL) {
            data++;
            double value = 0;
            if (strncmp(data, "Connected", strlen("Connected")) == 0)
                value = 1;
            metric_family_append(&fams[FAM_DRBD_CONNECTED], VALUE_GAUGE(value), NULL,
                                 &(label_pair_const_t){.name="device", .value=device}, NULL);
        }
    }

    if (strncmp("ro", fields[2], 2) == 0) {
        char *data = strchr(fields[2], ':');
        if (data != NULL) {
            data++;
            double value = 0;
            if (strncmp(data, "Primary", strlen("Primary")) == 0)
                value = 1;
            metric_family_append(&fams[FAM_DRDB_NODE_ROLE_IS_PRIMARY], VALUE_GAUGE(value), NULL,
                                 &(label_pair_const_t){.name="device", .value=device}, NULL);
        }
    }

    if (strncmp("ds", fields[3], 2) == 0) {
        char *data = strchr(fields[3], ':');
        if (data != NULL) {
            data++;
            double value = 0;
            if (strncmp(data, "UpToDate", strlen("UpToDate")) == 0)
                value = 1;
            metric_family_append(&fams[FAM_DRDB_DISK_STATE_IS_UP_TO_DATE], VALUE_GAUGE(value), NULL,
                                 &(label_pair_const_t){.name="device", .value=device}, NULL);
        }
    }

    return 0;
}

static int drbd_read(void)
{
    FILE *fh = fopen(path_proc_drbd, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Unable to open %s", path_proc_drbd);
        return -1;
    }

    char buffer[256];
    long int resource = -1;
    char *fields[16];

    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (fields_num < 3)
            continue;

        /* ignore headers (first two iterations) */
        if ((strcmp(fields[0], "version:") == 0) ||
            (strcmp(fields[0], "srcversion:") == 0) ||
            (strcmp(fields[0], "GIT-hash:") == 0) ||
            (strcmp(fields[1], "sync'ed:") == 0) ||
            (strcmp(fields[0], "finish:") == 0)) {
            continue;
        }

        if (isdigit(fields[0][0])) {
            /* parse the resource line, next loop iteration
                 will submit values for this resource */
            resource = strtol(fields[0], NULL, 10);
            drbd_status(resource, fields, fields_num);
        } else {
            /* handle stats data for the resource defined in the
                 previous iteration */
            drbd_metrics(resource, fields, fields_num);
            resource = -1;
        }
    }
    fclose(fh);

    plugin_dispatch_metric_family_array(fams, FAM_DRDB_MAX, 0);
    return 0;
}

static int drbd_init(void)
{
    path_proc_drbd = plugin_procpath("drbd");
    if (path_proc_drbd == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int drbd_shutdown(void)
{
    free(path_proc_drbd);
    return 0;
}

void module_register(void)
{
    plugin_register_init("drbd", drbd_init);
    plugin_register_read("drbd", drbd_read);
    plugin_register_shutdown("drbd", drbd_shutdown);
}
