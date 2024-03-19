// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

enum {
    FAM_CIFS_CONNECTED,
    FAM_CIFS_SMB_SENT,
    FAM_CIFS_SMB1_OPLOCK_BREAK,
    FAM_CIFS_SMB1_READ,
    FAM_CIFS_SMB1_READ_BYTES,
    FAM_CIFS_SMB1_WRITE,
    FAM_CIFS_SMB1_WRITE_BYTES,
    FAM_CIFS_SMB1_FLUSHES,
    FAM_CIFS_SMB1_LOCKS,
    FAM_CIFS_SMB1_HARD_LINKS,
    FAM_CIFS_SMB1_SYM_LINKS,
    FAM_CIFS_SMB1_OPENS,
    FAM_CIFS_SMB1_CLOSES,
    FAM_CIFS_SMB1_DELETES,
    FAM_CIFS_SMB1_POSIX_OPENS,
    FAM_CIFS_SMB1_POSIX_MKDIRS,
    FAM_CIFS_SMB1_MKDIRS,
    FAM_CIFS_SMB1_RMDIRS,
    FAM_CIFS_SMB1_RENAMES,
    FAM_CIFS_SMB1_T2RENAMES,
    FAM_CIFS_SMB1_FIND_FIRST,
    FAM_CIFS_SMB1_FIND_NEXT,
    FAM_CIFS_SMB1_FIND_CLOSE,
    FAM_CIFS_SMB2_READ_BYTES,
    FAM_CIFS_SMB2_WRITTEN_BYTES,
    FAM_CIFS_SMB2_LOCAL_OPENS,
    FAM_CIFS_SMB2_REMOTE_OPENS,
    FAM_CIFS_SMB2_TREE_CONNECT,
    FAM_CIFS_SMB2_TREE_CONNECT_FAIL,
    FAM_CIFS_SMB2_TREE_DISCONNECT,
    FAM_CIFS_SMB2_TREE_DISCONNECT_FAIL,
    FAM_CIFS_SMB2_CREATE,
    FAM_CIFS_SMB2_CREATE_FAIL,
    FAM_CIFS_SMB2_CLOSE,
    FAM_CIFS_SMB2_CLOSE_FAIL,
    FAM_CIFS_SMB2_FLUSH,
    FAM_CIFS_SMB2_FLUSH_FAIL,
    FAM_CIFS_SMB2_READ,
    FAM_CIFS_SMB2_READ_FAIL,
    FAM_CIFS_SMB2_WRITE,
    FAM_CIFS_SMB2_WRITE_FAIL,
    FAM_CIFS_SMB2_LOCK,
    FAM_CIFS_SMB2_LOCK_FAIL,
    FAM_CIFS_SMB2_IOCTL,
    FAM_CIFS_SMB2_IOCTL_FAIL,
    FAM_CIFS_SMB2_QUERY_DIRECTORY,
    FAM_CIFS_SMB2_QUERY_DIRECTORY_FAIL,
    FAM_CIFS_SMB2_CHANGE_NOTIFY,
    FAM_CIFS_SMB2_CHANGE_NOTIFY_FAIL,
    FAM_CIFS_SMB2_QUERY_INFO,
    FAM_CIFS_SMB2_QUERY_INFO_FAIL,
    FAM_CIFS_SMB2_SET_INFO,
    FAM_CIFS_SMB2_SET_INFO_FAIL,
    FAM_CIFS_SMB2_OPLOCK_BREAK,
    FAM_CIFS_SMB2_OPLOCK_BREAK_FAIL,
    FAM_CIFS_MAX,
};

