// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2014 Google, Inc.
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Jeremy Katz <jeremy at katzbox.net>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>

enum {
    FAM_ZOOKEEPER_UP,
    FAM_ZOOKEEPER_VERSION,
    FAM_ZOOKEEPER_SERVER_STATE,
    FAM_ZOOKEEPER_FOLLOWERS,
    FAM_ZOOKEEPER_SYNCED_FOLLOWERS,
    FAM_ZOOKEEPER_PENDING_SYNCS,
    FAM_ZOOKEEPER_EPHEMERALS,
    FAM_ZOOKEEPER_ALIVE_CONNECTIONS,
    FAM_ZOOKEEPER_LATENCY_AVG_SECONDS,
    FAM_ZOOKEEPER_OUTSTANDING_REQUESTS,
    FAM_ZOOKEEPER_ZNODES,
    FAM_ZOOKEEPER_GLOBAL_SESSIONS,
    FAM_ZOOKEEPER_NON_MTLS_REMOTE_CONN,
    FAM_ZOOKEEPER_PACKETS_SENT,
    FAM_ZOOKEEPER_PACKETS_RECEIVED,
    FAM_ZOOKEEPER_CONNECTION_DROP_PROBABILITY,
    FAM_ZOOKEEPER_WATCHES,
    FAM_ZOOKEEPER_AUTH_FAILED,
    FAM_ZOOKEEPER_LATENCY_MIN_SECONDS,
    FAM_ZOOKEEPER_MAX_FILE_DESCRIPTORS,
    FAM_ZOOKEEPER_APPROXIMATE_DATA_SIZE_BYTES,
    FAM_ZOOKEEPER_OPEN_FILE_DESCRIPTOR,
    FAM_ZOOKEEPER_LOCAL_SESSIONS,
    FAM_ZOOKEEPER_UPTIME_SECONDS,
    FAM_ZOOKEEPER_LATENCY_MAX_SECONDS,
    FAM_ZOOKEEPER_OUTSTANDING_TLS_HANDSHAKE,
    FAM_ZOOKEEPER_NON_MTLS_LOCAL_CONN,
    FAM_ZOOKEEPER_PROPOSAL,
    FAM_ZOOKEEPER_WATCH_BYTES,
    FAM_ZOOKEEPER_OUTSTANDING_CHANGES_REMOVED,
    FAM_ZOOKEEPER_THROTTLED_OPS,
    FAM_ZOOKEEPER_STALE_REQUESTS_DROPPED,
    FAM_ZOOKEEPER_LARGE_REQUESTS_REJECTED,
    FAM_ZOOKEEPER_INSECURE_ADMIN,
    FAM_ZOOKEEPER_CONNECTION_REJECTED,
    FAM_ZOOKEEPER_CNXN_CLOSED_WITHOUT_ZK_SERVER_RUNNING,
    FAM_ZOOKEEPER_SESSIONLESS_CONNECTIONS_EXPIRED,
    FAM_ZOOKEEPER_LOOKING,
    FAM_ZOOKEEPER_DEAD_WATCHERS_QUEUED,
    FAM_ZOOKEEPER_STALE_REQUESTS,
    FAM_ZOOKEEPER_CONNECTION_DROP,
    FAM_ZOOKEEPER_LEARNER_PROPOSAL_RECEIVED,
    FAM_ZOOKEEPER_DIGEST_MISMATCHES,
    FAM_ZOOKEEPER_DEAD_WATCHERS_CLEARED,
    FAM_ZOOKEEPER_RESPONSE_PACKET_CACHE_HITS,
    FAM_ZOOKEEPER_RECEIVED_BYTES,
    FAM_ZOOKEEPER_ADD_DEAD_WATCHER_STALL_TIME,
    FAM_ZOOKEEPER_REQUEST_THROTTLE_WAIT,
    FAM_ZOOKEEPER_REQUESTS_NOT_FORWARDED_TO_COMMIT_PROCESSOR,
    FAM_ZOOKEEPER_RESPONSE_PACKET_CACHE_MISSES,
    FAM_ZOOKEEPER_ENSEMBLE_AUTH_SUCCESS,
    FAM_ZOOKEEPER_PREP_PROCESSOR_REQUEST_QUEUED,
    FAM_ZOOKEEPER_LEARNER_COMMIT_RECEIVED,
    FAM_ZOOKEEPER_STALE_REPLIES,
    FAM_ZOOKEEPER_CONNECTION_REQUEST,
    FAM_ZOOKEEPER_RESPONSE_BYTES,
    FAM_ZOOKEEPER_ENSEMBLE_AUTH_FAIL,
    FAM_ZOOKEEPER_DIFF,
    FAM_ZOOKEEPER_RESPONSE_PACKET_GET_CHILDREN_CACHE_MISSES,
    FAM_ZOOKEEPER_CONNECTION_REVALIDATE,
    FAM_ZOOKEEPER_QUIT_LEADING_DUE_TO_DISLOYAL_VOTER,
    FAM_ZOOKEEPER_SNAP,
    FAM_ZOOKEEPER_UNRECOVERABLE_ERROR,
    FAM_ZOOKEEPER_UNSUCCESSFUL_HANDSHAKE,
    FAM_ZOOKEEPER_COMMIT,
    FAM_ZOOKEEPER_STALE_SESSIONS_EXPIRED,
    FAM_ZOOKEEPER_RESPONSE_PACKET_GET_CHILDREN_CACHE_HITS,
    FAM_ZOOKEEPER_SYNC_PROCESSOR_REQUEST_QUEUED,
    FAM_ZOOKEEPER_OUTSTANDING_CHANGES_QUEUED,
    FAM_ZOOKEEPER_REQUEST_COMMIT_QUEUED,
    FAM_ZOOKEEPER_ENSEMBLE_AUTH_SKIP,
    FAM_ZOOKEEPER_SKIP_LEARNER_REQUEST_TO_NEXT_PROCESSOR,
    FAM_ZOOKEEPER_TLS_HANDSHAKE_EXCEEDED,
    FAM_ZOOKEEPER_REVALIDATE,
    FAM_ZOOKEEPER_MAX,
};

