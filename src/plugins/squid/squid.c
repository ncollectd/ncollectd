// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <curl/curl.h>

enum {
    FAM_SQUID_UP,
    FAM_SQUID_CLIENT_HTTP_REQUESTS,
    FAM_SQUID_CLIENT_HTTP_HITS,
    FAM_SQUID_CLIENT_HTTP_ERRORS,
    FAM_SQUID_CLIENT_HTTP_IN_BYTES,
    FAM_SQUID_CLIENT_HTTP_OUT_BYTES,
    FAM_SQUID_CLIENT_HTTP_HIT_OUT_BYTES,
    FAM_SQUID_SERVER_ALL_REQUESTS,
    FAM_SQUID_SERVER_ALL_ERRORS,
    FAM_SQUID_SERVER_ALL_IN_BYTES,
    FAM_SQUID_SERVER_ALL_OUT_BYTES,
    FAM_SQUID_SERVER_HTTP_REQUESTS,
    FAM_SQUID_SERVER_HTTP_ERRORS,
    FAM_SQUID_SERVER_HTTP_IN_BYTES,
    FAM_SQUID_SERVER_HTTP_OUT_BYTES,
    FAM_SQUID_SERVER_FTP_REQUESTS,
    FAM_SQUID_SERVER_FTP_ERRORS,
    FAM_SQUID_SERVER_FTP_IN_BYTES,
    FAM_SQUID_SERVER_FTP_OUT_BYTES,
    FAM_SQUID_SERVER_OTHER_REQUESTS,
    FAM_SQUID_SERVER_OTHER_ERRORS,
    FAM_SQUID_SERVER_OTHER_IN_BYTES,
    FAM_SQUID_SERVER_OTHER_OUT_BYTES,
    FAM_SQUID_ICP_SENT_PKTS,
    FAM_SQUID_ICP_RECV_PKTS,
    FAM_SQUID_ICP_SENT_QUERIES,
    FAM_SQUID_ICP_SENT_REPLIES,
    FAM_SQUID_ICP_RECV_QUERIES,
    FAM_SQUID_ICP_RECV_REPLIES,
    FAM_SQUID_ICP_QUERY_TIMEOUTS,
    FAM_SQUID_ICP_REPLIES_QUEUED,
    FAM_SQUID_ICP_SENT_BYTES,
    FAM_SQUID_ICP_RECV_BYTES,
    FAM_SQUID_ICP_Q_SENT_BYTES,
    FAM_SQUID_ICP_R_SENT_BYTES,
    FAM_SQUID_ICP_Q_RECV_BYTES,
    FAM_SQUID_ICP_R_RECV_BYTES,
    FAM_SQUID_ICP_TIMES_USED,
    FAM_SQUID_CD_TIMES_USED,
    FAM_SQUID_CD_SENT_MSGS,
    FAM_SQUID_CD_RECV_MSGS,
    FAM_SQUID_CD_MEMORY,
    FAM_SQUID_CD_LOCAL_MEMORY,
    FAM_SQUID_CD_SENT_BYTES,
    FAM_SQUID_CD_RECV_BYTES,
    FAM_SQUID_UNLINK_REQUESTS,
    FAM_SQUID_PAGE_FAULTS,
    FAM_SQUID_SELECT_LOOPS,
    FAM_SQUID_CPU_SECONDS,
    FAM_SQUID_WALL_SECONDS,
    FAM_SQUID_SWAP_OUTS,
    FAM_SQUID_SWAP_INS,
    FAM_SQUID_SWAP_FILES_CLEANED,
    FAM_SQUID_ABORTED_REQUESTS,
    FAM_SQUID_MAX,
};

