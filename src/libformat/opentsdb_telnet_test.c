// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2016-2020  Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

#include "ncollectd.h"
#include "libutils/strbuf.h"
#include "libutils/common.h"
#include "libmetric/metric.h"
#include "libformat/opentsdb_telnet.h"
#include "libtest/testing.h"

DEF_TEST(opentsdb_telnet_unknow)
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

    EXPECT_EQ_INT(0, opentsdb_telnet_metric_family(&buf, &fam, 3600, FMT_OPENTSDB_MSEC));
    EXPECT_EQ_STR("putm metric_unknow 1592748157125 42 kairos_opt.ttl=3600\n", buf.ptr);

    strbuf_destroy(&buf);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentsdb_telnet_gauge)
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

    EXPECT_EQ_INT(0, opentsdb_telnet_metric_family(&buf, &fam, 3600, FMT_OPENTSDB_MSEC));
    EXPECT_EQ_STR("putm metric_gauge 1592748157125 42 kairos_opt.ttl=3600\n", buf.ptr);

    strbuf_destroy(&buf);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentsdb_telnet_counter_with_label)
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

    EXPECT_EQ_INT(0, opentsdb_telnet_metric_family(&buf, &fam, 3600, FMT_OPENTSDB_MSEC));
    EXPECT_EQ_STR("putm metric_counter_with_label_total 1592748157125 0 alpha=first beta=second kairos_opt.ttl=3600\n",
                  buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentsdb_telnet_escaped_label_value)
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

    EXPECT_EQ_INT(0, opentsdb_telnet_metric_family(&buf, &fam, 3600, FMT_OPENTSDB_MSEC));
    EXPECT_EQ_STR("putm escaped_label_value_total 1592748157125 18446744073709551615 alpha=first/value beta=second value kairos_opt.ttl=3600\n",
                  buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentsdb_telnet_system_uname)
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

    EXPECT_EQ_INT(0, opentsdb_telnet_metric_family(&buf, &fam, 3600, FMT_OPENTSDB_MSEC));
    EXPECT_EQ_STR("putm system_uname_info 1592748157125 1 hostname=arrakis.canopus machine=riscv128 nodename=arrakis.canopus release=998 sysname=Linux version=#1 SMP PREEMPT_DYNAMIC 10191 kairos_opt.ttl=3600\n",
                  buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.value.info);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentsdb_telnet_stateset)
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

    EXPECT_EQ_INT(0, opentsdb_telnet_metric_family(&buf, &fam, 3600, FMT_OPENTSDB_MSEC));
    EXPECT_EQ_STR("putm stateset 1592748157125 0 hostname=arrakis.canopus stateset=a kairos_opt.ttl=3600\n"
                  "putm stateset 1592748157125 1 hostname=arrakis.canopus stateset=bb kairos_opt.ttl=3600\n"
                  "putm stateset 1592748157125 0 hostname=arrakis.canopus stateset=ccc kairos_opt.ttl=3600\n",
                  buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentsdb_telnet_summary)
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

    EXPECT_EQ_INT(0, opentsdb_telnet_metric_family(&buf, &fam, 3600, FMT_OPENTSDB_MSEC));
    EXPECT_EQ_STR("putm summary 1592748157125 34.283829292 hostname=arrakis.canopus quantile=1 kairos_opt.ttl=3600\n"
                  "putm summary 1592748157125 2.829188272 hostname=arrakis.canopus quantile=0.99 kairos_opt.ttl=3600\n"
                  "putm summary 1592748157125 1.528948804 hostname=arrakis.canopus quantile=0.95 kairos_opt.ttl=3600\n"
                  "putm summary 1592748157125 0.821139321 hostname=arrakis.canopus quantile=0.9 kairos_opt.ttl=3600\n"
                  "putm summary 1592748157125 0.232227334 hostname=arrakis.canopus quantile=0.5 kairos_opt.ttl=3600\n"
                  "putm summary_count 1592748157125 27892 hostname=arrakis.canopus kairos_opt.ttl=3600\n"
                  "putm summary_sum 1592748157125 8953 hostname=arrakis.canopus kairos_opt.ttl=3600\n",
                  buf.ptr);

    strbuf_destroy(&buf);
    label_set_reset(&m.label);
    summary_destroy(summary);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentsdb_telnet_histogram)
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

    EXPECT_EQ_INT(0, opentsdb_telnet_metric_family(&buf, &fam, 3600, FMT_OPENTSDB_MSEC));
    EXPECT_EQ_STR("putm histogram_bucket 1592748157125 0 hostname=arrakis.canopus le=0.01 kairos_opt.ttl=3600\n"
                  "putm histogram_bucket 1592748157125 8 hostname=arrakis.canopus le=0.025 kairos_opt.ttl=3600\n"
                  "putm histogram_bucket 1592748157125 1672 hostname=arrakis.canopus le=0.05 kairos_opt.ttl=3600\n"
                  "putm histogram_bucket 1592748157125 8954 hostname=arrakis.canopus le=0.1 kairos_opt.ttl=3600\n"
                  "putm histogram_bucket 1592748157125 14251 hostname=arrakis.canopus le=0.25 kairos_opt.ttl=3600\n"
                  "putm histogram_bucket 1592748157125 24101 hostname=arrakis.canopus le=0.5 kairos_opt.ttl=3600\n"
                  "putm histogram_bucket 1592748157125 26351 hostname=arrakis.canopus le=1 kairos_opt.ttl=3600\n"
                  "putm histogram_bucket 1592748157125 27534 hostname=arrakis.canopus le=2.5 kairos_opt.ttl=3600\n"
                  "putm histogram_bucket 1592748157125 27814 hostname=arrakis.canopus le=5 kairos_opt.ttl=3600\n"
                  "putm histogram_bucket 1592748157125 27881 hostname=arrakis.canopus le=10 kairos_opt.ttl=3600\n"
                  "putm histogram_bucket 1592748157125 27890 hostname=arrakis.canopus le=25 kairos_opt.ttl=3600\n"
                  "putm histogram_bucket 1592748157125 27892 hostname=arrakis.canopus le=inf kairos_opt.ttl=3600\n"
                  "putm histogram_count 1592748157125 27892 hostname=arrakis.canopus kairos_opt.ttl=3600\n"
                  "putm histogram_sum 1592748157125 8953.332 hostname=arrakis.canopus kairos_opt.ttl=3600\n",
                  buf.ptr);

    strbuf_destroy(&buf);
    histogram_destroy(histogram);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

