// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2008 Doug MacEachern
// SPDX-FileCopyrightText: Copyright (C) 2009,2010 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2009 Marco Chiappero
// SPDX-FileCopyrightText: Copyright (C) 2009 Fabian Schuh
// SPDX-FileCopyrightText: Copyright (C) 2017-2020 Pavel Rochnyak
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Doug MacEachern <dougm at hyperic.com>
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Marco Chiappero <marco at absence.it>
// SPDX-FileContributor: Fabian Schuh <mail at xeroc.org>
// SPDX-FileContributor: Pavel Rochnyak <pavel2000 ngs.ru>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "config.h"

#if defined(STRPTIME_NEEDS_STANDARDS) || defined(HAVE_STRPTIME_STANDARDS)
#ifndef _ISOC99_SOURCE
#define _ISOC99_SOURCE 1
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif
#endif

#include "plugin.h"
#include "libutils/common.h"

/**
 * There is two main kinds of OpenVPN status file:
 * - for 'single' mode (point-to-point or client mode)
 * - for 'multi' mode  (server with multiple clients)
 *
 * For 'multi' there is 3 versions of status file format:
 * - version 1 - First version of status file: without line type tokens,
 *   comma delimited for easy machine parsing. Currently used by default.
 *   Added in openvpn-2.0-beta3.
 * - version 2 - with line type tokens, with 'HEADER' line type, uses comma
 *   as a delimiter.
 *   Added in openvpn-2.0-beta15.
 * - version 3 - The only difference from version 2 is delimiter: in version 3
 *   tabs are used instead of commas. Set of fields is the same.
 *   Added in openvpn-2.1_rc14.
 *
 * For versions 2/3 there may be different sets of fields in different
 * OpenVPN versions.
 *
 * Versions 2.0, 2.1, 2.2:
 * Common Name,Real Address,Virtual Address,
 * Bytes Received,Bytes Sent,Connected Since,Connected Since (time_t)
 *
 * Version 2.3:
 * Common Name,Real Address,Virtual Address,
 * Bytes Received,Bytes Sent,Connected Since,Connected Since (time_t),Username
 *
 * Version 2.4:
 * Common Name,Real Address,Virtual Address,Virtual IPv6 Address,
 * Bytes Received,Bytes Sent,Connected Since,Connected Since (time_t),Username,
 * Client ID,Peer ID
 *
 * Current Collectd code tries to handle changes in this field set,
 * if they are backward-compatible.
 **/

#define TITLE_SINGLE "OpenVPN STATISTICS\n"
#define TITLE_V1 "OpenVPN CLIENT LIST\n"
#define TITLE_V2 "TITLE,"
#define TITLE_V3 "TITLE\t"
#define V1HEADER "Common Name,Real Address,Bytes Received,Bytes Sent,Connected Since\n"

enum {
    FAM_OPENVPN_UPDATED,
    FAM_OPENVPN_TUN_TAP_READ_BYTES,
    FAM_OPENVPN_TUN_TAP_WRITE_BYTES,
    FAM_OPENVPN_TCP_UDP_READ_BYTES,
    FAM_OPENVPN_TCP_UDP_WRITE_BYTES,
    FAM_OPENVPN_AUTH_READ_BYTES,
    FAM_OPENVPN_PRE_COMPRESS_BYTES,
    FAM_OPENVPN_POST_COMPRESS_BYTES,
    FAM_OPENVPN_PRE_DECOMPRESS_BYTES,
    FAM_OPENVPN_POST_DECOMPRESS_BYTES,
    FAM_OPENVPN_TUN_READ_TRUNCATIONS,
    FAM_OPENVPN_TUN_WRITE_TRUNCATIONS,
    FAM_OPENVPN_PRE_ENCRYPT_TRUNCATIONS,
    FAM_OPENVPN_POST_DECRYPT_TRUNCATIONS,
    FAM_OPENVPN_CONNECTIONS,
    FAM_OPENVPN_USER_RECEIVED_BYTES,
    FAM_OPENVPN_USER_SENT_BYTES,
    FAM_OPENVPN_USER_CONNECTED_SINCE,
    FAM_OPENVPN_MAX,
};