static metric_family_t fams_squid[FAM_SQUID_MAX] = {
    [FAM_SQUID_UP] = {
        .name = "squid_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the apache server be reached.",
    },
    [FAM_SQUID_CLIENT_HTTP_REQUESTS] = {
        .name = "squid_client_http_requests",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CLIENT_HTTP_HITS] = {
        .name = "squid_client_http_hits",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CLIENT_HTTP_ERRORS] = {
        .name = "squid_client_http_errors",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CLIENT_HTTP_IN_BYTES] = {
        .name = "squid_client_http_in_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CLIENT_HTTP_OUT_BYTES] = {
        .name = "squid_client_http_out_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CLIENT_HTTP_HIT_OUT_BYTES] = {
        .name = "squid_client_http_hit_out_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_ALL_REQUESTS] = {
        .name = "squid_server_all_requests",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_ALL_ERRORS] = {
        .name = "squid_server_all_errors",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_ALL_IN_BYTES] = {
        .name = "squid_server_all_in_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_ALL_OUT_BYTES] = {
        .name = "squid_server_all_out_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_HTTP_REQUESTS] = {
        .name = "squid_server_http_requests",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_HTTP_ERRORS] = {
        .name = "squid_server_http_errors",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_HTTP_IN_BYTES] = {
        .name = "squid_server_http_in_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_HTTP_OUT_BYTES] = {
        .name = "squid_server_http_out_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_FTP_REQUESTS] = {
        .name = "squid_server_ftp_requests",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_FTP_ERRORS] = {
        .name = "squid_server_ftp_errors",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_FTP_IN_BYTES] = {
        .name = "squid_server_ftp_in_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_FTP_OUT_BYTES] = {
        .name = "squid_server_ftp_out_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_OTHER_REQUESTS] = {
        .name = "squid_server_other_requests",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_OTHER_ERRORS] = {
        .name = "squid_server_other_errors",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_OTHER_IN_BYTES] = {
        .name = "squid_server_other_in_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SERVER_OTHER_OUT_BYTES] = {
        .name = "squid_server_other_out_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_SENT_PKTS] = {
        .name = "squid_icp_sent_pkts",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_RECV_PKTS] = {
        .name = "squid_icp_recv_pkts",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_SENT_QUERIES] = {
        .name = "squid_icp_sent_queries",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_SENT_REPLIES] = {
        .name = "squid_icp_sent_replies",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_RECV_QUERIES] = {
        .name = "squid_icp_recv_queries",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_RECV_REPLIES] = {
        .name = "squid_icp_recv_replies",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_QUERY_TIMEOUTS] = {
        .name = "squid_icp_query_timeouts",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_REPLIES_QUEUED] = {
        .name = "squid_icp_replies_queued",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_SENT_BYTES] = {
        .name = "squid_icp_sent_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_RECV_BYTES] = {
        .name = "squid_icp_recv_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_Q_SENT_BYTES] = {
        .name = "squid_icp_q_sent_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_R_SENT_BYTES] = {
        .name = "squid_icp_r_sent_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_Q_RECV_BYTES] = {
        .name = "squid_icp_q_recv_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_R_RECV_BYTES] = {
        .name = "squid_icp_r_recv_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ICP_TIMES_USED] = {
        .name = "squid_icp_times_used",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CD_TIMES_USED] = {
        .name = "squid_cd_times_used",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CD_SENT_MSGS] = {
        .name = "squid_cd_sent_msgs",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CD_RECV_MSGS] = {
        .name = "squid_cd_recv_msgs",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CD_MEMORY] = {
        .name = "squid_cd_memory",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CD_LOCAL_MEMORY] = {
        .name = "squid_cd_local_memory",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CD_SENT_BYTES] = {
        .name = "squid_cd_sent_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CD_RECV_BYTES] = {
        .name = "squid_cd_recv_bytes",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_UNLINK_REQUESTS] = {
        .name = "squid_unlink_requests",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_PAGE_FAULTS] = {
        .name = "squid_page_faults",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SELECT_LOOPS] = {
        .name = "squid_select_loops",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_CPU_SECONDS] = {
        .name = "squid_cpu_time",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_WALL_SECONDS] = {
        .name = "squid_wall_time",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SWAP_OUTS] = {
        .name = "squid_swap_outs",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SWAP_INS] = {
        .name = "squid_swap_ins",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_SWAP_FILES_CLEANED] = {
        .name = "squid_swap_files_cleaned",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SQUID_ABORTED_REQUESTS] = {
        .name = "squid_aborted_requests",
        .type = METRIC_TYPE_COUNTER,
    },
};

#include "plugins/squid/squid_counters.h"

