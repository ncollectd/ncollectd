// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2015 Sebastian 'tokkee' Harl
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>

#include "ncollectd.h"
#include "libutils/common.h"
#include <stdbool.h>
#include <stddef.h>

#include "curl_stats.h"

enum {
    COLLECT_TOTAL_TIME              = (1 <<  0),
    COLLECT_NAMELOOKUP_TIME         = (1 <<  1),
    COLLECT_CONNECT_TIME            = (1 <<  2),
    COLLECT_PRETRANSFER_TIME        = (1 <<  3),
    COLLECT_SIZE_UPLOAD             = (1 <<  4),
    COLLECT_SIZE_DOWNLOAD           = (1 <<  5),
    COLLECT_SPEED_DOWNLOAD          = (1 <<  6),
    COLLECT_SPEED_UPLOAD            = (1 <<  7),
    COLLECT_HEADER_SIZE             = (1 <<  8),
    COLLECT_REQUEST_SIZE            = (1 <<  9),
    COLLECT_CONTENT_LENGTH_DOWNLOAD = (1 << 10),
    COLLECT_CONTENT_LENGTH_UPLOAD   = (1 << 11),
    COLLECT_STARTTRANSFER_TIME      = (1 << 12),
    COLLECT_REDIRECT_TIME           = (1 << 13),
    COLLECT_REDIRECT_COUNT          = (1 << 14),
    COLLECT_NUM_CONNECTS            = (1 << 15),
#ifdef HAVE_CURLINFO_APPCONNECT_TIME
    COLLECT_APPCONNECT_TIME         = (1 << 16),
#endif
};

static cf_flags_t curl_stats_flags[] = {
    { "total_time",              COLLECT_TOTAL_TIME              },
    { "namelookup_time",         COLLECT_NAMELOOKUP_TIME         },
    { "connect_time",            COLLECT_CONNECT_TIME            },
    { "pretransfer_time",        COLLECT_PRETRANSFER_TIME        },
    { "size_upload",             COLLECT_SIZE_UPLOAD             },
    { "size_download",           COLLECT_SIZE_DOWNLOAD           },
    { "speed_download",          COLLECT_SPEED_DOWNLOAD          },
    { "speed_upload",            COLLECT_SPEED_UPLOAD            },
    { "header_size",             COLLECT_HEADER_SIZE             },
    { "request_size",            COLLECT_REQUEST_SIZE            },
    { "content_length_download", COLLECT_CONTENT_LENGTH_DOWNLOAD },
    { "content_length_upload",   COLLECT_CONTENT_LENGTH_UPLOAD   },
    { "starttransfer_time",      COLLECT_STARTTRANSFER_TIME      },
    { "redirect_time",           COLLECT_REDIRECT_TIME           },
    { "redirect_count",          COLLECT_REDIRECT_COUNT          },
    { "num_connects",            COLLECT_NUM_CONNECTS            },
#ifdef HAVE_CURLINFO_APPCONNECT_TIME
    { "appconnect_time",         COLLECT_APPCONNECT_TIME         },
#endif
};
static size_t curl_stats_flags_size = STATIC_ARRAY_SIZE(curl_stats_flags);

typedef enum {
    DISPATCH_SPEED = 0,
    DISPATCH_GAUGE,
    DISPATCH_OFF_T,
    DISPATCH_SIZE,
} curl_stats_type_t;