static metric_family_t fams[FAM_CIFS_MAX] = {
    [FAM_CIFS_CONNECTED] = {
        .name = "system_cifs_connected",
        .type = METRIC_TYPE_GAUGE,
        .help = "The connection status for each CIFS filesystem mounted",
    },
    [FAM_CIFS_SMB_SENT] = {
        .name = "system_cifs_smb_sent",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of CIFS server operations for each CIFS filesystem mounted.",
    },
    [FAM_CIFS_SMB1_OPLOCK_BREAK] = {
        .name = "system_cifs_smb1_oplock_break",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of operation lock breaks for each CIFS filesystem mounted.",
    },
    [FAM_CIFS_SMB1_READ] = {
        .name = "system_cifs_smb1_read",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of read operations for each CIFS filesystem mounted."
    },
    [FAM_CIFS_SMB1_READ_BYTES] = {
        .name = "system_cifs_smb1_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of read bytes for each CIFS filesystem mounted.",
    },
    [FAM_CIFS_SMB1_WRITE] = {
        .name = "system_cifs_smb1_write",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of write operations for each CIFS filesystem mounted.",
    },
    [FAM_CIFS_SMB1_WRITE_BYTES] = {
        .name = "system_cifs_smb1_write_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of written bytes for each CIFS filesystem mounted.",
    },
    [FAM_CIFS_SMB1_FLUSHES] = {
        .name = "system_cifs_smb1_flushes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total mumber of cache flushes for each CIFS filesystem mounted..",
    },
    [FAM_CIFS_SMB1_LOCKS] = {
        .name = "system_cifs_smb1_locks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of open locks for each CIFS filesystem mounted.",
    },
    [FAM_CIFS_SMB1_HARD_LINKS] = {
        .name = "system_cifs_smb1_hard_links",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of hard links created for each CIFS filesystem mounted.",
    },
    [FAM_CIFS_SMB1_SYM_LINKS] = {
        .name = "system_cifs_smb1_sym_links",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of symbolic links created for each CIFS filesystem mounted.",
    },
    [FAM_CIFS_SMB1_OPENS] = {
        .name = "system_cifs_smb1_opens",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of file open operations for each CIFS filesystem mounted.",
    },
    [FAM_CIFS_SMB1_CLOSES] = {
        .name = "system_cifs_smb1_closes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of file close operations for each CIFS filesystem mounted.",
    },
    [FAM_CIFS_SMB1_DELETES] = {
        .name = "system_cifs_smb1_deletes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of file delete operations for each CIFS filesystem mounted.",
    },
    [FAM_CIFS_SMB1_POSIX_OPENS] = {
        .name = "system_cifs_smb1_posix_opens",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of posix file open operations for each CIFS filesystem mounted.",
    },
    [FAM_CIFS_SMB1_POSIX_MKDIRS] = {
        .name = "system_cifs_smb1_posix_mkdirs",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of posix directory creation operations "
                "for each CIFS filesystem mounted.",
    },
    [FAM_CIFS_SMB1_MKDIRS] = {
        .name = "system_cifs_smb1_mkdirs",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of directory creation operations for each CIFS filesystem mounted.",
    },
    [FAM_CIFS_SMB1_RMDIRS] = {
        .name = "system_cifs_smb1_rmdirs",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of directory removal operations for each CIFS filesystem mounted.",
    },
    [FAM_CIFS_SMB1_RENAMES] = {
        .name = "system_cifs_smb1_renames",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of rename operations for each CIFS filesystem mounted.",
    },
    [FAM_CIFS_SMB1_T2RENAMES] = {
        .name = "system_cifs_smb1_t2renames",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of t2 rename operations for each CIFS filesystem mounted.",
    },
    [FAM_CIFS_SMB1_FIND_FIRST] = {
        .name = "system_cifs_smb1_find_first",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of FindFirst requests to the server.",
    },
    [FAM_CIFS_SMB1_FIND_NEXT] = {
        .name = "system_cifs_smb1_find_next",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of FindNext requests to the server.",
    },
    [FAM_CIFS_SMB1_FIND_CLOSE] = {
        .name = "system_cifs_smb1_find_close",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of FindClose requests to the server.",
    },
    [FAM_CIFS_SMB2_READ_BYTES] = {
        .name = "system_cifs_smb2_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of read bytes for each CIFS filesystem mounted.",
    },
    [FAM_CIFS_SMB2_WRITTEN_BYTES] = {
        .name = "system_cifs_smb2_written_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of written bytes for each CIFS filesystem mounted.",
    },
    [FAM_CIFS_SMB2_LOCAL_OPENS] = {
        .name = "system_cifs_smb2_local_opens",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_REMOTE_OPENS] = {
        .name = "system_cifs_smb2_remote_opens",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_TREE_CONNECT] = {
        .name = "system_cifs_smb2_tree_connect",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_TREE_CONNECT_FAIL] = {
        .name = "system_cifs_smb2_tree_connect_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_TREE_DISCONNECT] = {
        .name = "system_cifs_smb2_tree_disconnect",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_TREE_DISCONNECT_FAIL] = {
        .name = "system_cifs_smb2_tree_disconnect_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_CREATE] = {
        .name = "system_cifs_smb2_create",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_CREATE_FAIL] = {
        .name = "system_cifs_smb2_create_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_CLOSE] = {
        .name = "system_cifs_smb2_close",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_CLOSE_FAIL] = {
        .name = "system_cifs_smb2_close_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_FLUSH] = {
        .name = "system_cifs_smb2_flush",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_FLUSH_FAIL] = {
        .name = "system_cifs_smb2_flush_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_READ] = {
        .name = "system_cifs_smb2_read",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_READ_FAIL] = {
        .name = "system_cifs_smb2_read_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_WRITE] = {
        .name = "system_cifs_smb2_write",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_WRITE_FAIL] = {
        .name = "system_cifs_smb2_write_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_LOCK] = {
        .name = "system_cifs_smb2_lock",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_LOCK_FAIL] = {
        .name = "system_cifs_smb2_lock_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_IOCTL] = {
        .name = "system_cifs_smb2_ioctl",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_IOCTL_FAIL] = {
        .name = "system_cifs_smb2_ioctl_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_QUERY_DIRECTORY] = {
        .name = "system_cifs_smb2_query_directory",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_QUERY_DIRECTORY_FAIL] = {
        .name = "system_cifs_smb2_query_directory_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_CHANGE_NOTIFY] = {
        .name = "system_cifs_smb2_change_notify",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_CHANGE_NOTIFY_FAIL] = {
        .name = "system_cifs_smb2_change_notify_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_QUERY_INFO] = {
        .name = "system_cifs_smb2_query_info",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_QUERY_INFO_FAIL] = {
        .name = "system_cifs_smb2_query_info_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_SET_INFO] = {
        .name = "system_cifs_smb2_set_info",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_SET_INFO_FAIL] = {
        .name = "system_cifs_smb2_set_info_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_OPLOCK_BREAK] = {
        .name = "system_cifs_smb2_oplock_break",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_CIFS_SMB2_OPLOCK_BREAK_FAIL] = {
        .name = "system_cifs_smb2_oplock_break_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
};

static char *path_proc_cifs;

static int cifs_read(void)
{
    FILE *fh = fopen(path_proc_cifs, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Failed to open '%s'", path_proc_cifs);
        return -1;
    }

    char share[PATH_MAX];
    share[0] = '\0';
    char conn[24];
    conn[0] = '\0';
    char line[1024];
    while (fgets(line, sizeof(line), fh) != NULL) {
        if (line[0] == '\n')
            continue;

        if (isdigit(line[0])) {
            char *ptr = line;
            while(isdigit(*ptr)) ptr++;
            if (*ptr != ')')
                continue;
            *ptr = '\0';
            sstrncpy(conn, line, sizeof(conn));
            ptr++;

            if (*(ptr++) != ' ')
                continue;
            size_t len = strlen(ptr);
            if (len == 0)
                continue;
            len--;
            if (ptr[len] == '\n')
                ptr[len] = '\0';

            double connectd = 1;
            if (len >= strlen("\tDISCONNECTED ")) {
                if (strcmp(ptr + (len - strlen("\tDISCONNECTED ")), "\tDISCONNECTED ") == 0) {
                    len -= strlen("\tDISCONNECTED ");
                    ptr[len] = '\0';
                    connectd = 0;
                }
            }

            sstrncpy(share, ptr, sizeof(share));
            metric_family_append(&fams[FAM_CIFS_CONNECTED],
                                 VALUE_GAUGE(connectd), NULL,
                                 &(label_pair_const_t){.name="share", .value=share},
                                 &(label_pair_const_t){.name="connection", .value=conn}, NULL);
            continue;
        }

        if (share[0] == '\0')
            continue;

        char *fields[10];
        int fields_num = strsplit(line, fields, STATIC_ARRAY_SIZE(fields));

        if (fields_num < 2)
            continue;

        switch(line[0]) {
        case 'R':
            if (strcmp(fields[0], "Reads:") == 0) {
                if (fields_num == 4) {
                    /* Reads:  %d Bytes: %llu */
                    metric_family_append(&fams[FAM_CIFS_SMB1_READ],
                                         VALUE_COUNTER(atoull(fields[1])), NULL,
                                         &(label_pair_const_t){.name="share", .value=share},
                                         &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                    metric_family_append(&fams[FAM_CIFS_SMB1_READ_BYTES],
                                         VALUE_COUNTER(atoull(fields[3])), NULL,
                                         &(label_pair_const_t){.name="share", .value=share},
                                         &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                } else if (fields_num == 5) {
                    /* Reads: %d total %d failed */
                    metric_family_append(&fams[FAM_CIFS_SMB2_READ],
                                         VALUE_COUNTER(atoull(fields[1])), NULL,
                                         &(label_pair_const_t){.name="share", .value=share},
                                         &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                    metric_family_append(&fams[FAM_CIFS_SMB2_READ_FAIL],
                                         VALUE_COUNTER(atoull(fields[3])), NULL,
                                         &(label_pair_const_t){.name="share", .value=share},
                                         &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                }
            }  else if ((strcmp(fields[0], "Renames:") == 0) && (fields_num == 5)) {
                /* Renames: %d T2 Renames %d */
                metric_family_append(&fams[FAM_CIFS_SMB1_RENAMES],
                                     VALUE_COUNTER(atoull(fields[1])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                metric_family_append(&fams[FAM_CIFS_SMB1_T2RENAMES],
                                     VALUE_COUNTER(atoull(fields[4])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
            }
            break;
        case 'W':
            if (strcmp(fields[0], "Writes:") == 0) {
                if (fields_num == 4) {
                    /* Writes: %d Bytes: %llu */
                    metric_family_append(&fams[FAM_CIFS_SMB1_WRITE],
                                         VALUE_COUNTER(atoull(fields[1])), NULL,
                                         &(label_pair_const_t){.name="share", .value=share},
                                         &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                    metric_family_append(&fams[FAM_CIFS_SMB1_WRITE_BYTES],
                                         VALUE_COUNTER(atoull(fields[3])), NULL,
                                         &(label_pair_const_t){.name="share", .value=share},
                                         &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                } else if (fields_num == 5) {
                    /* Writes: %d total %d failed */
                    metric_family_append(&fams[FAM_CIFS_SMB2_WRITE],
                                         VALUE_COUNTER(atoull(fields[1])), NULL,
                                         &(label_pair_const_t){.name="share", .value=share},
                                         &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                    metric_family_append(&fams[FAM_CIFS_SMB2_WRITE_FAIL],
                                         VALUE_COUNTER(atoull(fields[3])), NULL,
                                         &(label_pair_const_t){.name="share", .value=share},
                                         &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                }
            }
            break;
        case 'F':
            if (strcmp(fields[0], "Flushes:") == 0) {
                if (fields_num == 2) {
                    /* Flushes: %d */
                    metric_family_append(&fams[FAM_CIFS_SMB1_FLUSHES],
                                         VALUE_COUNTER(atoull(fields[1])), NULL,
                                         &(label_pair_const_t){.name="share", .value=share},
                                         &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                } else if (fields_num == 5) {
                    /* Flushes: %d total %d failed */
                    metric_family_append(&fams[FAM_CIFS_SMB2_FLUSH],
                                         VALUE_COUNTER(atoull(fields[1])), NULL,
                                         &(label_pair_const_t){.name="share", .value=share},
                                         &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                    metric_family_append(&fams[FAM_CIFS_SMB2_FLUSH_FAIL],
                                         VALUE_COUNTER(atoull(fields[3])), NULL,
                                         &(label_pair_const_t){.name="share", .value=share},
                                         &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                }
            } else if ((strcmp(fields[0], "FindFirst:") == 0) && (fields_num == 5)) {
                /* FindFirst: %d FNext %d FClose %d */
                metric_family_append(&fams[FAM_CIFS_SMB1_FIND_FIRST],
                                     VALUE_COUNTER(atoull(fields[1])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                metric_family_append(&fams[FAM_CIFS_SMB1_FIND_NEXT],
                                     VALUE_COUNTER(atoull(fields[3])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                metric_family_append(&fams[FAM_CIFS_SMB1_FIND_CLOSE],
                                     VALUE_COUNTER(atoull(fields[5])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
            }
            break;
        case 'L':
            if (strcmp(line, "Locks:") == 0) {
                if (fields_num == 6) {
                    /* Locks: %d HardLinks: %d Symlinks: %d */
                    metric_family_append(&fams[FAM_CIFS_SMB1_LOCKS],
                                         VALUE_COUNTER(atoull(fields[1])), NULL,
                                         &(label_pair_const_t){.name="share", .value=share},
                                         &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                    metric_family_append(&fams[FAM_CIFS_SMB1_HARD_LINKS],
                                         VALUE_COUNTER(atoull(fields[3])), NULL,
                                         &(label_pair_const_t){.name="share", .value=share},
                                         &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                    metric_family_append(&fams[FAM_CIFS_SMB1_SYM_LINKS],
                                         VALUE_COUNTER(atoull(fields[5])), NULL,
                                         &(label_pair_const_t){.name="share", .value=share},
                                         &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                } else if (fields_num == 5) {
                    /* Locks: %d total %d failed */
                    metric_family_append(&fams[FAM_CIFS_SMB2_LOCK],
                                         VALUE_COUNTER(atoull(fields[1])), NULL,
                                         &(label_pair_const_t){.name="share", .value=share},
                                         &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                    metric_family_append(&fams[FAM_CIFS_SMB2_LOCK_FAIL],
                                         VALUE_COUNTER(atoull(fields[3])), NULL,
                                         &(label_pair_const_t){.name="share", .value=share},
                                         &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                }
            }
            break;
        case 'O':
            if ((strcmp(fields[0], "Opens:") == 0) && (fields_num == 6)) {
                /* Opens: %d Closes: %d Deletes: %d */
                metric_family_append(&fams[FAM_CIFS_SMB1_OPENS],
                                     VALUE_COUNTER(atoull(fields[1])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                metric_family_append(&fams[FAM_CIFS_SMB1_CLOSES],
                                     VALUE_COUNTER(atoull(fields[3])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                metric_family_append(&fams[FAM_CIFS_SMB1_DELETES],
                                     VALUE_COUNTER(atoull(fields[5])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
            } else if ((strcmp(fields[0], "Open") == 0) && (fields_num == 9)) {
                /* Open files: %d total (local), %d open on server */
                metric_family_append(&fams[FAM_CIFS_SMB2_LOCAL_OPENS],
                                     VALUE_COUNTER(atoull(fields[2])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                metric_family_append(&fams[FAM_CIFS_SMB2_REMOTE_OPENS],
                                     VALUE_COUNTER(atoull(fields[5])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
            } else if ((strcmp(fields[0], "OplockBreaks:") == 0) && (fields_num == 5)) {
                /* OplockBreaks: %d sent %d failed */
                metric_family_append(&fams[FAM_CIFS_SMB2_OPLOCK_BREAK],
                                     VALUE_COUNTER(atoull(fields[1])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                metric_family_append(&fams[FAM_CIFS_SMB2_OPLOCK_BREAK_FAIL],
                                     VALUE_COUNTER(atoull(fields[3])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
            }
            break;
        case 'P':
            if ((strcmp(fields[0], "Posix") == 0) && (fields_num == 6)) {
                /* Posix Opens: %d Posix Mkdirs: %d */
                metric_family_append(&fams[FAM_CIFS_SMB1_POSIX_OPENS],
                                     VALUE_COUNTER(atoull(fields[2])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                metric_family_append(&fams[FAM_CIFS_SMB1_POSIX_MKDIRS],
                                     VALUE_COUNTER(atoull(fields[5])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
            }
            break;
        case 'M':
            if ((strcmp(fields[0], "Mkdirs:") == 0) && (fields_num == 4)) {
                /* Mkdirs: %d Rmdirs: %d */
                metric_family_append(&fams[FAM_CIFS_SMB1_MKDIRS],
                                     VALUE_COUNTER(atoull(fields[1])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                metric_family_append(&fams[FAM_CIFS_SMB1_RMDIRS],
                                     VALUE_COUNTER(atoull(fields[3])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
            }
            break;
        case 'B':
            if ((strcmp(fields[0], "Bytes") == 0) && (fields_num == 6)) {
                /* Bytes read: %llu  Bytes written: %llu */
                metric_family_append(&fams[FAM_CIFS_SMB2_READ_BYTES],
                                     VALUE_COUNTER(atoull(fields[2])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                metric_family_append(&fams[FAM_CIFS_SMB2_WRITTEN_BYTES],
                                     VALUE_COUNTER(atoull(fields[5])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
            }
            break;
        case 'T':
            if ((strcmp(fields[0], "TreeConnects:") == 0) && (fields_num == 5)) {
                /* TreeConnects: %d total %d failed */
                metric_family_append(&fams[FAM_CIFS_SMB2_TREE_CONNECT],
                                     VALUE_COUNTER(atoull(fields[1])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                metric_family_append(&fams[FAM_CIFS_SMB2_TREE_CONNECT_FAIL],
                                     VALUE_COUNTER(atoull(fields[3])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
            } else if ((strcmp(fields[0], "TreeDisconnects:") == 0) && (fields_num == 5)) {
                /* TreeDisconnects: %d total %d failed */
                metric_family_append(&fams[FAM_CIFS_SMB2_TREE_DISCONNECT],
                                     VALUE_COUNTER(atoull(fields[1])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                metric_family_append(&fams[FAM_CIFS_SMB2_TREE_DISCONNECT_FAIL],
                                     VALUE_COUNTER(atoull(fields[3])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
            }
            break;
        case 'C':
            if ((strcmp(fields[0], "Creates:") == 0) && (fields_num == 5)) {
                /* Creates: %d total %d failed */
                metric_family_append(&fams[FAM_CIFS_SMB2_CREATE],
                                     VALUE_COUNTER(atoull(fields[1])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                metric_family_append(&fams[FAM_CIFS_SMB2_CREATE_FAIL],
                                     VALUE_COUNTER(atoull(fields[3])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
            } else if ((strcmp(fields[0], "Closes:") == 0) && (fields_num == 5)) {
                /* Closes: %d total %d failed */
                metric_family_append(&fams[FAM_CIFS_SMB2_CLOSE],
                                     VALUE_COUNTER(atoull(fields[1])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                metric_family_append(&fams[FAM_CIFS_SMB2_CLOSE_FAIL],
                                     VALUE_COUNTER(atoull(fields[3])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
            } else if ((strcmp(fields[0], "ChangeNotifies:") == 0) && (fields_num == 5)) {
                /* ChangeNotifies: %d total %d failed */
                metric_family_append(&fams[FAM_CIFS_SMB2_CHANGE_NOTIFY],
                                     VALUE_COUNTER(atoull(fields[1])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                metric_family_append(&fams[FAM_CIFS_SMB2_CHANGE_NOTIFY_FAIL],
                                     VALUE_COUNTER(atoull(fields[3])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
            }
            break;
        case 'I':
            if ((strcmp(fields[0], "IOCTLs:") == 0) && (fields_num == 5)) {
                /* IOCTLs: %d total %d failed */
                metric_family_append(&fams[FAM_CIFS_SMB2_IOCTL],
                                     VALUE_COUNTER(atoull(fields[1])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                metric_family_append(&fams[FAM_CIFS_SMB2_IOCTL_FAIL],
                                     VALUE_COUNTER(atoull(fields[3])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
            }
            break;
        case 'Q':
            if ((strcmp(fields[0], "QueryDirectories:") == 0) && (fields_num == 5)) {
                /* QueryDirectories: %d total %d failed */
                metric_family_append(&fams[FAM_CIFS_SMB2_QUERY_DIRECTORY],
                                     VALUE_COUNTER(atoull(fields[1])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                metric_family_append(&fams[FAM_CIFS_SMB2_QUERY_DIRECTORY_FAIL],
                                     VALUE_COUNTER(atoull(fields[3])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
            } else if ((strcmp(fields[0], "QueryInfos:") == 0) && (fields_num == 5)) {
                /* QueryInfos: %d total %d failed */
                metric_family_append(&fams[FAM_CIFS_SMB2_QUERY_INFO],
                                     VALUE_COUNTER(atoull(fields[1])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                metric_family_append(&fams[FAM_CIFS_SMB2_QUERY_INFO_FAIL],
                                     VALUE_COUNTER(atoull(fields[3])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
            }
            break;
        case 'S':
            if ((strcmp(fields[0], "SetInfos:") == 0)  && (fields_num == 5)) {
                /* SetInfos: %d total %d failed */
                metric_family_append(&fams[FAM_CIFS_SMB2_SET_INFO],
                                     VALUE_COUNTER(atoull(fields[1])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                metric_family_append(&fams[FAM_CIFS_SMB2_SET_INFO_FAIL],
                                     VALUE_COUNTER(atoull(fields[3])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
            } else if ((strcmp(fields[0], "SMBs:") == 0)  && (fields_num >= 2)) {
                /* SMBs: %d */
                metric_family_append(&fams[FAM_CIFS_SMB_SENT],
                                     VALUE_COUNTER(atoull(fields[1])), NULL,
                                     &(label_pair_const_t){.name="share", .value=share},
                                     &(label_pair_const_t){.name="connection", .value=conn}, NULL);
                /* SMBs: %d Oplocks breaks: %d */
                if (fields_num == 5)
                    metric_family_append(&fams[FAM_CIFS_SMB1_OPLOCK_BREAK],
                                         VALUE_COUNTER(atoull(fields[4])), NULL,
                                         &(label_pair_const_t){.name="share", .value=share},
                                         &(label_pair_const_t){.name="connection", .value=conn}, NULL);
            }
            break;
        }
    }
    fclose(fh);

    plugin_dispatch_metric_family_array(fams, FAM_CIFS_MAX, 0);

    return 0;
}

static int cifs_init(void)
{
    path_proc_cifs = plugin_procpath("fs/cifs/Stats");
    if (path_proc_cifs == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int cifs_shutdown(void)
{
    free(path_proc_cifs);
    return 0;
}

void module_register(void)
{
    plugin_register_init("cifs", cifs_init);
    plugin_register_read("cifs", cifs_read);
    plugin_register_shutdown("cifs", cifs_shutdown);
}