static metric_family_t fams[FAM_OPENVPN_MAX] = {
    [FAM_OPENVPN_UPDATED] = {
        .name = "openvpn_updated",
        .type = METRIC_TYPE_GAUGE,
        .help = "Unix timestamp when the data was updated.",
    },
    [FAM_OPENVPN_TUN_TAP_READ_BYTES] = {
        .name = "openvpn_tun_tap_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total amount of TUN/TAP traffic read, in bytes.",
    },
    [FAM_OPENVPN_TUN_TAP_WRITE_BYTES] = {
        .name = "openvpn_tun_tap_write_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total amount of TUN/TAP traffic written, in bytes.",
    },
    [FAM_OPENVPN_TCP_UDP_READ_BYTES] = {
        .name = "openvpn_tcp_udp_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total amount of TCP/UDP traffic read, in bytes.",
    },
    [FAM_OPENVPN_TCP_UDP_WRITE_BYTES] = {
        .name = "openvpn_tcp_udp_write_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total amount of TCP/UDP traffic written, in bytes.",
    },
    [FAM_OPENVPN_AUTH_READ_BYTES] = {
        .name = "openvpn_auth_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total amount of authentication traffic read, in bytes.",
    },
    [FAM_OPENVPN_PRE_COMPRESS_BYTES] = {
        .name = "openvpn_pre_compress_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total amount of data before compression, in bytes.",
    },
    [FAM_OPENVPN_POST_COMPRESS_BYTES] = {
        .name = "openvpn_post_compress_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total amount of data after compression, in bytes.",
    },
    [FAM_OPENVPN_PRE_DECOMPRESS_BYTES] = {
        .name = "openvpn_pre_decompress_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total amount of data before decompression, in bytes.",
    },
    [FAM_OPENVPN_POST_DECOMPRESS_BYTES] = {
        .name = "openvpn_post_decompress_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total amount of data after decompression, in bytes.",
    },
    [FAM_OPENVPN_TUN_READ_TRUNCATIONS] = {
        .name = "openvpn_tun_read_truncations",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_OPENVPN_TUN_WRITE_TRUNCATIONS] = {
        .name = "openvpn_tun_write_truncations",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_OPENVPN_PRE_ENCRYPT_TRUNCATIONS] = {
        .name = "openvpn_pre_encrypt_truncations",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_OPENVPN_POST_DECRYPT_TRUNCATIONS] = {
        .name = "openvpn_post_decrypt_truncations",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_OPENVPN_CONNECTIONS] = {
        .name = "openvpn_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Currently connected clients",
    },
    [FAM_OPENVPN_USER_RECEIVED_BYTES] = {
        .name = "openvpn_user_received_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total bytes received via the connection",
    },
    [FAM_OPENVPN_USER_SENT_BYTES] = {
        .name = "openvpn_user_sent_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total bytes sent via the connection",
    },
    [FAM_OPENVPN_USER_CONNECTED_SINCE] = {
        .name = "openvpn_user_connected_since",
        .type = METRIC_TYPE_GAUGE,
        .help = "Unix timestamp when the connection was established",
    },
};

typedef struct {
    char *instance;
    char *file;
    label_set_t labels;
    plugin_filter_t *filter;
    metric_family_t fams[FAM_OPENVPN_MAX];
} openvpn_instance_t;

/* Helper function
 * copy-n-pasted from common.c - changed delim to ",\t"  */
static int openvpn_strsplit(char *string, char **fields, size_t size)
{
    size_t i = 0;
    char *ptr = string;
    char *saveptr = NULL;

    while ((fields[i] = strtok_r(ptr, ",\t", &saveptr)) != NULL) {
        ptr = NULL;
        i++;

        if (i >= size)
            break;
    }

    return i;
}