typedef struct {
    char *instance;
    label_set_t labels;
    plugin_filter_t *filter;
    char *url;
    char *furl;
    char *user;
    char *pass;
    char *credentials;
    bool digest;
    bool verify_peer;
    bool verify_host;
    char *cacert;
    struct curl_slist *headers;
    CURL *curl;
    char curl_errbuf[CURL_ERROR_SIZE];
    strbuf_t buffer;
    metric_family_t fams[FAM_SQUID_MAX];
} squid_t;

static size_t squid_curl_callback(void *buf, size_t size, size_t nmemb, void *user_data)
{
    size_t len = size * nmemb;
    if (unlikely(len == 0))
        return len;

    squid_t *sq = user_data;
    if (unlikely(sq == NULL))
        return 0;

    int status = strbuf_putstrn(&sq->buffer, buf, len);
    if (status != 0) {
        PLUGIN_ERROR("Failed to copy to buffer.");
        return 0;
    }

    return len;
}

static void squid_free(void *arg)
{
    squid_t *sq = (squid_t *)arg;
    if (sq == NULL)
        return;

    if (sq->curl != NULL)
        curl_easy_cleanup(sq->curl);
    sq->curl = NULL;

    label_set_reset(&sq->labels);
    plugin_filter_free(sq->filter);

    free(sq->url);
    free(sq->furl);
    free(sq->user);
    free(sq->pass);
    free(sq->credentials);
    free(sq->cacert);
    curl_slist_free_all(sq->headers);

    strbuf_destroy(&sq->buffer);

    free(sq);
}

static int squid_config_append_string(config_item_t *ci, const char *name,
                                                          struct curl_slist **dest)
{
    struct curl_slist *temp = NULL;
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("`%s' needs exactly one string argument.", name);
        return -1;
    }

    temp = curl_slist_append(*dest, ci->values[0].value.string);
    if (unlikely(temp == NULL))
        return -1;

    *dest = temp;

    return 0;
}

