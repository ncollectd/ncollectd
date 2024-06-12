// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/itoa.h"
#include "libutils/avltree.h"
#include "libformat/format.h"

#include <librdkafka/rdkafka.h>

enum {
    FAM_KAFKA_UP,
    FAM_KAFKA_CLUSTER,
    FAM_KAFKA_BROKER,
    FAM_KAFKA_TOPIC,
    FAM_KAFKA_TOPIC_PARTITION_LOW_WATER_MARK,
    FAM_KAFKA_TOPIC_PARTITION_HIGH_WATER_MARK,
    FAM_KAFKA_CONSUMER_GROUP_STATE,
    FAM_KAFKA_CONSUMER_GROUP_MEMBERS,
    FAM_KAFKA_CONSUMER_GROUP_EMPTY_MEMBERS,
    FAM_KAFKA_CONSUMER_GROUP_TOPIC_PARTITION_OFFSET,
    FAM_KAFKA_CONSUMER_GROUP_TOPIC_PARTITION_LAG,
    FAM_KAFKA_CONSUMER_GROUP_TOPIC_LAG,
    FAM_KAFKA_MAX
};

static metric_family_t fams[FAM_KAFKA_MAX] = {
    [FAM_KAFKA_UP] = {
        .name = "kafka_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the kafka server be reached.",
    },
    [FAM_KAFKA_CLUSTER] = {
        .name = "kafka_cluster",
        .type = METRIC_TYPE_INFO,
        .help = "Kafka cluster information.",
    },
    [FAM_KAFKA_BROKER] = {
        .name = "kafka_broker",
        .type = METRIC_TYPE_INFO,
        .help = "Kafka broker information.",
    },
    [FAM_KAFKA_TOPIC] = {
        .name = "kafka_topic",
        .type = METRIC_TYPE_INFO,
        .help = "Info for a given topic.",
    },
    [FAM_KAFKA_TOPIC_PARTITION_LOW_WATER_MARK] = {
        .name = "kafka_topic_partition_low_water_mark",
        .type = METRIC_TYPE_GAUGE,
        .help = "Partition Low Water Mark.",
    },
    [FAM_KAFKA_TOPIC_PARTITION_HIGH_WATER_MARK] = {
        .name = "kafka_topic_partition_high_water_mark",
        .type = METRIC_TYPE_GAUGE,
        .help = "Partition High Water Mark.",
    },
    [FAM_KAFKA_CONSUMER_GROUP_STATE] = {
        .name = "kafka_consumer_group_state",
        .type = METRIC_TYPE_STATE_SET,
        .help = "Consumer Group state",
    },
    [FAM_KAFKA_CONSUMER_GROUP_MEMBERS] = {
        .name = "kafka_consumer_group_members",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of members in the consumer group.",
    },
    [FAM_KAFKA_CONSUMER_GROUP_EMPTY_MEMBERS] = {
        .name = "kafka_consumer_group_empty_members",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of members in the consumer group with no partition assigned.",
    },
    [FAM_KAFKA_CONSUMER_GROUP_TOPIC_PARTITION_OFFSET] = {
        .name = "kafka_consumer_group_topic_partition_offset",
        .type = METRIC_TYPE_GAUGE,
        .help = "The committed group offsets for a partitions in a topic.",
    },
    [FAM_KAFKA_CONSUMER_GROUP_TOPIC_PARTITION_LAG] = {
        .name = "kafka_consumer_group_topic_partition_lag",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of messages a consumer group is lagging behind "
                "the latest offset of a partition",
    },
    [FAM_KAFKA_CONSUMER_GROUP_TOPIC_LAG] = {
        .name = "kafka_consumer_group_topic_lag",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of messages a consumer group is lagging behind across "
                "all partitions in a topic",
    },

};