static struct {
    int flag;
    CURLINFO info;
    curl_stats_type_t type;
    char *metric_name;
} curl_stats_field_specs[] = {
    { COLLECT_TOTAL_TIME,              CURLINFO_TOTAL_TIME,                DISPATCH_GAUGE, "total_seconds"          },
    { COLLECT_NAMELOOKUP_TIME,         CURLINFO_NAMELOOKUP_TIME,           DISPATCH_GAUGE, "namelookup_seconds"     },
    { COLLECT_CONNECT_TIME,            CURLINFO_CONNECT_TIME,              DISPATCH_GAUGE, "connect_seconds"        },
    { COLLECT_PRETRANSFER_TIME,        CURLINFO_PRETRANSFER_TIME,          DISPATCH_GAUGE, "pretransfer_seconds"    },
#ifdef HAVE_CURLINFO_SIZE_UPLOAD_T
    { COLLECT_SIZE_UPLOAD,             CURLINFO_SIZE_UPLOAD_T,             DISPATCH_OFF_T, "upload_bytes"           },
#else
    { COLLECT_SIZE_UPLOAD,             CURLINFO_SIZE_UPLOAD,               DISPATCH_GAUGE, "upload_bytes"           },
#endif
#ifdef HAVE_CURLINFO_SIZE_DOWNLOAD_T
    { COLLECT_SIZE_DOWNLOAD,           CURLINFO_SIZE_DOWNLOAD_T,           DISPATCH_OFF_T, "download_bytes"         },
#else
    { COLLECT_SIZE_DOWNLOAD,           CURLINFO_SIZE_DOWNLOAD,             DISPATCH_GAUGE, "download_bytes"         },
#endif
#ifdef HAVE_CURLINFO_SPEED_DOWNLOAD_T
    { COLLECT_SPEED_DOWNLOAD,          CURLINFO_SPEED_DOWNLOAD_T,          DISPATCH_OFF_T, "download_bitrate"       },
#else
    { COLLECT_SPEED_DOWNLOAD,          CURLINFO_SPEED_DOWNLOAD,            DISPATCH_SPEED, "download_bitrate"       },
#endif
#ifdef HAVE_CURLINFO_SPEED_UPLOAD_T
    { COLLECT_SPEED_UPLOAD,            CURLINFO_SPEED_UPLOAD_T,            DISPATCH_OFF_T, "upload_bitrate"         },
#else
    { COLLECT_SPEED_UPLOAD,            CURLINFO_SPEED_UPLOAD,              DISPATCH_SPEED, "upload_bitrate"         },
#endif
    { COLLECT_HEADER_SIZE,             CURLINFO_HEADER_SIZE,               DISPATCH_SIZE,  "header_bytes"           },
    { COLLECT_REQUEST_SIZE,            CURLINFO_REQUEST_SIZE,              DISPATCH_SIZE,  "request_bytes"          },
#ifdef HAVE_CURLINFO_CONTENT_LENGTH_DOWNLOAD_T
    { COLLECT_CONTENT_LENGTH_DOWNLOAD, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, DISPATCH_OFF_T, "download_content_bytes" },
#else
    { COLLECT_CONTENT_LENGTH_DOWNLOAD, CURLINFO_CONTENT_LENGTH_DOWNLOAD,   DISPATCH_GAUGE, "download_content_bytes" },
#endif
#ifdef HAVE_CURLINFO_CONTENT_LENGTH_UPLOAD_T
    { COLLECT_CONTENT_LENGTH_UPLOAD,   CURLINFO_CONTENT_LENGTH_UPLOAD_T,   DISPATCH_OFF_T, "upload_content_bytes"   },
#else
    { COLLECT_CONTENT_LENGTH_UPLOAD,   CURLINFO_CONTENT_LENGTH_UPLOAD,     DISPATCH_GAUGE, "upload_content_bytes"   },
#endif
    { COLLECT_STARTTRANSFER_TIME,      CURLINFO_STARTTRANSFER_TIME,        DISPATCH_GAUGE, "start_transfer_seconds" },
    { COLLECT_REDIRECT_TIME,           CURLINFO_REDIRECT_TIME,             DISPATCH_GAUGE, "redirect_seconds"       },
    { COLLECT_REDIRECT_COUNT,          CURLINFO_REDIRECT_COUNT,            DISPATCH_SIZE,  "redirects"              },
    { COLLECT_NUM_CONNECTS,            CURLINFO_NUM_CONNECTS,              DISPATCH_SIZE,  "connects"               },
#ifdef HAVE_CURLINFO_APPCONNECT_TIME
    { COLLECT_APPCONNECT_TIME,         CURLINFO_APPCONNECT_TIME,           DISPATCH_GAUGE, "appconnect_seconds"     },
#endif
};
static size_t curl_stats_field_specs_size = STATIC_ARRAY_SIZE(curl_stats_field_specs);

int curl_stats_from_config(config_item_t *ci, uint64_t *flags)
{
    return cf_util_get_flags(ci, curl_stats_flags, curl_stats_flags_size, flags);
}

int curl_stats_dispatch(CURL *curl, uint64_t flags, char *metric_prefix ,label_set_t *labels)
{
    if (curl == NULL) {
        PLUGIN_ERROR("dispatch() called with missing arguments (curl=%p)", curl);
        return -1;
    }

    for (size_t field = 0; field < curl_stats_field_specs_size; field++) {
        if (!(curl_stats_field_specs[field].flag & flags))
            continue;

        CURLcode code;
        value_t value = {0};
        switch (curl_stats_field_specs[field].type) {
            case DISPATCH_SPEED: {
                double raw;
                code = curl_easy_getinfo(curl, curl_stats_field_specs[field].info, &raw);
                if (code != CURLE_OK)
                    continue;
                value = VALUE_GAUGE(raw * 8);
            }   break;
            case DISPATCH_GAUGE: {
                double raw;
                code = curl_easy_getinfo(curl, curl_stats_field_specs[field].info, &raw);
                if (code != CURLE_OK)
                    continue;
                value = VALUE_GAUGE(raw);
            }   break;
            case DISPATCH_OFF_T: {
                curl_off_t raw;
                code = curl_easy_getinfo(curl, curl_stats_field_specs[field].info, &raw);
                if (code != CURLE_OK)
                    continue;
                value = VALUE_GAUGE(raw);
            }   break;
            case DISPATCH_SIZE: {
                long raw;
                code = curl_easy_getinfo(curl, curl_stats_field_specs[field].info, &raw);
                if (code != CURLE_OK)
                    continue;
                value = VALUE_GAUGE(raw);
            }   break;
        }

        metric_family_t fam = {0};

        char buffer[1024];
        if (metric_prefix == NULL) {
            fam.name = curl_stats_field_specs[field].metric_name;
        } else {
            ssnprintf(buffer, sizeof(buffer), "%s%s",
                      metric_prefix, curl_stats_field_specs[field].metric_name);
            fam.name = buffer;
        }

        fam.type = METRIC_TYPE_GAUGE;

        metric_family_append(&fam, value, labels, NULL);

        plugin_dispatch_metric_family(&fam, 0);
    }

    return 0;
}
