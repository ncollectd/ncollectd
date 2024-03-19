// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2016-2020  Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

#include "ncollectd.h"
#include "libutils/strbuf.h"
#include "libutils/common.h"
#include "libmetric/metric.h"
#include "libformat/influxdb.h"
#include "libtest/testing.h"

DEF_TEST(influxdb_unknow)
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

    EXPECT_EQ_INT(0, influxdb_metric_family(&buf, &fam, FMT_INFLUXDB_MSEC));
    EXPECT_EQ_STR("metric_unknow value=42 1592748157125000000\n" , buf.ptr);

    strbuf_destroy(&buf);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(influxdb_gauge)
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

    EXPECT_EQ_INT(0, influxdb_metric_family(&buf, &fam, FMT_INFLUXDB_MSEC));
    EXPECT_EQ_STR("metric_gauge value=42 1592748157125000000\n", buf.ptr);

    strbuf_destroy(&buf);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(influxdb_counter_with_label)
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

    EXPECT_EQ_INT(0, influxdb_metric_family(&buf, &fam, FMT_INFLUXDB_MSEC));
    EXPECT_EQ_STR("metric_counter_with_label,alpha=first,beta=second value=0u 1592748157125000000\n",
                  buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(influxdb_escaped_label_value)
{
    metric_family_t fam = {
        .name = "escaped_label_value",
        .type = METRIC_TYPE_COUNTER,
    };

    metric_t m = {
        .value = (value_t){.counter.uint64 = 18446744073709551615LLU},
        .time = 1710200311404036096, /* 1592748157.125 */
    };

    label_set_add(&m.label, true, "alpha", "first/value");
    label_set_add(&m.label, true, "beta", "second value");

    metric_family_metric_append(&fam, m);

    strbuf_t buf = STRBUF_CREATE;

    EXPECT_EQ_INT(0, influxdb_metric_family(&buf, &fam, FMT_INFLUXDB_MSEC));
    EXPECT_EQ_STR("escaped_label_value,alpha=first/value,beta=second value value=18446744073709551615u 1592748157125000000\n",
                  buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(influxdb_system_uname)
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

    EXPECT_EQ_INT(0, influxdb_metric_family(&buf, &fam, FMT_INFLUXDB_MSEC));
    EXPECT_EQ_STR("system_uname_info,hostname=arrakis.canopus,machine=riscv128,nodename=arrakis.canopus,release=998,sysname=Linux,version=#1 SMP PREEMPT_DYNAMIC 10191 value=1i 1592748157125000000\n",
                  buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.value.info);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(influxdb_stateset)
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

    EXPECT_EQ_INT(0, influxdb_metric_family(&buf, &fam, FMT_INFLUXDB_MSEC));
    EXPECT_EQ_STR("stateset,hostname=arrakis.canopus,stateset=a value=0i 1592748157125000000\n"
                  "stateset,hostname=arrakis.canopus,stateset=bb value=1i 1592748157125000000\n"
                  "stateset,hostname=arrakis.canopus,stateset=ccc value=0i 1592748157125000000\n",
                  buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(influxdb_summary)
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

    EXPECT_EQ_INT(0, influxdb_metric_family(&buf, &fam, FMT_INFLUXDB_MSEC));
    EXPECT_EQ_STR("summary,hostname=arrakis.canopus,quantile=1 value=34.283829292 1592748157125000000\n"
                  "summary,hostname=arrakis.canopus,quantile=0.99 value=2.829188272 1592748157125000000\n"
                  "summary,hostname=arrakis.canopus,quantile=0.95 value=1.528948804 1592748157125000000\n"
                  "summary,hostname=arrakis.canopus,quantile=0.9 value=0.821139321 1592748157125000000\n"
                  "summary,hostname=arrakis.canopus,quantile=0.5 value=0.232227334 1592748157125000000\n"
                  "summary_count,hostname=arrakis.canopus value=27892u 1592748157125000000\n"
                  "summary_sum,hostname=arrakis.canopus value=8953.332 1592748157125000000\n",
                  buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.label);
    summary_destroy(summary);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(influxdb_histogram)
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

    EXPECT_EQ_INT(0, influxdb_metric_family(&buf, &fam, FMT_INFLUXDB_MSEC));
    EXPECT_EQ_STR("histogram_bucket,hostname=arrakis.canopus,le=0.01 value=0u 1592748157125000000\n"
                  "histogram_bucket,hostname=arrakis.canopus,le=0.025 value=8u 1592748157125000000\n"
                  "histogram_bucket,hostname=arrakis.canopus,le=0.05 value=1672u 1592748157125000000\n"
                  "histogram_bucket,hostname=arrakis.canopus,le=0.1 value=8954u 1592748157125000000\n"
                  "histogram_bucket,hostname=arrakis.canopus,le=0.25 value=14251u 1592748157125000000\n"
                  "histogram_bucket,hostname=arrakis.canopus,le=0.5 value=24101u 1592748157125000000\n"
                  "histogram_bucket,hostname=arrakis.canopus,le=1 value=26351u 1592748157125000000\n"
                  "histogram_bucket,hostname=arrakis.canopus,le=2.5 value=27534u 1592748157125000000\n"
                  "histogram_bucket,hostname=arrakis.canopus,le=5 value=27814u 1592748157125000000\n"
                  "histogram_bucket,hostname=arrakis.canopus,le=10 value=27881u 1592748157125000000\n"
                  "histogram_bucket,hostname=arrakis.canopus,le=25 value=27890u 1592748157125000000\n"
                  "histogram_bucket,hostname=arrakis.canopus,le=inf value=27892u 1592748157125000000\n"
                  "histogram_count,hostname=arrakis.canopus value=27892u 1592748157125000000\n"
                  "histogram_sum,hostname=arrakis.canopus value=8953.332 1592748157125000000\n",
                  buf.ptr);

    strbuf_destroy(&buf);
    histogram_destroy(histogram);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(influxdb_guage_histogram)
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

    EXPECT_EQ_INT(0, influxdb_metric_family(&buf, &fam, FMT_INFLUXDB_MSEC));
    EXPECT_EQ_STR("gauge_histogram_bucket,hostname=arrakis.canopus,le=1024 value=4u 1592748157125000000\n"
                  "gauge_histogram_bucket,hostname=arrakis.canopus,le=4096 value=10u 1592748157125000000\n"
                  "gauge_histogram_bucket,hostname=arrakis.canopus,le=8192 value=22u 1592748157125000000\n"
                  "gauge_histogram_bucket,hostname=arrakis.canopus,le=16384 value=26u 1592748157125000000\n"
                  "gauge_histogram_bucket,hostname=arrakis.canopus,le=32768 value=42u 1592748157125000000\n"
                  "gauge_histogram_bucket,hostname=arrakis.canopus,le=65536 value=61u 1592748157125000000\n"
                  "gauge_histogram_bucket,hostname=arrakis.canopus,le=131072 value=85u 1592748157125000000\n"
                  "gauge_histogram_bucket,hostname=arrakis.canopus,le=262144 value=96u 1592748157125000000\n"
                  "gauge_histogram_bucket,hostname=arrakis.canopus,le=524288 value=98u 1592748157125000000\n"
                  "gauge_histogram_bucket,hostname=arrakis.canopus,le=786432 value=107u 1592748157125000000\n"
                  "gauge_histogram_bucket,hostname=arrakis.canopus,le=1048576 value=115u 1592748157125000000\n"
                  "gauge_histogram_bucket,hostname=arrakis.canopus,le=inf value=120u 1592748157125000000\n"
                  "gauge_histogram_gcount,hostname=arrakis.canopus value=120u 1592748157125000000\n"
                  "gauge_histogram_gsum,hostname=arrakis.canopus value=120 1592748157125000000\n",
                  buf.ptr);

    strbuf_destroy(&buf);
    histogram_destroy(histogram);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

int main(void)
{
    RUN_TEST(influxdb_unknow);
    RUN_TEST(influxdb_gauge);
    RUN_TEST(influxdb_counter_with_label);
    RUN_TEST(influxdb_escaped_label_value);
    RUN_TEST(influxdb_system_uname);
    RUN_TEST(influxdb_stateset);
    RUN_TEST(influxdb_summary);
    RUN_TEST(influxdb_histogram);
    RUN_TEST(influxdb_guage_histogram);

    END_TEST;
}