#if 0
    // OffsetConsumer records consumed
    e.offsetConsumerRecordsConsumed = prometheus.NewDesc(
        prometheus.BuildFQName(e.cfg.Namespace, "exporter", "offset_consumer_records_consumed_total"),
        "The number of offset records that have been consumed by the internal offset consumer",
        []string{},
        nil,
    )


        .name = "kafka_consumer_group_topic_members",
        .type = METRIC_TYPE_GAUGE,
        .help = "It will report the number of members in the consumer group assigned on a given topic",
        []string{"group_id", "topic_name"},
    },
        .name = "kafka_consumer_group_topic_assigned_partitions",
        .type = METRIC_TYPE_GAUGE,
        .help = "It will report the number of partitions assigned in the consumer group for a given topic",
        []string{"group_id", "topic_name"},
    },

        .name = "kafka_consumer_group_offset_commits",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of offsets committed by a group",
        []string{"group_id"},
    },
#endif

typedef struct {
    char *topic;
    int32_t partition;
    int64_t offset;
} kafka_topic_offset_t;

typedef struct {
    char *group;
    char *topic;
    int64_t lag;
} kafka_group_lag_t;

typedef struct {
    const char *group;
    const char *topic;
    int64_t lag;
} kafka_group_lag_const_t;

typedef struct {
    char *instance;
    label_set_t labels;
    plugin_filter_t *filter;
    rd_kafka_conf_t *conf;
    rd_kafka_t *rk;
    c_avl_tree_t *topic_offsets;
    c_avl_tree_t *group_lag;
    metric_family_t fams[FAM_KAFKA_MAX];
} kafka_ctx_t;

#if RD_KAFKA_VERSION > 0x020000ff

static int kafka_topic_offset_cmp(const void *a, const void *b)
{
    const kafka_topic_offset_t *oa = a;
    const kafka_topic_offset_t *ob = b;

    int ret = strcmp(oa->topic, ob->topic);
    if (ret == 0) {
        if (oa->partition > ob->partition) {
            return 1;
        } else if (oa->partition < ob->partition) {
            return -1;
        }
        return 0;
    }

    return ret;
}

static int kafka_topic_offset_append(c_avl_tree_t *tree, char *topic,
                                     int32_t partition, int64_t offset)
{
    if (topic == NULL)
        return -1;

    kafka_topic_offset_t key = {.topic = topic, .partition = partition};
    kafka_topic_offset_t *value = NULL ;
    int status = c_avl_get(tree, (void *)&key, (void *)&value);
    if (status == 0) {
        value->offset = offset;
        return 0;
    }

    kafka_topic_offset_t *new_key = calloc(1, sizeof(*new_key));
    if (new_key == NULL)
        return -1;

    new_key->topic = strdup(topic);
    if (new_key->topic == NULL) {
        free(new_key);
        return -1;
    }
    new_key->partition = partition;
    new_key->offset = offset;

    status = c_avl_insert(tree, new_key, new_key);
    if (unlikely(status != 0)) {
        free(new_key->topic);
        free(new_key);
        return -1;
    }

    return 0;
}

static int64_t kafka_topic_offset_get(c_avl_tree_t *tree, char *topic, int32_t partition)
{
    kafka_topic_offset_t key = {.topic = topic, .partition = partition};
    kafka_topic_offset_t *value = NULL ;
    int status = c_avl_get(tree, (void *)&key, (void *)&value);
    if (status == 0)
        return value->offset;

    return -1;
}

static void kafka_topic_offset_reset(c_avl_tree_t *tree)
{
    while (true) {
        kafka_topic_offset_t *key = NULL;
        kafka_topic_offset_t *value = NULL;
        int status = c_avl_pick(tree, (void *)&key, (void *)&value);
        if (status != 0)
            break;
        if (key != NULL) {
            free(key->topic);
            free(key);
        }
    }
}

static int kafka_group_lag_cmp(const void *a, const void *b)
{
    const kafka_group_lag_t *ga = a;
    const kafka_group_lag_t *gb = b;

    int ret = strcmp(ga->topic, gb->topic);
    if (ret == 0)
        return strcmp(ga->group, gb->topic);

    return ret;
}