static void openvpn_free(void *arg)
{
    openvpn_instance_t *oi = arg;

    free(oi->instance);
    free(oi->file);
    label_set_reset(&oi->labels);
    plugin_filter_free(oi->filter);
    free(oi);
}

static int single_read(openvpn_instance_t *oi, FILE *fh)
{
    char buffer[1024];
    char *fields[4];
    const int max_fields = STATIC_ARRAY_SIZE(fields);

    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        int fields_num = openvpn_strsplit(buffer, fields, max_fields);

        /* status file is generated by openvpn/sig.c:print_status()
         * http://svn.openvpn.net/projects/openvpn/trunk/openvpn/sig.c
         *
         * The line we're expecting has 2 fields. We ignore all lines
         *  with more or less fields.
         */
        if (fields_num != 2) {
            continue;
        }

        if (strcmp(fields[0], "Updated") == 0) {
            struct tm tm;
            strptime(fields[1], "%Y-%m-%d %H:%M:%S", &tm);
            metric_family_append(&oi->fams[FAM_OPENVPN_UPDATED],
                                 VALUE_GAUGE((double) mktime(&tm)), &oi->labels, NULL);
        } else if (strcmp(fields[0], "TUN/TAP read bytes") == 0) {
            /* read from the system and sent over the tunnel */
            metric_family_append(&oi->fams[FAM_OPENVPN_TUN_TAP_READ_BYTES],
                                 VALUE_COUNTER(atoll(fields[1])), &oi->labels, NULL);
        } else if (strcmp(fields[0], "TUN/TAP write bytes") == 0) {
            /* read from the tunnel and written in the system */
            metric_family_append(&oi->fams[FAM_OPENVPN_TUN_TAP_WRITE_BYTES],
                                 VALUE_COUNTER(atoll(fields[1])), &oi->labels, NULL);
        } else if (strcmp(fields[0], "TCP/UDP read bytes") == 0) {
            metric_family_append(&oi->fams[FAM_OPENVPN_TCP_UDP_READ_BYTES],
                                 VALUE_COUNTER(atoll(fields[1])), &oi->labels, NULL);
        } else if (strcmp(fields[0], "TCP/UDP write bytes") == 0) {
            metric_family_append(&oi->fams[FAM_OPENVPN_TCP_UDP_WRITE_BYTES],
                                 VALUE_COUNTER(atoll(fields[1])), &oi->labels, NULL);
        } else if (strcmp(fields[0], "pre-compress bytes") == 0) {
            metric_family_append(&oi->fams[FAM_OPENVPN_PRE_COMPRESS_BYTES],
                                 VALUE_COUNTER(atoll(fields[1])), &oi->labels, NULL);
        } else if (strcmp(fields[0], "post-compress bytes") == 0) {
            metric_family_append(&oi->fams[FAM_OPENVPN_POST_COMPRESS_BYTES],
                                 VALUE_COUNTER(atoll(fields[1])), &oi->labels, NULL);
        } else if (strcmp(fields[0], "pre-decompress bytes") == 0) {
            metric_family_append(&oi->fams[FAM_OPENVPN_PRE_DECOMPRESS_BYTES],
                                 VALUE_COUNTER(atoll(fields[1])), &oi->labels, NULL);
        } else if (strcmp(fields[0], "post-decompress bytes") == 0) {
            metric_family_append(&oi->fams[FAM_OPENVPN_POST_DECOMPRESS_BYTES],
                                 VALUE_COUNTER(atoll(fields[1])), &oi->labels, NULL);
        } else if (strcmp(fields[0], "Auth read bytes") == 0) {
            metric_family_append(&oi->fams[FAM_OPENVPN_AUTH_READ_BYTES],
                                 VALUE_COUNTER(atoll(fields[1])), &oi->labels, NULL);
        } else if (strcmp(fields[0], "TUN read truncations") == 0) {
            metric_family_append(&oi->fams[FAM_OPENVPN_TUN_READ_TRUNCATIONS],
                                 VALUE_COUNTER(atoll(fields[1])), &oi->labels, NULL);
        } else if (strcmp(fields[0], "TUN write truncations") == 0) {
            metric_family_append(&oi->fams[FAM_OPENVPN_TUN_WRITE_TRUNCATIONS],
                                 VALUE_COUNTER(atoll(fields[1])), &oi->labels, NULL);
        } else if (strcmp(fields[0], "Pre-encrypt truncations") == 0) {
            metric_family_append(&oi->fams[FAM_OPENVPN_PRE_ENCRYPT_TRUNCATIONS],
                                 VALUE_COUNTER(atoll(fields[1])), &oi->labels, NULL);
        } else if (strcmp(fields[0], "Post-decrypt truncations") == 0) {
            metric_family_append(&oi->fams[FAM_OPENVPN_PRE_ENCRYPT_TRUNCATIONS],
                                 VALUE_COUNTER(atoll(fields[1])), &oi->labels, NULL);
        }
    }

    return 0;
}