static metric_family_t fams[FAM_ZOOKEEPER_MAX] = {
    [FAM_ZOOKEEPER_UP] = {
        .name = "zookeeper_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the zookeeper server be reached.",
    },
    [FAM_ZOOKEEPER_VERSION] = {
        .name = "zookeeper_version",
        .type = METRIC_TYPE_INFO,
        .help = "Zookeper version.",
    },
    [FAM_ZOOKEEPER_SERVER_STATE] = {
        .name = "zookeeper_server_state",
        .type = METRIC_TYPE_STATE_SET,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_FOLLOWERS] = {
        .name = "zookeeper_followers",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of Followers.",
    },
    [FAM_ZOOKEEPER_SYNCED_FOLLOWERS] = {
        .name = "zookeeper_synced_followers",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_PENDING_SYNCS] = {
        .name = "zookeeper_pending_syncs",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_EPHEMERALS] = {
        .name = "zookeeper_ephemerals",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of ephemeral nodes that a ZooKeeper server has in its data tree.",
    },
    [FAM_ZOOKEEPER_ALIVE_CONNECTIONS] = {
        .name = "zookeeper_alive_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of connections",
    },
    [FAM_ZOOKEEPER_LATENCY_AVG_SECONDS] = {
        .name = "zookeeper_latency_avg_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Average time in seconds for requests to be processed.",
    },
    [FAM_ZOOKEEPER_OUTSTANDING_REQUESTS] = {
        .name = "zookeeper_outstanding_requests",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of queued requests when the server is under load and is receiving more sustained requests than it can process.",
    },
    [FAM_ZOOKEEPER_ZNODES] = {
        .name = "zookeeper_znodes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of z-nodes that a ZooKeeper server has in its data tree.",
    },
    [FAM_ZOOKEEPER_GLOBAL_SESSIONS] = {
        .name = "zookeeper_global_sessions",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of global sessions.",
    },
    [FAM_ZOOKEEPER_NON_MTLS_REMOTE_CONN] = {
        .name = "zookeeper_non_mtls_remote_conn",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_PACKETS_SENT] = {
        .name = "zookeeper_packets_sent",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of packets send.",
    },
    [FAM_ZOOKEEPER_PACKETS_RECEIVED] = {
        .name = "zookeeper_packets_received",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of packets received.",
    },
    [FAM_ZOOKEEPER_CONNECTION_DROP_PROBABILITY] = {
        .name = "zookeeper_connection_drop_probability",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_WATCHES] = {
        .name = "zookeeper_watches",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of watches placed on Z-Nodes on a ZooKeeper server",
    },
    [FAM_ZOOKEEPER_AUTH_FAILED] = {
        .name = "zookeeper_auth_failed",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_LATENCY_MIN_SECONDS] = {
        .name = "zookeeper_latency_min_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Minimun time in seconds for requests to be processed.",
    },
    [FAM_ZOOKEEPER_MAX_FILE_DESCRIPTORS] = {
        .name = "zookeeper_max_file_descriptors",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_APPROXIMATE_DATA_SIZE_BYTES] = {
        .name = "zookeeper_approximate_data_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_OPEN_FILE_DESCRIPTOR] = {
        .name = "zookeeper_open_file_descriptor",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of open file descriptors",
    },
    [FAM_ZOOKEEPER_LOCAL_SESSIONS] = {
        .name = "zookeeper_local_sessions",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of local sessions.",
    },
    [FAM_ZOOKEEPER_UPTIME_SECONDS] = {
        .name = "zookeeper_uptime_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Time that a peer has been in a table leading/following/observing stat.",
    },
    [FAM_ZOOKEEPER_LATENCY_MAX_SECONDS] = {
        .name = "zookeeper_latency_max_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Maximun time in seconds for requests to be processed.",
    },
    [FAM_ZOOKEEPER_OUTSTANDING_TLS_HANDSHAKE] = {
        .name = "zookeeper_outstanding_tls_handshake",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_NON_MTLS_LOCAL_CONN] = {
        .name = "zookeeper_non_mtls_local_conn",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_PROPOSAL] = {
        .name = "zookeeper_proposal",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_WATCH_BYTES] = {
        .name = "zookeeper_watch_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_OUTSTANDING_CHANGES_REMOVED] = {
        .name = "zookeeper_outstanding_changes_removed",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_THROTTLED_OPS] = {
        .name = "zookeeper_throttled_ops",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_STALE_REQUESTS_DROPPED] = {
        .name = "zookeeper_stale_requests_dropped",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_LARGE_REQUESTS_REJECTED] = {
        .name = "zookeeper_large_requests_rejected",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_INSECURE_ADMIN] = {
        .name = "zookeeper_insecure_admin",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_CONNECTION_REJECTED] = {
        .name = "zookeeper_connection_rejected",
        .type = METRIC_TYPE_COUNTER,
        .help = "Connections rejected",
    },
    [FAM_ZOOKEEPER_CNXN_CLOSED_WITHOUT_ZK_SERVER_RUNNING] = {
        .name = "zookeeper_cnxn_closed_without_zk_server_running",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_SESSIONLESS_CONNECTIONS_EXPIRED] = {
        .name = "zookeeper_sessionless_connections_expired",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_LOOKING] = {
        .name = "zookeeper_looking",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of transitions into looking state",
    },
    [FAM_ZOOKEEPER_DEAD_WATCHERS_QUEUED] = {
        .name = "zookeeper_dead_watchers_queued",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_STALE_REQUESTS] = {
        .name = "zookeeper_stale_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_CONNECTION_DROP] = {
        .name = "zookeeper_connection_drop",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_LEARNER_PROPOSAL_RECEIVED] = {
        .name = "zookeeper_learner_proposal_received",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_DIGEST_MISMATCHES] = {
        .name = "zookeeper_digest_mismatches",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_DEAD_WATCHERS_CLEARED] = {
        .name = "zookeeper_dead_watchers_cleared",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_RESPONSE_PACKET_CACHE_HITS] = {
        .name = "zookeeper_response_packet_cache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_RECEIVED_BYTES] = {
        .name = "zookeeper_received_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of bytes received.",
    },
    [FAM_ZOOKEEPER_ADD_DEAD_WATCHER_STALL_TIME] = {
        .name = "zookeeper_add_dead_watcher_stall_time",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_REQUEST_THROTTLE_WAIT] = {
        .name = "zookeeper_request_throttle_wait",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_REQUESTS_NOT_FORWARDED_TO_COMMIT_PROCESSOR] = {
        .name = "zookeeper_requests_not_forwarded_to_commit_processor",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_RESPONSE_PACKET_CACHE_MISSES] = {
        .name = "zookeeper_response_packet_cache_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_ENSEMBLE_AUTH_SUCCESS] = {
        .name = "zookeeper_ensemble_auth_success",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_PREP_PROCESSOR_REQUEST_QUEUED] = {
        .name = "zookeeper_prep_processor_request_queued",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_LEARNER_COMMIT_RECEIVED] = {
        .name = "zookeeper_learner_commit_received",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_STALE_REPLIES] = {
        .name = "zookeeper_stale_replies",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_CONNECTION_REQUEST] = {
        .name = "zookeeper_connection_request",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of incoming client connection requests.",
    },
    [FAM_ZOOKEEPER_RESPONSE_BYTES] = {
        .name = "zookeeper_response_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_ENSEMBLE_AUTH_FAIL] = {
        .name = "zookeeper_ensemble_auth_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_DIFF] = {
        .name = "zookeeper_diff",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of diff syncs performed.",
    },
    [FAM_ZOOKEEPER_RESPONSE_PACKET_GET_CHILDREN_CACHE_MISSES] = {
        .name = "zookeeper_response_packet_get_children_cache_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_CONNECTION_REVALIDATE] = {
        .name = "zookeeper_connection_revalidate",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of connection revalidations.\n",
    },
    [FAM_ZOOKEEPER_QUIT_LEADING_DUE_TO_DISLOYAL_VOTER] = {
        .name = "zookeeper_quit_leading_due_to_disloyal_voter",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_SNAP] = {
        .name = "zookeeper_snap",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of snap syncs performed"
    },
    [FAM_ZOOKEEPER_UNRECOVERABLE_ERROR] = {
        .name = "zookeeper_unrecoverable_error",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_UNSUCCESSFUL_HANDSHAKE] = {
        .name = "zookeeper_unsuccessful_handshake",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_COMMIT] = {
        .name = "zookeeper_commit",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_STALE_SESSIONS_EXPIRED] = {
        .name = "zookeeper_stale_sessions_expired",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_RESPONSE_PACKET_GET_CHILDREN_CACHE_HITS] = {
        .name = "zookeeper_response_packet_get_children_cache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_SYNC_PROCESSOR_REQUEST_QUEUED] = {
        .name = "zookeeper_sync_processor_request_queued",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_OUTSTANDING_CHANGES_QUEUED] = {
        .name = "zookeeper_outstanding_changes_queued",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_REQUEST_COMMIT_QUEUED] = {
        .name = "zookeeper_request_commit_queued",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_ENSEMBLE_AUTH_SKIP] = {
        .name = "zookeeper_ensemble_auth_skip",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_SKIP_LEARNER_REQUEST_TO_NEXT_PROCESSOR] = {
        .name = "zookeeper_skip_learner_request_to_next_processor",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_TLS_HANDSHAKE_EXCEEDED] = {
        .name = "zookeeper_tls_handshake_exceeded",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ZOOKEEPER_REVALIDATE] = {
        .name = "zookeeper_revalidate",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of connection revalidations.",
    },
};