static int kafka_group_lag_add(c_avl_tree_t *tree, const char *topic, const char *group,
                                                   int64_t lag)
{
    if ((topic == NULL) || (group == NULL))
        return -1;

    kafka_group_lag_const_t key = {.topic = topic, .group = group};
    kafka_group_lag_t *value = NULL ;
    int status = c_avl_get(tree, (void *)&key, (void *)&value);
    if (status == 0) {
        value->lag += lag;
        return 0;
    }

    kafka_group_lag_t *new_key = calloc(1, sizeof(*new_key));
    if (new_key == NULL)
        return -1;

    new_key->topic = strdup(topic);
    if (new_key->topic == NULL) {
        free(new_key);
        return -1;
    }
    new_key->group = strdup(group);
    if (new_key->group == NULL) {
        free(new_key->topic);
        free(new_key);
        return -1;
    }
    new_key->lag = lag;

    status = c_avl_insert(tree, new_key, new_key);
    if (unlikely(status != 0)) {
        free(new_key->topic);
        free(new_key->group);
        free(new_key);
        return -1;
    }

    return 0;
}

static void kafka_group_lag_reset(c_avl_tree_t *tree, metric_family_t *fam, label_set_t *labels,
                                                      const char *cluster_id)
{
    while (true) {
        kafka_group_lag_t *key = NULL;
        kafka_group_lag_t *value = NULL;
        int status = c_avl_pick(tree, (void *)&key, (void *)&value);
        if (status != 0)
            break;

        if (key != NULL) {
            metric_family_append(fam, VALUE_GAUGE(key->lag), labels,
                                 &(label_pair_const_t){.name="cluster_id", .value=cluster_id},
                                 &(label_pair_const_t){.name="group_id", .value=key->group},
                                 &(label_pair_const_t){.name="topic", .value=key->topic},
                                 NULL);

            free(key->topic);
            free(key->group);
            free(key);
        }
    }
}