/* for reading status version 1 */
static int multi1_read(openvpn_instance_t *oi, FILE *fh)
{
    char buffer[1024];
    char *fields[10];
    const int max_fields = STATIC_ARRAY_SIZE(fields);
    long long sum_users = 0;
    bool found_header = false;

    /* read the file until the "ROUTING TABLE" line is found (no more info after)
     */
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        if (strcmp(buffer, "ROUTING TABLE\n") == 0)
            break;

        if (strncmp(buffer, "Updated,", strlen("Updated,")) == 0) {
            struct tm tm;
            strptime(buffer + strlen("Updated,"), "%Y-%m-%d %H:%M:%S", &tm);
            metric_family_append(&oi->fams[FAM_OPENVPN_UPDATED],
                                 VALUE_GAUGE((double) mktime(&tm)), &oi->labels, NULL);
            continue;
        }

        if (strcmp(buffer, V1HEADER) == 0) {
            found_header = true;
            continue;
        }

        /* skip the first lines until the client list section is found */
        if (found_header == false)
            /* we can't start reading data until this string is found */
            continue;

        int fields_num = openvpn_strsplit(buffer, fields, max_fields);
        if (fields_num < 4)
            continue;

        metric_family_append(&oi->fams[FAM_OPENVPN_USER_RECEIVED_BYTES],
                             VALUE_COUNTER(atoll(fields[2])), &oi->labels,
                              &(label_pair_const_t){.name="common_name", .value=fields[0]}, NULL);
        metric_family_append(&oi->fams[FAM_OPENVPN_USER_SENT_BYTES],
                             VALUE_COUNTER(atoll(fields[3])), &oi->labels,
                             &(label_pair_const_t){.name="common_name", .value=fields[0]}, NULL);

        struct tm tm;
        strptime(fields[4], "%Y-%m-%d %H:%M:%S", &tm);
        metric_family_append(&oi->fams[FAM_OPENVPN_USER_CONNECTED_SINCE],
                             VALUE_GAUGE((double) mktime(&tm)), &oi->labels,
                             &(label_pair_const_t){.name="common_name", .value=fields[0]}, NULL);

        sum_users += 1;
    }

    if (ferror(fh))
        return -1;

    if (found_header == false) {
        PLUGIN_NOTICE("Unknown file format in instance %s, please report this as bug. "
                      "Make sure to include your status file, so the plugin can be adapted.",
                      oi->instance);
        return -1;
    }

    metric_family_append(&oi->fams[FAM_OPENVPN_CONNECTIONS],
                         VALUE_GAUGE(sum_users), &oi->labels, NULL);
    return 0;
}