#include "plugins/zookeeper/zookeeper_stats.h"

#define ZOOKEEPER_DEF_HOST "127.0.0.1"
#define ZOOKEEPER_DEF_PORT "2181"

typedef struct {
    char *instance;
    char *host;
    char *port;
    label_set_t labels;
    metric_family_t fams[FAM_ZOOKEEPER_MAX];
} zookeeper_instance_t;

static int zookeeper_connect(zookeeper_instance_t *conf)
{
    const char *host = (conf->host != NULL) ? conf->host : ZOOKEEPER_DEF_HOST;
    const char *port = (conf->port != NULL) ? conf->port : ZOOKEEPER_DEF_PORT;

    struct addrinfo *ai_list;
    struct addrinfo ai_hints = {.ai_family = AF_UNSPEC,
                                                            .ai_socktype = SOCK_STREAM};
    int status = getaddrinfo(host, port, &ai_hints, &ai_list);
    if (status != 0) {
        PLUGIN_INFO("getaddrinfo failed: %s",
                    (status == EAI_SYSTEM) ? STRERRNO : gai_strerror(status));
        return -1;
    }

    int sk = -1;
    for (struct addrinfo *ai = ai_list; ai != NULL; ai = ai->ai_next) {
        sk = socket(ai->ai_family, SOCK_STREAM, 0);
        if (sk < 0) {
            PLUGIN_WARNING("socket(2) failed: %s", STRERRNO);
            continue;
        }
        status = (int)connect(sk, ai->ai_addr, ai->ai_addrlen);
        if (status != 0) {
            close(sk);
            sk = -1;
            PLUGIN_WARNING("connect(2) failed: %s", STRERRNO);
            continue;
        }
        /* connected */
        break;
    }
    freeaddrinfo(ai_list);

    if (sk < 0)
        return -1;

    status = (int)swrite(sk, "mntr\r\n", strlen("mntr\r\n"));
    if (status != 0) {
        PLUGIN_ERROR("write(2) failed: %s", STRERRNO);
        close(sk);
        return -1;
    }

    return sk;
}

