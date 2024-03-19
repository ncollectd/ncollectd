// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2016-2020  Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

#include "ncollectd.h"
#include "libutils/strbuf.h"
#include "libutils/common.h"
#include "libmetric/metric.h"
#include "libformat/opentsdb_json.h"
#include "libtest/testing.h"

DEF_TEST(opentsdb_json_unknow)
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

    EXPECT_EQ_INT(0, opentsdb_json_metric_family(&buf, &fam, 3600));
    EXPECT_EQ_STR("[{\"metric\":\"metric_unknow\",\"timestamp\":1592748157125,\"ttl\":3600,\"value\":42}]",
                  buf.ptr);

    strbuf_destroy(&buf);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentsdb_json_gauge)
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

    EXPECT_EQ_INT(0, opentsdb_json_metric_family(&buf, &fam, 3600));
    EXPECT_EQ_STR("[{\"metric\":\"metric_gauge\",\"timestamp\":1592748157125,\"ttl\":3600,\"value\":42}]",
                  buf.ptr);

    strbuf_destroy(&buf);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentsdb_json_counter_with_label)
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

    EXPECT_EQ_INT(0, opentsdb_json_metric_family(&buf, &fam, 3600));
    EXPECT_EQ_STR("[{\"metric\":\"metric_counter_with_label_total\",\"tags\":{\"alpha\":\"first\",\"beta\":\"second\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":0}]",
                  buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentsdb_json_escaped_label_value)
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

    EXPECT_EQ_INT(0, opentsdb_json_metric_family(&buf, &fam, 3600));
    EXPECT_EQ_STR("[{\"metric\":\"escaped_label_value_total\",\"tags\":{\"alpha\":\"first/value\",\"beta\":\"second value\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":42}]",
                  buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentsdb_json_system_uname)
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

    EXPECT_EQ_INT(0, opentsdb_json_metric_family(&buf, &fam, 3600));
    EXPECT_EQ_STR("[{\"metric\":\"system_uname_info\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"machine\":\"riscv128\",\"nodename\":\"arrakis.canopus\",\"release\":\"998\",\"sysname\":\"Linux\",\"version\":\"#1 SMP PREEMPT_DYNAMIC 10191\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":1}]",
                  buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.value.info);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentsdb_json_stateset)
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

    EXPECT_EQ_INT(0, opentsdb_json_metric_family(&buf, &fam, 3600));
    EXPECT_EQ_STR("[{\"metric\":\"stateset\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"stateset\":\"a\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":0},{\"metric\":\"stateset\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"stateset\":\"bb\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":1},{\"metric\":\"stateset\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"stateset\":\"ccc\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":0}]",
                  buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentsdb_json_summary)
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

    EXPECT_EQ_INT(0, opentsdb_json_metric_family(&buf, &fam, 3600));
    EXPECT_EQ_STR("[{\"metric\":\"summary\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"quantile\":\"1\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":34.283829292},{\"metric\":\"summary\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"quantile\":\"0.99\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":2.829188272},{\"metric\":\"summary\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"quantile\":\"0.95\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":1.528948804},{\"metric\":\"summary\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"quantile\":\"0.9\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":0.821139321},{\"metric\":\"summary\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"quantile\":\"0.5\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":0.232227334},{\"metric\":\"summary_count\",\"tags\":{\"hostname\":\"arrakis.canopus\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":27892},{\"metric\":\"summary_sum\",\"tags\":{\"hostname\":\"arrakis.canopus\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":8953}]",
                  buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.label);
    summary_destroy(summary);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentsdb_json_histogram)
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

    EXPECT_EQ_INT(0, opentsdb_json_metric_family(&buf, &fam, 3600));
    EXPECT_EQ_STR("[{\"metric\":\"histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"0.01\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":0},{\"metric\":\"histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"0.025\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":8},{\"metric\":\"histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"0.05\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":1672},{\"metric\":\"histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"0.1\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":8954},{\"metric\":\"histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"0.25\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":14251},{\"metric\":\"histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"0.5\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":24101},{\"metric\":\"histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"1\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":26351},{\"metric\":\"histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"2.5\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":27534},{\"metric\":\"histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"5\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":27814},{\"metric\":\"histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"10\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":27881},{\"metric\":\"histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"25\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":27890},{\"metric\":\"histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"inf\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":27892},{\"metric\":\"histogram_count\",\"tags\":{\"hostname\":\"arrakis.canopus\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":27892},{\"metric\":\"histogram_sum\",\"tags\":{\"hostname\":\"arrakis.canopus\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":8953.332}]",
                  buf.ptr);

    strbuf_destroy(&buf);
    histogram_destroy(histogram);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentsdb_json_guage_histogram)
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

    EXPECT_EQ_INT(0, opentsdb_json_metric_family(&buf, &fam, 3600));
    EXPECT_EQ_STR("[{\"metric\":\"gauge_histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"1024\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":4},{\"metric\":\"gauge_histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"4096\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":10},{\"metric\":\"gauge_histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"8192\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":22},{\"metric\":\"gauge_histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"16384\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":26},{\"metric\":\"gauge_histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"32768\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":42},{\"metric\":\"gauge_histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"65536\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":61},{\"metric\":\"gauge_histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"131072\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":85},{\"metric\":\"gauge_histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"262144\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":96},{\"metric\":\"gauge_histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"524288\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":98},{\"metric\":\"gauge_histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"786432\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":107},{\"metric\":\"gauge_histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"1048576\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":115},{\"metric\":\"gauge_histogram\",\"tags\":{\"hostname\":\"arrakis.canopus\",\"le\":\"inf\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":120},{\"metric\":\"gauge_histogram_gcount\",\"tags\":{\"hostname\":\"arrakis.canopus\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":120},{\"metric\":\"gauge_histogram_gsum\",\"tags\":{\"hostname\":\"arrakis.canopus\"},\"timestamp\":1592748157125,\"ttl\":3600,\"value\":120}]",
                  buf.ptr);

    strbuf_destroy(&buf);
    histogram_destroy(histogram);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

int main(void)
{
    RUN_TEST(opentsdb_json_unknow);
    RUN_TEST(opentsdb_json_gauge);
    RUN_TEST(opentsdb_json_counter_with_label);
    RUN_TEST(opentsdb_json_escaped_label_value);
    RUN_TEST(opentsdb_json_system_uname);
    RUN_TEST(opentsdb_json_stateset);
    RUN_TEST(opentsdb_json_summary);
    RUN_TEST(opentsdb_json_histogram);
    RUN_TEST(opentsdb_json_guage_histogram);

    END_TEST;
}