static int kafka_describe_consumer_groups(kafka_ctx_t *ctx, const char *cluster_id)
{
    rd_kafka_event_t *event = NULL;
    rd_kafka_resp_err_t resp_err;
    char errbuf[1024];
    int status = 0;

    rd_kafka_queue_t *queue = rd_kafka_queue_new(ctx->rk);

    rd_kafka_AdminOptions_t *options =
        rd_kafka_AdminOptions_new(ctx->rk, RD_KAFKA_ADMIN_OP_DESCRIBECONSUMERGROUPS);

    resp_err = rd_kafka_AdminOptions_set_request_timeout(options, 10*1000/*10s*/,errbuf, sizeof(errbuf));
    if (resp_err != 0) {
        PLUGIN_ERROR("Failed to set timeout: %s.", errbuf);
        status = -1;
        goto exit;
    }

    rd_kafka_DescribeConsumerGroups(ctx->rk, NULL, 0, options, queue);

    event = rd_kafka_queue_poll(queue, -1);
    if (event != NULL) {
        status = -1;
        goto exit;
    }

    resp_err = rd_kafka_event_error(event);
    if (resp_err != 0) {
        PLUGIN_ERROR("DescribeConsumerGroups failed[%" PRId32 "]: %s\n",
                     resp_err, rd_kafka_event_error_string(event));
        status = -1;
        goto exit;
    }

    const rd_kafka_DescribeConsumerGroups_result_t *result =
        rd_kafka_event_DescribeConsumerGroups_result(event);

    size_t result_groups_cnt = 0;
    const rd_kafka_ConsumerGroupDescription_t **result_groups =
        rd_kafka_DescribeConsumerGroups_result_groups(result, &result_groups_cnt);
    if (result_groups_cnt == 0) {
        status = 0;
        goto exit;
    }

    for (size_t i = 0; i < result_groups_cnt; i++) {
        const rd_kafka_ConsumerGroupDescription_t *group = result_groups[i];

        const char *group_id = rd_kafka_ConsumerGroupDescription_group_id(group);
//      const char *partition_assignor = rd_kafka_ConsumerGroupDescription_partition_assignor(group);

        rd_kafka_consumer_group_state_t state = rd_kafka_ConsumerGroupDescription_state(group);

        size_t member_cnt = rd_kafka_ConsumerGroupDescription_member_count(group);

//      const rd_kafka_error_t *error = rd_kafka_ConsumerGroupDescription_error(group);
//      const rd_kafka_Node_t *coordinator = rd_kafka_ConsumerGroupDescription_coordinator(group);

#if 0
        if (coordinator != NULL) {
            snprintf(coordinator_desc, sizeof(coordinator_desc),
                     ", coordinator [id: %" PRId32 ", host: %s, port: %" PRIu16 "]",
                     rd_kafka_Node_id(coordinator),
                     rd_kafka_Node_host(coordinator),
                     rd_kafka_Node_port(coordinator));
        }
        printf("Group \"%s\", partition assignor \"%s\", state %s%s, with %" PRId32 " member(s)", group_id, partition_assignor, rd_kafka_consumer_group_state_name(state), coordinator_desc, member_cnt);
        if (error)
                printf(" error[%" PRId32 "]: %s", rd_kafka_error_code(error), rd_kafka_error_string(error));
#endif

        state_t states[] = {
            { .name = "UNKNOWN",              .enabled = false },
            { .name = "PREPARING_REBALANCE",  .enabled = false },
            { .name = "COMPLETING_REBALANCE", .enabled = false },
            { .name = "STABLE",               .enabled = false },
            { .name = "DEAD",                 .enabled = false },
            { .name = "EMPTY",                .enabled = false },
        };
        state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };

        switch(state) {
        case RD_KAFKA_CONSUMER_GROUP_STATE_UNKNOWN:
            states[0].enabled = true;
            break;
        case RD_KAFKA_CONSUMER_GROUP_STATE_PREPARING_REBALANCE:
            states[1].enabled = true;
            break;
        case RD_KAFKA_CONSUMER_GROUP_STATE_COMPLETING_REBALANCE:
            states[2].enabled = true;
            break;
        case RD_KAFKA_CONSUMER_GROUP_STATE_STABLE:
            states[3].enabled = true;
            break;
        case RD_KAFKA_CONSUMER_GROUP_STATE_DEAD:
            states[4].enabled = true;
            break;
        case RD_KAFKA_CONSUMER_GROUP_STATE_EMPTY:
            states[5].enabled = true;
            break;
        default:
            states[0].enabled = true;
            break;
        }

        metric_family_append(&ctx->fams[FAM_KAFKA_CONSUMER_GROUP_STATE],
                             VALUE_STATE_SET(set), &ctx->labels,
                             &(label_pair_const_t){.name="cluster_id", .value=cluster_id},
                             &(label_pair_const_t){.name="group_id", .value=group_id }, NULL);

        metric_family_append(&ctx->fams[FAM_KAFKA_CONSUMER_GROUP_MEMBERS],
                             VALUE_GAUGE(member_cnt), &ctx->labels,
                             &(label_pair_const_t){.name="cluster_id", .value=cluster_id},
                             &(label_pair_const_t){.name="group_id", .value=group_id }, NULL);

        size_t members_empty = 0;

        for (size_t j = 0; j < member_cnt; j++) {
            const rd_kafka_MemberDescription_t *member = NULL;
            const rd_kafka_MemberAssignment_t *assignment = NULL;
            const rd_kafka_topic_partition_list_t *partitions = NULL;

            member = rd_kafka_ConsumerGroupDescription_member(group, j);
#if 0
            printf("  Member \"%s\" with client-id %s, group instance id: %s, host %s\n",
                rd_kafka_MemberDescription_consumer_id(member),
                rd_kafka_MemberDescription_client_id(member),
                rd_kafka_MemberDescription_group_instance_id(member),
                rd_kafka_MemberDescription_host(member));
#endif
            assignment = rd_kafka_MemberDescription_assignment(member);
            partitions = rd_kafka_MemberAssignment_partitions(assignment);

            if (!partitions) {
                members_empty++; // No assignment
            } else if (partitions->cnt == 0) {
                members_empty++; // Empty assignment
            } else {
//                printf("    Assignment:\n");
//                print_partition_list(stdout, topic_partitions, 0, "      ");

                for (int k = 0; k < partitions->cnt; k++) {
                    char partition_id[ITOA_MAX];
                    itoa(partitions->elems[k].partition, partition_id);
                    char *topic = partitions->elems[k].topic;

                    metric_family_append(&ctx->fams[FAM_KAFKA_CONSUMER_GROUP_TOPIC_PARTITION_OFFSET],
                                         VALUE_GAUGE(partitions->elems[k].offset),
                                         &ctx->labels,
                                         &(label_pair_const_t){.name="cluster_id", .value=cluster_id},
                                         &(label_pair_const_t){.name="group_id", .value=group_id},
                                         &(label_pair_const_t){.name="topic", .value=topic},
                                         &(label_pair_const_t){.name="partition_id", .value=partition_id},
                                         NULL);
                    // rd_kafka_err2str(partitions->elems[k].err));

                    int64_t offset = kafka_topic_offset_get(ctx->topic_offsets, topic,
                                                            partitions->elems[k].partition);
                    if (offset < 0)
                        continue;
                    int64_t lag = offset - partitions->elems[k].offset;
                    if (lag < 0)
                        lag = 0;
                    kafka_group_lag_add(ctx->group_lag, topic, group_id, lag);

                    metric_family_append(&ctx->fams[FAM_KAFKA_CONSUMER_GROUP_TOPIC_PARTITION_LAG],
                                         VALUE_GAUGE(lag), &ctx->labels,
                                         &(label_pair_const_t){.name="cluster_id", .value=cluster_id},
                                         &(label_pair_const_t){.name="group_id", .value=group_id},
                                         &(label_pair_const_t){.name="topic", .value=topic},
                                         &(label_pair_const_t){.name="partition_id", .value=partition_id},
                                         NULL);
                }
            }
        }

        kafka_group_lag_reset(ctx->group_lag, &ctx->fams[FAM_KAFKA_CONSUMER_GROUP_TOPIC_LAG],
                                              &ctx->labels, cluster_id);

        metric_family_append(&ctx->fams[FAM_KAFKA_CONSUMER_GROUP_EMPTY_MEMBERS],
                             VALUE_GAUGE(members_empty), &ctx->labels,
                             &(label_pair_const_t){.name="cluster_id", .value=cluster_id},
                             &(label_pair_const_t){.name="group_id", .value=group_id }, NULL);
    }

    status = 0;
