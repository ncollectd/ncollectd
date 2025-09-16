// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2016-2020  Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

#include "ncollectd.h"
#include "libutils/strbuf.h"
#include "libutils/common.h"
#include "libmetric/metric.h"
#include "libformat/json.h"
#include "libtest/testing.h"

DEF_TEST(json_unknow)
{
    metric_family_t fam = {
        .name = "metric_unknow",
        .type = METRIC_TYPE_UNKNOWN,
    };

    metric_t m = (metric_t){
        .value.gauge.float64 = 42,
        .time = 1710200311404036096, /* 1592748157.125 */
    };

    metric_family_metric_append(&fam, m);

    strbuf_t buf = STRBUF_CREATE;

    EXPECT_EQ_INT(0, json_metric_family(&buf, &fam));
    EXPECT_EQ_STR("{\"metric\":\"metric_unknow\",\"type\":\"unknown\",\"metrics\":[{\"labels\":{},\"timestamp\":1592748157125,\"interval\":0,\"value\":42}]}", buf.ptr);

    strbuf_destroy(&buf);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(json_gauge)
{
    metric_family_t fam = {
        .name = "metric_gauge",
        .type = METRIC_TYPE_GAUGE,
    };

    metric_t m = {
        .value.gauge.float64 = 42,
        .time = 1710200311404036096, /* 1592748157.125 */
    };

    metric_family_metric_append(&fam, m);

    strbuf_t buf = STRBUF_CREATE;

    EXPECT_EQ_INT(0, json_metric_family(&buf, &fam));
    EXPECT_EQ_STR("{\"metric\":\"metric_gauge\",\"type\":\"gauge\",\"metrics\":[{\"labels\":{},\"timestamp\":1592748157125,\"interval\":0,\"value\":42}]}", buf.ptr);

    strbuf_destroy(&buf);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(json_counter_with_label)
{
    metric_family_t fam = {
        .name = "metric_counter_with_label",
        .type = METRIC_TYPE_COUNTER,
    };


    metric_t m = {
        .value.counter.uint64 = 0,
        .time = 1710200311404036096, /* 1592748157.125 */
    };

    label_set_add(&m.label, true, "alpha", "first");
    label_set_add(&m.label, true, "beta", "second");

    metric_family_metric_append(&fam, m);

    strbuf_t buf = STRBUF_CREATE;

    EXPECT_EQ_INT(0, json_metric_family(&buf, &fam));
    EXPECT_EQ_STR("{\"metric\":\"metric_counter_with_label\",\"type\":\"counter\",\"metrics\":[{\"labels\":{\"alpha\":\"first\",\"beta\":\"second\"},\"timestamp\":1592748157125,\"interval\":0,\"value\":0}]}", buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(json_escaped_label_value)
{
    metric_family_t fam = {
        .name = "escaped_label_value",
        .type = METRIC_TYPE_COUNTER,
    };

    metric_t m = {
        .value = (value_t){.counter.uint64 = 42},
        .time = 1710200311404036096, /* 1592748157.125 */
    };

    label_set_add(&m.label, true, "alpha", "first/value");
    label_set_add(&m.label, true, "beta", "second value");

    metric_family_metric_append(&fam, m);

    strbuf_t buf = STRBUF_CREATE;

    EXPECT_EQ_INT(0, json_metric_family(&buf, &fam));
    EXPECT_EQ_STR("{\"metric\":\"escaped_label_value\",\"type\":\"counter\",\"metrics\":[{\"labels\":{\"alpha\":\"first/value\",\"beta\":\"second value\"},\"timestamp\":1592748157125,\"interval\":0,\"value\":42}]}", buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(json_system_uname)
{
    metric_family_t fam = {
        .name = "system_uname",
        .type = METRIC_TYPE_INFO,
    };

    metric_t m = {
        .time = 1710200311404036096, /* 1592748157.125 */
    };

    label_set_add(&m.label, true, "hostname", "arrakis.canopus");
    label_set_add(&m.value.info, true, "machine",  "riscv128");
    label_set_add(&m.value.info, true, "nodename", "arrakis.canopus");
    label_set_add(&m.value.info, true, "release",  "998");
    label_set_add(&m.value.info, true, "sysname",  "Linux");
    label_set_add(&m.value.info, true, "version",  "#1 SMP PREEMPT_DYNAMIC 10191");

    metric_family_metric_append(&fam, m);

    strbuf_t buf = STRBUF_CREATE;

    EXPECT_EQ_INT(0, json_metric_family(&buf, &fam));
    EXPECT_EQ_STR("{\"metric\":\"system_uname\",\"type\":\"info\",\"metrics\":[{\"labels\":{\"hostname\":\"arrakis.canopus\"},\"timestamp\":1592748157125,\"interval\":0,\"info\":{\"machine\":\"riscv128\",\"nodename\":\"arrakis.canopus\",\"release\":\"998\",\"sysname\":\"Linux\",\"version\":\"#1 SMP PREEMPT_DYNAMIC 10191\"}}]}", buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.value.info);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(json_stateset)
{
    metric_family_t fam = {
        .name = "stateset",
        .type = METRIC_TYPE_STATE_SET,
    };

    state_t state[] = {
        {"a",   false},
        {"bb",  true },
        {"ccc", false},
    };

    metric_t m = {
        .value.state_set.num = 3,
        .value.state_set.ptr = state,
        .time = 1710200311404036096, /* 1592748157.125 */
    };

    label_set_add(&m.label, true, "hostname", "arrakis.canopus");

    metric_family_metric_append(&fam, m);

    strbuf_t buf = STRBUF_CREATE;

    EXPECT_EQ_INT(0, json_metric_family(&buf, &fam));
    EXPECT_EQ_STR("{\"metric\":\"stateset\",\"type\":\"stateset\",\"metrics\":[{\"labels\":{\"hostname\":\"arrakis.canopus\"},\"timestamp\":1592748157125,\"interval\":0,\"stateset\":{\"a\":false,\"bb\":true,\"ccc\":false}}]}", buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(json_summary)
{
    metric_family_t fam = {
        .name = "summary",
        .type = METRIC_TYPE_SUMMARY,
    };

    summary_t *summary = summary_new();
    summary = summary_quantile_append(summary, 0.5,  0.232227334);
    summary = summary_quantile_append(summary, 0.90, 0.821139321);
    summary = summary_quantile_append(summary, 0.95, 1.528948804);
    summary = summary_quantile_append(summary, 0.99, 2.829188272);
    summary = summary_quantile_append(summary, 1,    34.283829292);
    summary->sum = 8953.332;
    summary->count = 27892;

    metric_t m = {
        .value.summary = summary,
        .time = 1710200311404036096, /* 1592748157.125 */
    };

    label_set_add(&m.label, true, "hostname", "arrakis.canopus");

    metric_family_metric_append(&fam, m);

    strbuf_t buf = STRBUF_CREATE;

    EXPECT_EQ_INT(0, json_metric_family(&buf, &fam));
    EXPECT_EQ_STR("{\"metric\":\"summary\",\"type\":\"summary\",\"metrics\":[{\"labels\":{\"hostname\":\"arrakis.canopus\"},\"timestamp\":1592748157125,\"interval\":0,\"quantiles\":[[1,34.283829292],[0.99,2.829188272],[0.95,1.528948804],[0.9,0.821139321],[0.5,0.232227334]],\"count\":27892,\"sum\":8953}]}", buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.label);
    summary_destroy(summary);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(json_histogram)
{
    metric_family_t fam = {
        .name = "histogram",
        .type = METRIC_TYPE_HISTOGRAM,
    };

    histogram_t *histogram = histogram_new();
    histogram = histogram_bucket_append(histogram, INFINITY, 27892);
    histogram = histogram_bucket_append(histogram, 25, 27890);
    histogram = histogram_bucket_append(histogram, 10, 27881);
    histogram = histogram_bucket_append(histogram, 5, 27814);
    histogram = histogram_bucket_append(histogram, 2.5, 27534);
    histogram = histogram_bucket_append(histogram, 1, 26351);
    histogram = histogram_bucket_append(histogram, 0.5, 24101);
    histogram = histogram_bucket_append(histogram, 0.25, 14251);
    histogram = histogram_bucket_append(histogram, 0.1, 8954);
    histogram = histogram_bucket_append(histogram, 0.05, 1672);
    histogram = histogram_bucket_append(histogram, 0.025, 8);
    histogram = histogram_bucket_append(histogram, 0.01, 0);
    histogram->sum = 8953.332;

    metric_t m = {
        .value.histogram = histogram,
        .time = 1710200311404036096, /* 1592748157.125 */
    };

    label_set_add(&m.label, true, "hostname", "arrakis.canopus");

    metric_family_metric_append(&fam, m);

    strbuf_t buf = STRBUF_CREATE;

    EXPECT_EQ_INT(0, json_metric_family(&buf, &fam));
    EXPECT_EQ_STR("{\"metric\":\"histogram\",\"type\":\"histogram\",\"metrics\":[{\"labels\":{\"hostname\":\"arrakis.canopus\"},\"timestamp\":1592748157125,\"interval\":0,\"buckets\":[[0.01,0],[0.025,8],[0.05,1672],[0.1,8954],[0.25,14251],[0.5,24101],[1,26351],[2.5,27534],[5,27814],[10,27881],[25,27890],[inf,27892]],\"count\":27892,\"sum\":27892}]}", buf.ptr);

    strbuf_destroy(&buf);
    histogram_destroy(histogram);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(json_guage_histogram)
{
    metric_family_t fam = {
        .name = "gauge_histogram",
        .type = METRIC_TYPE_GAUGE_HISTOGRAM,
    };

    histogram_t *histogram = histogram_new();
    histogram = histogram_bucket_append(histogram, INFINITY,120);
    histogram = histogram_bucket_append(histogram, 1048576, 115);
    histogram = histogram_bucket_append(histogram, 786432, 107);
    histogram = histogram_bucket_append(histogram, 524288, 98);
    histogram = histogram_bucket_append(histogram, 262144, 96);
    histogram = histogram_bucket_append(histogram, 131072, 85);
    histogram = histogram_bucket_append(histogram, 65536, 61);
    histogram = histogram_bucket_append(histogram, 32768, 42);
    histogram = histogram_bucket_append(histogram, 16384, 26);
    histogram = histogram_bucket_append(histogram, 8192, 22);
    histogram = histogram_bucket_append(histogram, 4096, 10);
    histogram = histogram_bucket_append(histogram, 1024, 4);
    histogram->sum = 120;

    metric_t m = {
        .value.histogram = histogram,
        .time = 1710200311404036096, /* 1592748157.125 */
    };

    label_set_add(&m.label, true, "hostname", "arrakis.canopus");

    metric_family_metric_append(&fam, m);

    strbuf_t buf = STRBUF_CREATE;

    EXPECT_EQ_INT(0, json_metric_family(&buf, &fam));
    EXPECT_EQ_STR("{\"metric\":\"gauge_histogram\",\"type\":\"gaugehistogram\",\"metrics\":[{\"labels\":{\"hostname\":\"arrakis.canopus\"},\"timestamp\":1592748157125,\"interval\":0,\"buckets\":[[1024,4],[4096,10],[8192,22],[16384,26],[32768,42],[65536,61],[131072,85],[262144,96],[524288,98],[786432,107],[1048576,115],[inf,120]],\"gcount\":120,\"gsum\":120}]}", buf.ptr);

    strbuf_destroy(&buf);
    histogram_destroy(histogram);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

int main(void)
{
    RUN_TEST(json_unknow);
    RUN_TEST(json_gauge);
    RUN_TEST(json_counter_with_label);
    RUN_TEST(json_escaped_label_value);
    RUN_TEST(json_system_uname);
    RUN_TEST(json_stateset);
    RUN_TEST(json_summary);
    RUN_TEST(json_histogram);
    RUN_TEST(json_guage_histogram);

    END_TEST;
}
