// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2016-2020  Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

#include "ncollectd.h"
#include "libutils/strbuf.h"
#include "libutils/common.h"
#include "libmetric/metric.h"
#include "libformat/opentelemetry_json.h"
#include "libtest/testing.h"

DEF_TEST(opentelemetry_json_unknow)
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

    EXPECT_EQ_INT(0, opentelemetry_json_metric_family(&buf, &fam));
    EXPECT_EQ_STR("{\"resourceMetrics\":[{\"scopeMetrics\":{\"scope\":{\"name\":\""PACKAGE_NAME"\",\"version\":\""PACKAGE_VERSION"\"},\"metrics\":[{\"name\":\"metric_unknow\",\"gauge\":{\"dataPoints\":[{\"asDouble\":42,\"timeUnixNano\":1592748157125000000}]}}]}}]}",
                  buf.ptr);

    strbuf_destroy(&buf);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentelemetry_json_gauge)
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

    EXPECT_EQ_INT(0, opentelemetry_json_metric_family(&buf, &fam));
    EXPECT_EQ_STR("{\"resourceMetrics\":[{\"scopeMetrics\":{\"scope\":{\"name\":\""PACKAGE_NAME"\",\"version\":\""PACKAGE_VERSION"\"},\"metrics\":[{\"name\":\"metric_gauge\",\"gauge\":{\"dataPoints\":[{\"asDouble\":42,\"timeUnixNano\":1592748157125000000}]}}]}}]}",
                  buf.ptr);

    strbuf_destroy(&buf);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentelemetry_json_counter_with_label)
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

    EXPECT_EQ_INT(0, opentelemetry_json_metric_family(&buf, &fam));
    EXPECT_EQ_STR("{\"resourceMetrics\":[{\"scopeMetrics\":{\"scope\":{\"name\":\""PACKAGE_NAME"\",\"version\":\""PACKAGE_VERSION"\"},\"metrics\":[{\"name\":\"metric_counter_with_label\",\"sum\":{\"aggregationTemporality\":2,\"dataPoints\":[{\"isMonotonic\":true,\"asInt\":0,\"timeUnixNano\":1592748157125000000,\"attributes\":[{\"key\":\"alpha\",\"value\":{\"stringValue\":\"first\"}},{\"key\":\"beta\",\"value\":{\"stringValue\":\"second\"}}]}]}}]}}]}",
                  buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentelemetry_json_escaped_label_value)
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

    EXPECT_EQ_INT(0, opentelemetry_json_metric_family(&buf, &fam));
    EXPECT_EQ_STR("{\"resourceMetrics\":[{\"scopeMetrics\":{\"scope\":{\"name\":\""PACKAGE_NAME"\",\"version\":\""PACKAGE_VERSION"\"},\"metrics\":[{\"name\":\"escaped_label_value\",\"sum\":{\"aggregationTemporality\":2,\"dataPoints\":[{\"isMonotonic\":true,\"asInt\":42,\"timeUnixNano\":1592748157125000000,\"attributes\":[{\"key\":\"alpha\",\"value\":{\"stringValue\":\"first/value\"}},{\"key\":\"beta\",\"value\":{\"stringValue\":\"second value\"}}]}]}}]}}]}",
                  buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentelemetry_json_system_uname)
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

    EXPECT_EQ_INT(0, opentelemetry_json_metric_family(&buf, &fam));
    EXPECT_EQ_STR("{\"resourceMetrics\":[{\"scopeMetrics\":{\"scope\":{\"name\":\""PACKAGE_NAME"\",\"version\":\""PACKAGE_VERSION"\"},\"metrics\":[{\"name\":\"system_uname\",\"gauge\":{\"dataPoints\":[{\"asInt\":1,\"timeUnixNano\":1592748157125000000,\"attributes\":[{\"key\":\"hostname\",\"value\":{\"stringValue\":\"arrakis.canopus\"}},{\"key\":\"machine\",\"value\":{\"stringValue\":\"riscv128\"}},{\"key\":\"nodename\",\"value\":{\"stringValue\":\"arrakis.canopus\"}},{\"key\":\"release\",\"value\":{\"stringValue\":\"998\"}},{\"key\":\"sysname\",\"value\":{\"stringValue\":\"Linux\"}},{\"key\":\"version\",\"value\":{\"stringValue\":\"#1 SMP PREEMPT_DYNAMIC 10191\"}}]}]}}]}}]}",
                  buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.value.info);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentelemetry_json_stateset)
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

    EXPECT_EQ_INT(0, opentelemetry_json_metric_family(&buf, &fam));
    EXPECT_EQ_STR("{\"resourceMetrics\":[{\"scopeMetrics\":{\"scope\":{\"name\":\""PACKAGE_NAME"\",\"version\":\""PACKAGE_VERSION"\"},\"metrics\":[{\"name\":\"stateset\",\"sum\":{\"dataPoints\":[{\"isMonotonic\":false,\"asInt\":0,\"timeUnixNano\":1592748157125000000,\"attributes\":[{\"key\":\"hostname\",\"value\":{\"stringValue\":\"arrakis.canopus\"}},{\"key\":\"stateset\",\"value\":{\"stringValue\":\"a\"}}]},{\"isMonotonic\":false,\"asInt\":1,\"timeUnixNano\":1592748157125000000,\"attributes\":[{\"key\":\"hostname\",\"value\":{\"stringValue\":\"arrakis.canopus\"}},{\"key\":\"stateset\",\"value\":{\"stringValue\":\"bb\"}}]},{\"isMonotonic\":false,\"asInt\":0,\"timeUnixNano\":1592748157125000000,\"attributes\":[{\"key\":\"hostname\",\"value\":{\"stringValue\":\"arrakis.canopus\"}},{\"key\":\"stateset\",\"value\":{\"stringValue\":\"ccc\"}}]}]}}]}}]}",
                  buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentelemetry_json_summary)
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

    EXPECT_EQ_INT(0, opentelemetry_json_metric_family(&buf, &fam));
    EXPECT_EQ_STR("{\"resourceMetrics\":[{\"scopeMetrics\":{\"scope\":{\"name\":\""PACKAGE_NAME"\",\"version\":\""PACKAGE_VERSION"\"},\"metrics\":[{\"name\":\"summary\",\"summary\":{\"dataPoints\":[{\"ValueAtQuantile\":{\"quantile\":1,\"value\":34.283829292,\"quantile\":0.99,\"value\":2.829188272,\"quantile\":0.95,\"value\":1.528948804,\"quantile\":0.9,\"value\":0.821139321,\"quantile\":0.5,\"value\":0.232227334},\"count\":27892,\"sum\":8953,\"timeUnixNano\":1592748157125000000,\"attributes\":[{\"key\":\"hostname\",\"value\":{\"stringValue\":\"arrakis.canopus\"}}]}]}}]}}]}",
                  buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.label);
    summary_destroy(summary);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentelemetry_json_histogram)
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

    EXPECT_EQ_INT(0, opentelemetry_json_metric_family(&buf, &fam));
    EXPECT_EQ_STR("{\"resourceMetrics\":[{\"scopeMetrics\":{\"scope\":{\"name\":\""PACKAGE_NAME"\",\"version\":\""PACKAGE_VERSION"\"},\"metrics\":[{\"name\":\"histogram\",\"histogram\":{\"aggregationTemporality\":2,\"dataPoints\":[{\"bucketCounts\":[0,8,1672,8954,14251,24101,26351,27534,27814,27881,27890,27892],\"explicitBounds\":[0.01,0.025,0.05,0.1,0.25,0.5,1,2.5,5,10,25],\"count\":27892,\"sum\":8953.332,\"timeUnixNano\":1592748157125000000,\"attributes\":[{\"key\":\"hostname\",\"value\":{\"stringValue\":\"arrakis.canopus\"}}]}]}}]}}]}",
                  buf.ptr);

    strbuf_destroy(&buf);
    histogram_destroy(histogram);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentelemetry_json_guage_histogram)
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

    EXPECT_EQ_INT(0, opentelemetry_json_metric_family(&buf, &fam));
    EXPECT_EQ_PTR(NULL, buf.ptr);

    strbuf_destroy(&buf);
    histogram_destroy(histogram);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

int main(void)
{
    RUN_TEST(opentelemetry_json_unknow);
    RUN_TEST(opentelemetry_json_gauge);
    RUN_TEST(opentelemetry_json_counter_with_label);
    RUN_TEST(opentelemetry_json_escaped_label_value);
    RUN_TEST(opentelemetry_json_system_uname);
    RUN_TEST(opentelemetry_json_stateset);
    RUN_TEST(opentelemetry_json_summary);
    RUN_TEST(opentelemetry_json_histogram);
    RUN_TEST(opentelemetry_json_guage_histogram);

    END_TEST;
}