/* for reading status version 2 / version 3
 * status file is generated by openvpn/multi.c:multi_print_status()
 * http://svn.openvpn.net/projects/openvpn/trunk/openvpn/multi.c
 */
static int multi2_read(openvpn_instance_t *oi, FILE *fh, const char *delim)
{
    char buffer[1024];
    /* OpenVPN-2.4 has 11 fields of data + 2 fields for "HEADER" and "CLIENT_LIST"
     * So, set array size to 20 elements, to support future extensions.
     */
    char *fields[20];
    const int max_fields = STATIC_ARRAY_SIZE(fields);
    long long sum_users = 0;

    bool found_header = false;
    int idx_cname = 0;
    int idx_bytes_recv = 0;
    int idx_bytes_sent = 0;
    int idx_since = 0;
    int columns = 0;

    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        int fields_num = 0;

        char *field = fields[fields_num++] = buffer;

        while ((field = strstr(field, delim)) != NULL) {
            *field = '\0';
            fields[fields_num++] = ++field;
            if (fields_num >= max_fields)
                break;
        }

        /* Try to find section header */
        if (found_header == false) {
            if (fields_num < 2)
                continue;
            if ((strcmp(fields[0], "TIME") == 0) && (fields_num > 2)) {
                metric_family_append(&oi->fams[FAM_OPENVPN_UPDATED],
                                     VALUE_GAUGE(atoll(fields[2])), &oi->labels, NULL);
                continue;
            }
            if (strcmp(fields[0], "HEADER") != 0)
                continue;
            if (strcmp(fields[1], "CLIENT_LIST") != 0)
                continue;

            for (int i = 2; i < fields_num; i++) {
                if (strcmp(fields[i], "Common Name") == 0) {
                    idx_cname = i - 1;
                } else if (strcmp(fields[i], "Bytes Received") == 0) {
                    idx_bytes_recv = i - 1;
                } else if (strcmp(fields[i], "Bytes Sent") == 0) {
                    idx_bytes_sent = i - 1;
                } else if (strcmp(fields[i], "Connected Since (time_t)") == 0) {
                    idx_since = i - 1;
                }
            }

            PLUGIN_DEBUG("found MULTI v2/v3 HEADER. "
                         "Column idx: cname: %d, bytes_recv: %d, bytes_sent: %d",
                         idx_cname, idx_bytes_recv, idx_bytes_sent);

            if (idx_cname == 0 || idx_bytes_recv == 0 || idx_bytes_sent == 0)
                break;

            /* Data row has 1 field ("HEADER") less than header row */
            columns = fields_num - 1;

            found_header = true;
            continue;
        }

        /* Header already found. Check if the line is the section data.
         * If no match, then section was finished and there is no more data.
         * Empty section is OK too.
         */
        if (fields_num == 0 || strcmp(fields[0], "CLIENT_LIST") != 0)
            break;

        /* Check if the data line fields count matches header line. */
        if (fields_num != columns) {
            PLUGIN_ERROR("File format error in instance %s: Fields count mismatch.", oi->instance);
            return -1;
        }

        PLUGIN_DEBUG("found MULTI v2/v3 CLIENT_LIST. "
                     "Columns: cname: %s, bytes_recv: %s, bytes_sent: %s",
                     fields[idx_cname], fields[idx_bytes_recv], fields[idx_bytes_sent]);

        metric_family_append(&oi->fams[FAM_OPENVPN_USER_RECEIVED_BYTES],
                    VALUE_COUNTER(atoll(fields[idx_bytes_recv])), &oi->labels,
                    &(label_pair_const_t){.name="common_name", .value=fields[idx_cname]}, NULL);
        metric_family_append(&oi->fams[FAM_OPENVPN_USER_SENT_BYTES],
                    VALUE_COUNTER(atoll(fields[idx_bytes_sent])), &oi->labels,
                    &(label_pair_const_t){.name="common_name", .value=fields[idx_cname]}, NULL);

        if (idx_since != 0)
            metric_family_append(&oi->fams[FAM_OPENVPN_USER_CONNECTED_SINCE],
                        VALUE_GAUGE((double)atoll(fields[idx_since])), &oi->labels,
                        &(label_pair_const_t){.name="common_name", .value=fields[idx_cname]}, NULL);

        sum_users += 1;
    }

    if (ferror(fh))
        return -1;

    if (found_header == false) {
        PLUGIN_NOTICE("Unknown file format in instance %s, please report this as bug. "
                      "Make sure to include your status file, so the plugin can be adapted.",
                      oi->instance);
        return -1;
    }

    metric_family_append(&oi->fams[FAM_OPENVPN_CONNECTIONS],
                        VALUE_GAUGE(sum_users), &oi->labels, NULL);
    return 0;
}