exit:
    if (event != NULL)
        rd_kafka_event_destroy(event);
    if (options != NULL)
        rd_kafka_AdminOptions_destroy(options);
    if (queue != NULL)
        rd_kafka_queue_destroy(queue);

    return status;
}

#endif

static int kafka_read(user_data_t *user_data)
{
    kafka_ctx_t *ctx = user_data->data;

    if (ctx->rk == NULL) {
        char errbuf[1024];
        ctx->rk = rd_kafka_new(RD_KAFKA_PRODUCER, ctx->conf, errbuf, sizeof(errbuf));
        if (ctx->rk == NULL) {
            PLUGIN_WARNING("Failed to create new producer: %s.", errbuf);
            return 0;
        }
    }

    char *cluster_id = rd_kafka_clusterid (ctx->rk, 10 * 1000);
    if (cluster_id == NULL) {
        metric_family_append(&ctx->fams[FAM_KAFKA_UP], VALUE_GAUGE(0), &ctx->labels,
                             &(label_pair_const_t){.name="cluster_id", .value=cluster_id},
                             NULL);
        plugin_dispatch_metric_family_filtered(&ctx->fams[FAM_KAFKA_UP], ctx->filter, 0);
        return 0;
    }

    metric_family_append(&ctx->fams[FAM_KAFKA_UP], VALUE_GAUGE(1), &ctx->labels,
                         &(label_pair_const_t){.name="cluster_id", .value=cluster_id},
                         NULL);

#if 0
char *rd_kafka_clusterid (rd_kafka_t *rk, int timeout_ms);
rd_kafka_mem_free (rd_kafka_t *rk, void *ptr);

int32_t rd_kafka_controllerid (rd_kafka_t *rk, int timeout_ms);

FAM_KAFKA_CLUSTER
"cluster_version", "broker_count", "controller_id", "cluster_id"
#endif

    const struct rd_kafka_metadata *metadata;
    rd_kafka_resp_err_t err = rd_kafka_metadata(ctx->rk, 1, NULL, &metadata, 5000);
    if (err != RD_KAFKA_RESP_ERR_NO_ERROR) {
        PLUGIN_ERROR("Failed to acquire metadata: %s\n", rd_kafka_err2str(err));
        return 0;
    }

    for (int i = 0; i < metadata->broker_cnt; i++) {
        char *controller = metadata->brokers[i].id ? "true" : "false";
        char broker_id[ITOA_MAX];
        itoa(metadata->brokers[i].id, broker_id);
        char port[ITOA_MAX];
        itoa(metadata->brokers[i].port, port);

        label_set_t info = {
            .num = 4,
            .ptr = (label_pair_t[]) {
                {.name = "broker_id",     .value = broker_id                },
                {.name = "address",       .value = metadata->brokers[i].host},
                {.name = "port",          .value = port                     },
                {.name = "is_controller", .value = controller               }
            },
       };

        metric_family_append(&ctx->fams[FAM_KAFKA_BROKER], VALUE_INFO(info), &ctx->labels,
                             &(label_pair_const_t){.name="cluster_id", .value=cluster_id}, NULL);
    }

    for (int i = 0; i < metadata->topic_cnt; i++) {
        const struct rd_kafka_metadata_topic *topic = &metadata->topics[i];

        char partition_cnt[ITOA_MAX];
        itoa(topic->partition_cnt, partition_cnt);

        label_set_t info = {
            .num = 2,
            .ptr = (label_pair_t[]) {
                {.name = "topic",      .value = topic->topic },
                {.name = "partitions", .value = partition_cnt},
            }
        };
        metric_family_append(&ctx->fams[FAM_KAFKA_TOPIC], VALUE_INFO(info), &ctx->labels,
                             &(label_pair_const_t){.name="cluster_id", .value=cluster_id}, NULL);

        //  topic->err == RD_KAFKA_RESP_ERR_LEADER_NOT_AVAILABLE)

        for (int j = 0; j < topic->partition_cnt; j++) {
            const struct rd_kafka_metadata_partition *partition = &topic->partitions[j];

            int64_t low = 0;
            int64_t high = 0;

            err = rd_kafka_query_watermark_offsets (ctx->rk, topic->topic, j, &low, &high, 5000);
            if (err != RD_KAFKA_RESP_ERR_NO_ERROR) {
                PLUGIN_WARNING("Failed to get watermark offsets: %s\n", rd_kafka_err2str(err));
                continue;
            }

            char partition_id[ITOA_MAX];
            itoa(partition->id, partition_id);

            metric_family_append(&ctx->fams[FAM_KAFKA_TOPIC_PARTITION_LOW_WATER_MARK],
                                 VALUE_GAUGE(low), &ctx->labels,
                                 &(label_pair_const_t){.name="cluster_id",   .value=cluster_id  },
                                 &(label_pair_const_t){.name="topic",        .value=topic->topic},
                                 &(label_pair_const_t){.name="partition_id", .value=partition_id},
                                 NULL);
            metric_family_append(&ctx->fams[FAM_KAFKA_TOPIC_PARTITION_HIGH_WATER_MARK],
                                 VALUE_GAUGE(high), &ctx->labels,
                                 &(label_pair_const_t){.name="cluster_id",   .value=cluster_id  },
                                 &(label_pair_const_t){.name="topic",        .value=topic->topic},
                                 &(label_pair_const_t){.name="partition_id", .value=partition_id},
                                 NULL);

#if RD_KAFKA_VERSION > 0x020000ff
            kafka_topic_offset_append(ctx->topic_offsets, topic->topic, partition->id, high);
#endif
        }
    }

    rd_kafka_metadata_destroy(metadata);

#if RD_KAFKA_VERSION > 0x020000ff
    kafka_describe_consumer_groups(ctx, cluster_id);
    kafka_topic_offset_reset(ctx->topic_offsets);
#endif

    plugin_dispatch_metric_family_array_filtered(ctx->fams, FAM_KAFKA_MAX, ctx->filter, 0);

    if (cluster_id != NULL)
        rd_kafka_mem_free (ctx->rk, cluster_id);

    return 0;
}