static int zookeeper_query(zookeeper_instance_t *conf, char *buffer, size_t buffer_size)
{
    int sk, status;
    size_t buffer_fill;

    sk = zookeeper_connect(conf);
    if (sk < 0) {
        PLUGIN_ERROR("Could not connect to daemon");
        return -1;
    }

    status = (int)swrite(sk, "mntr\r\n", strlen("mntr\r\n"));
    if (status != 0) {
        PLUGIN_ERROR("write(2) failed: %s", STRERRNO);
        close(sk);
        return -1;
    }

    memset(buffer, 0, buffer_size);
    buffer_fill = 0;

    while ((status = (int)recv(sk, buffer + buffer_fill,
                                   buffer_size - buffer_fill, /* flags = */ 0)) != 0) {
        if (status < 0) {
            if ((errno == EAGAIN) || (errno == EINTR))
                continue;
            PLUGIN_ERROR("Error reading from socket: %s", STRERRNO);
            close(sk);
            return -1;
        }

        buffer_fill += (size_t)status;
    } /* while (recv) */

    status = 0;
    if (buffer_fill == 0) {
        PLUGIN_WARNING("No data returned by MNTR command.");
        status = -1;
    }

    close(sk);
    return status;
}

static int zookeeper_read_instance(zookeeper_instance_t *conf)
{
    char buf[65536];
    char *line;

    if (zookeeper_query(conf, buf, sizeof(buf)) < 0)
        return -1;

    char *ptr = buf;
    char *save_ptr = NULL;
    while ((line = strtok_r(ptr, "\n\r", &save_ptr)) != NULL) {
        ptr = NULL;
        char *fields[2];
        if (strsplit(line, fields, 2) != 2)
            continue;

        const struct zkstats_metric *zm = zkstats_get_key (fields[0], strlen(fields[0]));
        if (zm != NULL) {
            metric_family_t *fam = &conf->fams[zm->fam];

            switch (zm->fam) {
            case FAM_ZOOKEEPER_VERSION: {
                label_set_t info = {
                    .num = 1,
                    .ptr = (label_pair_t[]){
                        {.name = "version", .value = fields[1]}
                    }
                };
                metric_family_append(fam, VALUE_INFO(info), &conf->labels, NULL);
            }   break;
            case FAM_ZOOKEEPER_SERVER_STATE: {
                state_t states[] = {
                    {.name = "leader"     , .enabled = false},
                    {.name = "follower"   , .enabled = false},
                    {.name = "standalone" , .enabled = false},
                };
                state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };
                state_set_enable(set, fields[1]);
                metric_family_append(fam, VALUE_STATE_SET(set), &conf->labels, NULL);
            }   break;
            default:
                if (fam->type == METRIC_TYPE_COUNTER) {
                    metric_family_append(fam, VALUE_COUNTER(atoll(fields[1])), &conf->labels, NULL);
                } else if (fam->type == METRIC_TYPE_GAUGE) {
                    metric_family_append(fam, VALUE_GAUGE(atof(fields[1])), &conf->labels, NULL);
                } else {
                    continue;
                }
                break;
            }
        }
    }

    return 0;
}