static int openvpn_read(user_data_t *user_data)
{
    openvpn_instance_t *oi = user_data->data;

    FILE *fh = fopen(oi->file, "r");
    if (fh == NULL) {
        PLUGIN_WARNING("fopen(%s) failed: %s", oi->file, STRERRNO);
        return -1;
    }

    // Try to detect file format by its first line
    char buffer[1024];
    if ((fgets(buffer, sizeof(buffer), fh)) == NULL) {
        PLUGIN_WARNING("failed to get data from: %s", oi->file);
        fclose(fh);
        return -1;
    }

    int read = 0;
    if (strcmp(buffer, TITLE_SINGLE) == 0) { // OpenVPN STATISTICS
        PLUGIN_DEBUG("found status file SINGLE");
        read = single_read(oi, fh);
    } else if (strcmp(buffer, TITLE_V1) == 0) { // OpenVPN CLIENT LIST
        PLUGIN_DEBUG("found status file MULTI version 1");
        read = multi1_read(oi, fh);
    } else if (strncmp(buffer, TITLE_V2, strlen(TITLE_V2)) == 0) { // TITLE,
        PLUGIN_DEBUG("found status file MULTI version 2");
        read = multi2_read(oi, fh, ",");
    } else if (strncmp(buffer, TITLE_V3, strlen(TITLE_V3)) == 0) { // TITLE\t
        PLUGIN_DEBUG("found status file MULTI version 3");
        read = multi2_read(oi, fh, "\t");
    } else {
        PLUGIN_NOTICE("%s: Unknown file format, please report this as bug. "
                      "Make sure to include your status file, so the plugin can be adapted.",
                      oi->file);
        read = -1;
    }
    fclose(fh);

    plugin_dispatch_metric_family_array_filtered(oi->fams, FAM_OPENVPN_MAX, oi->filter, 0);

    return read;
}

static int openvpn_instance_config(config_item_t *ci)
{
    openvpn_instance_t *oi = calloc(1, sizeof(*oi));
    if (oi == NULL) {
        PLUGIN_ERROR("calloc failed: %s.", STRERRNO);
        return -1;
    }

    int status = cf_util_get_string(ci, &oi->instance);
    if (status != 0) {
        openvpn_free(oi);
        return -1;
    }

    memcpy(oi->fams, fams, sizeof(*fams) * FAM_OPENVPN_MAX);

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("status-file", child->key) == 0) {
            status = cf_util_get_string(child, &oi->file);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &oi->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &oi->filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        openvpn_free(oi);
        return status;
    }

    label_set_add(&oi->labels, true, "instance", oi->instance);

    return plugin_register_complex_read("openvpn", oi->instance, openvpn_read, interval,
                                        &(user_data_t){ .data = oi, .free_func = openvpn_free });
}

static int openvpn_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = openvpn_instance_config(child);
        } else {
            PLUGIN_ERROR("The configuration option '%s' in %s:%d is not allowed here.",
                         child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("openvpn", openvpn_config);
}