#ifdef HAVE_LIBRDKAFKA_LOG_CB
static void kafka_log(__attribute__((unused)) const rd_kafka_t *rk, int level,
                      __attribute__((unused)) const char *fac, const char *buf)
{
    plugin_log(level, NULL, 0, NULL, "%s", buf);
}
#endif

static void kafka_free(void *arg)
{
    kafka_ctx_t *ctx = arg;

    if (ctx == NULL)
        return;

    free(ctx->instance);

    label_set_reset(&ctx->labels);
    plugin_filter_free(ctx->filter);

    if (ctx->rk != NULL) {
        rd_kafka_destroy(ctx->rk);
    } else if (ctx->conf != NULL) {
        rd_kafka_conf_destroy(ctx->conf);
    }

    if (ctx->topic_offsets != NULL)
        c_avl_destroy(ctx->topic_offsets);

    if (ctx->group_lag != NULL)
        c_avl_destroy(ctx->group_lag);

    free(ctx);
}

static int kafka_config_property(config_item_t *ci, rd_kafka_conf_t *conf)
{
    if (ci->values_num != 2) {
        PLUGIN_WARNING("kafka properties need both a key and a value.");
        return -1;
    }

    if ((ci->values[0].type != CONFIG_TYPE_STRING) ||
        (ci->values[1].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("kafka properties needs string arguments.");
        return -1;
    }

    const char *key = ci->values[0].value.string;
    const char *val = ci->values[1].value.string;

    char errbuf[1024];
    rd_kafka_conf_res_t ret = rd_kafka_conf_set(conf, key, val, errbuf, sizeof(errbuf));
    if (ret != RD_KAFKA_CONF_OK) {
        PLUGIN_WARNING("cannot set kafka property %s to %s: %s.", key, val, errbuf);
        return 1;
    }

    return 0;
}

static int kafka_config_instance(config_item_t *ci)
{
    kafka_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = cf_util_get_string(ci, &ctx->instance);
    if (status != 0) {
        kafka_free(ctx);
        return status;
    }

    memcpy(ctx->fams, fams, sizeof(ctx->fams[0])*FAM_KAFKA_MAX);

    ctx->conf = rd_kafka_conf_new();
    if (ctx->conf == NULL) {
        PLUGIN_ERROR("cannot allocate kafka configuration.");
        kafka_free(ctx);
        return -1;
    }

#ifdef HAVE_LIBRDKAFKA_LOG_CB
    rd_kafka_conf_set_log_cb(ctx->conf, kafka_log);
#endif

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = &ci->children[i];

        if (strcasecmp("property", child->key) == 0) {
            status = kafka_config_property(child, ctx->conf);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &ctx->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &ctx->filter);
        } else {
            PLUGIN_WARNING("Invalid directive: %s.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        kafka_free(ctx);
        return -1;
    }

#if RD_KAFKA_VERSION > 0x020000ff
    ctx->topic_offsets = c_avl_create(kafka_topic_offset_cmp);
    if (unlikely(ctx->topic_offsets == NULL)) {
        kafka_free(ctx);
        return -1;
    }

    ctx->group_lag = c_avl_create(kafka_group_lag_cmp);
    if (unlikely(ctx->group_lag == NULL)) {
        kafka_free(ctx);
        return -1;
    }
#endif

    label_set_add(&ctx->labels, true, "instance", ctx->instance);

    return plugin_register_complex_read ("kafka", ctx->instance, kafka_read, interval,
                                         &(user_data_t){.data = ctx, .free_func = kafka_free});
}

static int kafka_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = kafka_config_instance(child);
        } else {
            PLUGIN_ERROR("Invalid configuration option: %s.", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("kafka", kafka_config);
}