DEF_TEST(opentsdb_telnet_guage_histogram)
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

    EXPECT_EQ_INT(0, opentsdb_telnet_metric_family(&buf, &fam, 3600, FMT_OPENTSDB_MSEC));
    EXPECT_EQ_STR("putm gauge_histogram_bucket 1592748157125 4 hostname=arrakis.canopus le=1024 kairos_opt.ttl=3600\n"
                  "putm gauge_histogram_bucket 1592748157125 10 hostname=arrakis.canopus le=4096 kairos_opt.ttl=3600\n"
                  "putm gauge_histogram_bucket 1592748157125 22 hostname=arrakis.canopus le=8192 kairos_opt.ttl=3600\n"
                  "putm gauge_histogram_bucket 1592748157125 26 hostname=arrakis.canopus le=16384 kairos_opt.ttl=3600\n"
                  "putm gauge_histogram_bucket 1592748157125 42 hostname=arrakis.canopus le=32768 kairos_opt.ttl=3600\n"
                  "putm gauge_histogram_bucket 1592748157125 61 hostname=arrakis.canopus le=65536 kairos_opt.ttl=3600\n"
                  "putm gauge_histogram_bucket 1592748157125 85 hostname=arrakis.canopus le=131072 kairos_opt.ttl=3600\n"
                  "putm gauge_histogram_bucket 1592748157125 96 hostname=arrakis.canopus le=262144 kairos_opt.ttl=3600\n"
                  "putm gauge_histogram_bucket 1592748157125 98 hostname=arrakis.canopus le=524288 kairos_opt.ttl=3600\n"
                  "putm gauge_histogram_bucket 1592748157125 107 hostname=arrakis.canopus le=786432 kairos_opt.ttl=3600\n"
                  "putm gauge_histogram_bucket 1592748157125 115 hostname=arrakis.canopus le=1048576 kairos_opt.ttl=3600\n"
                  "putm gauge_histogram_bucket 1592748157125 120 hostname=arrakis.canopus le=inf kairos_opt.ttl=3600\n"
                  "putm gauge_histogram_gcount 1592748157125 120 hostname=arrakis.canopus kairos_opt.ttl=3600\n"
                  "putm gauge_histogram_gsum 1592748157125 120 hostname=arrakis.canopus kairos_opt.ttl=3600\n",
                  buf.ptr);

    strbuf_destroy(&buf);
    histogram_destroy(histogram);
    label_set_reset(&m.label);
    metric_family_metric_reset(&fam);

    return 0;
}

int main(void)
{
    RUN_TEST(opentsdb_telnet_unknow);
    RUN_TEST(opentsdb_telnet_gauge);
    RUN_TEST(opentsdb_telnet_counter_with_label);
    RUN_TEST(opentsdb_telnet_escaped_label_value);
    RUN_TEST(opentsdb_telnet_system_uname);
    RUN_TEST(opentsdb_telnet_stateset);
    RUN_TEST(opentsdb_telnet_summary);
    RUN_TEST(opentsdb_telnet_histogram);
    RUN_TEST(opentsdb_telnet_guage_histogram);

    END_TEST;
}