static int zookeeper_read(user_data_t *ud)
{
    if ((ud == NULL) || (ud->data == NULL))
        return EINVAL;

    zookeeper_instance_t *conf = ud->data;

    int status = zookeeper_read_instance(conf);

    metric_family_append(&conf->fams[FAM_ZOOKEEPER_UP],
                         VALUE_GAUGE(status < 0 ? 0 : 1), &conf->labels, NULL);

    plugin_dispatch_metric_family_array(conf->fams, FAM_ZOOKEEPER_MAX, 0);

    return 0;
}

static void zookeeper_config_free(void *ptr)
{
    zookeeper_instance_t *conf = ptr;

    if (conf == NULL)
        return;

    if (conf->instance != NULL)
        free(conf->instance);
    if (conf->host != NULL)
        free(conf->host);
    if (conf->port != NULL)
        free(conf->port);
    label_set_reset(&conf->labels);

    free(conf);
}

static int zookeper_config_instance(const config_item_t *ci)
{
    zookeeper_instance_t *conf = calloc(1, sizeof(*conf));
    if (conf == NULL)
        return ENOMEM;
    conf->instance = NULL;

    memcpy(conf->fams, fams, FAM_ZOOKEEPER_MAX * sizeof(*fams));

    if (ci->values_num == 1) {
        int status;

        status = cf_util_get_string(ci, &conf->instance);
        if (status != 0) {
            free(conf);
            return status;
        }
        assert(conf->instance != NULL);

    } else if (ci->values_num > 1) {
        PLUGIN_WARNING("'instance' blocks accept only one argument.");
        free(conf);
        return EINVAL;
    }

    cdtime_t interval = 0;
    int status = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp("host", child->key) == 0) {
            status = cf_util_get_string(child, &conf->host);
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_string(child, &conf->port);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &conf->labels);
        } else {
            WARNING("Zookeeper plugin: Option `%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        zookeeper_config_free(conf);
        return status;
    }

    label_set_add(&conf->labels, true, "instance", conf->instance);

    return plugin_register_complex_read("zookeeper", conf->instance, zookeeper_read, interval,
                                        &(user_data_t){.data=conf, .free_func=zookeeper_config_free});
}

static int zookeeper_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = zookeper_config_instance(child);
        } else {
            PLUGIN_ERROR("Unknown configuration option: '%s'", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("zookeeper", zookeeper_config);
}
