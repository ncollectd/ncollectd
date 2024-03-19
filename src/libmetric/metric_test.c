// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2020
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

#include "ncollectd.h"

#include "libutils/strbuf.h"
#include "libmetric/metric.h"
#include "libtest/testing.h"

DEF_TEST(metric_label_set)
{
    struct {
        char const *key;
        char const *value;
        int want_err;
        char const *want_get;
    } cases[] = {
        {
            .key = "foo",
            .value = "bar",
            .want_get = "bar",
        },
        {
            .key = NULL,
            .value = "bar",
            .want_err = EINVAL,
        },
        {
            .key = "foo",
            .value = NULL,
        },
        {
            .key = "",
            .value = "bar",
            .want_err = EINVAL,
        },
        {
            .key = "valid",
            .value = "",
        },
        {
            .key = "1nvalid",
            .value = "bar",
            .want_err = EINVAL,
        },
        {
            .key = "val1d",
            .value = "bar",
            .want_get = "bar",
        },
        {
            .key = "inva!id",
            .value = "bar",
            .want_err = EINVAL,
        },
    };

    for (size_t i = 0; i < (sizeof(cases) / sizeof(cases[0])); i++) {
        printf("## Case %zu: %s=\"%s\"\n", i,
               cases[i].key ? cases[i].key : "(null)",
               cases[i].value ? cases[i].value : "(null)");

        metric_t m = {0};

        EXPECT_EQ_INT(cases[i].want_err, metric_label_set(&m, cases[i].key, cases[i].value));
        EXPECT_EQ_STR(cases[i].want_get, metric_label_get(&m, cases[i].key));

        metric_reset(&m, METRIC_TYPE_UNKNOWN);
        EXPECT_EQ_PTR(NULL, m.label.ptr);
        EXPECT_EQ_INT(0, m.label.num);
    }

    return 0;
}

#if 0
DEF_TEST(metric_identity)
{
    struct {
        char *name;
        label_pair_t *labels;
        size_t labels_num;
        char const *want;
    } cases[] = {
        {
            .name = "metric_without_labels",
            .want = "metric_without_labels",
        },
        {
            .name = "metric_with_labels",
            .labels = (label_pair_t[]){
                {"sorted", "yes"},
                {"alphabetically", "true"},
            },
            .labels_num = 2,
            .want = "metric_with_labels{alphabetically=\"true\",sorted=\"yes\"}",
        },
        {
            .name = "escape_sequences",
            .labels = (label_pair_t[]){
                {"newline", "\n"},
                {"quote", "\""},
                {"tab", "\t"},
                {"cardridge_return", "\r"},
            },
            .labels_num = 4,
            .want = "escape_sequences{cardridge_return=\"\\r\",newline=\"\\n\","
                    "quote=\"\\\"\",tab=\"\\t\"}",
        },
    };

    for (size_t i = 0; i < (sizeof(cases) / sizeof(cases[0])); i++) {
        printf("## Case %zu: %s\n", i, cases[i].name);

        metric_family_t fam = {
            .name = cases[i].name,
            .type = METRIC_TYPE_UNKNOWN,
        };
        metric_t m = {0};

        for (size_t j = 0; j < cases[i].labels_num; j++) {
            CHECK_ZERO(metric_label_set(&m, cases[i].labels[j].name, cases[i].labels[j].value));
        }

        strbuf_t buf = STRBUF_CREATE;
        CHECK_ZERO(metric_identity(&buf, &m));

        EXPECT_EQ_STR(cases[i].want, buf.ptr);

        strbuf_destroy(&buf);
        metric_family_metric_reset(&fam);
        metric_reset(&m, METRIC_TYPE_UNKNOWN);
    }

    return 0;
}
#endif

DEF_TEST(metric_family_append)
{
    struct {
        label_set_t labels;
        double value;
        cdtime_t interval;
        cdtime_t time;
        int want_err;
        label_set_t want_labels;
        double want_value;
        cdtime_t want_time;
        cdtime_t want_interval;
    } cases[] = {
        {
            .value = 42,
            .want_value = 42,
        },
        {
            .labels = (label_set_t){
                .ptr = &(label_pair_t){"type", "test"},
                .num = 1,
            },
            .value = 42,
            .want_labels = (label_set_t){
                .ptr = &(label_pair_t){"type", "test"},
                .num = 1,
            },
            .want_value = 42,
        },
        {
            .value = 42,
            .time = TIME_T_TO_CDTIME_T(1594107920),
            .want_value = 42,
            .want_time = TIME_T_TO_CDTIME_T(1594107920),
        },
        {
            .value = 42,
            .interval = TIME_T_TO_CDTIME_T(10),
            .want_value = 42,
            .want_interval = TIME_T_TO_CDTIME_T(10),
        },
        {
            .labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"type",   "test"},
                    {"common", "label"},
                },
                .num = 2,
            },
            .value = 42,
            .want_labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"common", "label"},
                    {"type",   "test"},
                },
                .num = 2,
            },
            .want_value = 42,
        },
    };

    for (size_t i = 0; i < (sizeof(cases) / sizeof(cases[0])); i++) {
        metric_family_t fam = {
            .name = "test_total",
            .type = METRIC_TYPE_GAUGE,
        };

        EXPECT_EQ_INT(cases[i].want_err,
                      metric_family_append(&fam, (value_t){.gauge.float64 = cases[i].value},
                                           &cases[i].labels, NULL));
        if (cases[i].want_err != 0)
            continue;

        EXPECT_EQ_INT(1, fam.metric.num);
        metric_t const *m = fam.metric.ptr;

        EXPECT_EQ_INT(cases[i].want_labels.num, m->label.num);
        for (size_t j = 0; j < cases[i].want_labels.num; j++) {
            EXPECT_EQ_STR(cases[i].want_labels.ptr[j].value,
                          metric_label_get(m, cases[i].want_labels.ptr[j].name));
        }

        EXPECT_EQ_DOUBLE(cases[i].want_value, m->value.gauge.float64);
//      EXPECT_EQ_UINT64(cases[i].want_time, m->time);
//      EXPECT_EQ_UINT64(cases[i].want_interval, m->interval);

        metric_family_metric_reset(&fam);
    }

    return 0;
}

int main(void)
{
    RUN_TEST(metric_label_set);
#if 0
    RUN_TEST(metric_identity);
#endif
    RUN_TEST(metric_family_append);

    END_TEST;
}