static int squid_curl_init(squid_t *sq)
{
    if (sq->curl != NULL)
        return 0;

    sq->curl = curl_easy_init();
    if (unlikely(sq->curl == NULL)) {
        PLUGIN_ERROR("curl_easy_init failed.");
        return -1;
    }

    CURLcode rcode = 0;

    rcode = curl_easy_setopt(sq->curl, CURLOPT_NOSIGNAL, 1L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_NOSIGNAL failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(sq->curl, CURLOPT_WRITEFUNCTION, squid_curl_callback);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEFUNCTION failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(sq->curl, CURLOPT_WRITEDATA, sq);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEDATA failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(sq->curl, CURLOPT_USERAGENT, NCOLLECTD_USERAGENT);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERAGENT failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(sq->curl, CURLOPT_ERRORBUFFER, sq->curl_errbuf);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_ERRORBUFFER failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(sq->curl, CURLOPT_FOLLOWLOCATION, 1L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_ERRORBUFFER failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(sq->curl, CURLOPT_MAXREDIRS, 50L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_ERRORBUFFER failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(sq->curl, CURLOPT_IPRESOLVE, (long)CURL_IPRESOLVE_WHATEVER);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_IPRESOLVE failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    if (sq->user != NULL) {
#ifdef HAVE_CURLOPT_USERNAME
        rcode = curl_easy_setopt(sq->curl, CURLOPT_USERNAME, sq->user);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERNAME failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
        rcode = curl_easy_setopt(sq->curl, CURLOPT_PASSWORD, (sq->pass == NULL) ? "" : sq->pass);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_PASSWORD failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
#else
        size_t credentials_size;

        credentials_size = strlen(sq->user) + 2; if (sq->pass != NULL)
        credentials_size += strlen(sq->pass);

        sq->credentials = malloc(credentials_size);
        if (sq->credentials == NULL) {
            PLUGIN_ERROR("malloc failed.");
            return -1;
        }

        snprintf(sq->credentials, credentials_size, "%s:%s", sq->user,
                         (sq->pass == NULL) ? "" : sq->pass);
        rcode = curl_easy_setopt(sq->curl, CURLOPT_USERPWD, sq->credentials);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERPWD failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
#endif
        if (sq->digest) {
            rcode = curl_easy_setopt(sq->curl, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST);
            if (rcode != CURLE_OK) {
                PLUGIN_ERROR("curl_easy_setopt CURLOPT_HTTPAUTH failed: %s",
                             curl_easy_strerror(rcode));
                return -1;
            }
        }
    }

    rcode = curl_easy_setopt(sq->curl, CURLOPT_SSL_VERIFYPEER, (long)sq->verify_peer);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_SSL_VERIFYPEER failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(sq->curl, CURLOPT_SSL_VERIFYHOST, sq->verify_host ? 2L : 0L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_SSL_VERIFYHOST failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    if (sq->cacert != NULL) {
        rcode = curl_easy_setopt(sq->curl, CURLOPT_CAINFO, sq->cacert);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_CAINFO failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }
    if (sq->headers != NULL) {
        rcode = curl_easy_setopt(sq->curl, CURLOPT_HTTPHEADER, sq->headers);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_HTTPHEADER failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }

#ifdef HAVE_CURLOPT_TIMEOUT_MS
    rcode = curl_easy_setopt(sq->curl, CURLOPT_TIMEOUT_MS,
                                       (long)CDTIME_T_TO_MS(plugin_get_interval()));
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_TIMEOUT_MS failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }
#endif

    rcode = curl_easy_setopt(sq->curl, CURLOPT_URL, sq->furl);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_URL failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    return 0;
}

static void squid_curl_cleanup(squid_t *sq)
{
    if (sq->curl == NULL) {
        curl_easy_cleanup(sq->curl);
        sq->curl = NULL;
        sq->curl_errbuf[0] = '\0';
    }
}

static int squid_curl_perform(squid_t *sq)
{
    strbuf_reset(&sq->buffer);

    int status = curl_easy_perform(sq->curl);
    if (unlikely(status != CURLE_OK)) {
        PLUGIN_ERROR("curl_easy_perform failed with status %i: %s", status, sq->curl_errbuf);
        return -1;
    }

    long rcode = 0;
    status = curl_easy_getinfo(sq->curl, CURLINFO_RESPONSE_CODE, &rcode);
    if (unlikely(status != CURLE_OK)) {
        PLUGIN_ERROR("Fetching response code failed with status %i: %s",
                    status, sq->curl_errbuf);
        return -1;
    }

    if (unlikely(rcode != 200)) {
        PLUGIN_ERROR("response code for %s was %ld", sq->furl, rcode);
        return -1;
    }

    if (unlikely(strbuf_len(&sq->buffer) == 0)) {
        PLUGIN_ERROR("empty response for %s", sq->furl);
        return -1;
    }

    return 0;
}

static int squid_read(user_data_t *ud)
{
    if (unlikely((ud == NULL) || (ud->data == NULL))) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }

    squid_t *sq = (squid_t *)ud->data;

    if (squid_curl_init(sq) != 0) {
        squid_curl_cleanup(sq);
        metric_family_append(&sq->fams[FAM_SQUID_UP], VALUE_GAUGE(0), &sq->labels, NULL);
        plugin_dispatch_metric_family_filtered(&sq->fams[FAM_SQUID_UP], sq->filter, 0);
        return 0;
    }

    if (squid_curl_perform(sq) != 0) {
        squid_curl_cleanup(sq);
        metric_family_append(&sq->fams[FAM_SQUID_UP], VALUE_GAUGE(0), &sq->labels, NULL);
        plugin_dispatch_metric_family_filtered(&sq->fams[FAM_SQUID_UP], sq->filter, 0);
        return 0;
    }

    metric_family_append(&sq->fams[FAM_SQUID_UP], VALUE_GAUGE(1), &sq->labels, NULL);

    char *ptr = sq->buffer.ptr;
    char *saveptr = NULL;
    char *line;
    while ((line = strtok_r(ptr, "\n", &saveptr)) != NULL) {
        ptr = NULL;

        char *key = line;
        char *val = strchr(key, '=');
        if (val == NULL)
            continue;
        *val = '\0';
        val++;
        while (*val == ' ') val++;

        size_t key_len = strlen(key);
        while ((key_len > 0) && key[key_len - 1] == ' ') {
            key[key_len - 1] = '\0';
            key_len--;
        }

        const struct squid_counter *sc = squid_counter_get_key(key, key_len);
        if (sc == NULL)
            continue;

        value_t value = {0};

        switch(sc->fam) {
        case FAM_SQUID_CPU_SECONDS:
        case FAM_SQUID_WALL_SECONDS: {
            double ret_value = 0;
            if (parse_double(val, &ret_value) != 0) {
                PLUGIN_WARNING("Unable to parse field '%s'.", key);
                continue;
            }
            value = VALUE_COUNTER_FLOAT64(ret_value);
            break;
        }
        default:
            if (sq->fams[sc->fam].type == METRIC_TYPE_GAUGE) {
                double ret_value = 0;
                if (parse_double(val, &ret_value) != 0) {
                    PLUGIN_WARNING("Unable to parse field '%s'.", key);
                    continue;
                }
                if (sc->scale != 0)
                    ret_value = ret_value * sc->scale;
                value = VALUE_GAUGE(ret_value);
            } else if (sq->fams[sc->fam].type == METRIC_TYPE_COUNTER) {
                uint64_t ret_value = 0;
                if (parse_uinteger(val, &ret_value) != 0) {
                    PLUGIN_WARNING("Unable to parse field '%s'.", key);
                    continue;
                }
                if (sc->scale != 0)
                    ret_value = ret_value * (uint64_t)sc->scale;
                value = VALUE_COUNTER(ret_value);
            }
            break;
        }

        metric_family_append(&sq->fams[sc->fam], value, &sq->labels, NULL);
    }

    plugin_dispatch_metric_family_array_filtered(sq->fams, FAM_SQUID_MAX, sq->filter, 0);

    return 0;
}

static int squid_config_instance(config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("'instance' blocks need exactly one string argument.");
        return -1;
    }

    squid_t *sq = calloc(1, sizeof(*sq));
    if (sq == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }
    sq->digest = false;
    sq->verify_peer = true;
    sq->verify_host = true;

    memcpy(sq->fams, fams_squid, FAM_SQUID_MAX * sizeof(fams_squid[0]));

    sq->instance = strdup(ci->values[0].value.string);
    if (sq->instance == NULL) {
        PLUGIN_ERROR("strdup failed.");
        free(sq);
        return -1;
    }

    int status = 0;
    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("url", child->key) == 0) {
            status = cf_util_get_string(child, &sq->url);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &sq->labels);
        } else if (strcasecmp("user", child->key) == 0) {
            status = cf_util_get_string(child, &sq->user);
        } else if (strcasecmp("user-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &sq->user);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &sq->pass);
        } else if (strcasecmp("password-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &sq->pass);
        } else if (strcasecmp("digest", child->key) == 0) {
            status = cf_util_get_boolean(child, &sq->digest);
        } else if (strcasecmp("verify-peer", child->key) == 0) {
            status = cf_util_get_boolean(child, &sq->verify_peer);
        } else if (strcasecmp("verify-host", child->key) == 0) {
            status = cf_util_get_boolean(child, &sq->verify_host);
        } else if (strcasecmp("ca-cert", child->key) == 0) {
            status = cf_util_get_string(child, &sq->cacert);
        } else if (strcasecmp("header", child->key) == 0) {
            status = squid_config_append_string(child, "Header", &sq->headers);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &sq->filter);
        } else {
            PLUGIN_WARNING("Option '%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status == 0) {
        if (sq->url == NULL) {
            PLUGIN_WARNING("'url' missing in 'instance' block.");
            status = -1;
        } else {
            char url[1024];
            size_t url_len = strlen(sq->url);
            ssnprintf(url, sizeof(url), "%s%ssquid-internal-mgr/counters",
                            sq->url, sq->url[url_len-1] == '/' ? "" : "/");
            sq->furl = strdup(url);
            if (sq->furl == NULL) {
                PLUGIN_ERROR("strdup failed.");
                status = -1;
            }
        }
    }

    if (status != 0) {
        squid_free(sq);
        return status;
    }

    label_set_add(&sq->labels, true, "instance", sq->instance);

    return plugin_register_complex_read("squid", sq->instance, squid_read, interval,
                                        &(user_data_t){ .data = sq, .free_func = squid_free });
}

static int squid_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = squid_config_instance(child);
        } else {
            PLUGIN_WARNING("Option `%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int squid_init(void)
{
    curl_global_init(CURL_GLOBAL_SSL);
    return 0;
}

void module_register(void)
{
    plugin_register_config("squid", squid_config);
    plugin_register_init("squid", squid_init);
}
