// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "libmetric/metric.h"
#include "libmetric/parser.h"
#include "libtest/testing.h"

static metric_family_t *g_fams;
static size_t g_fams_num = 0;

static int dispatch_metric_family(metric_family_t *fam,
                                  __attribute__((unused)) plugin_filter_t *filter,
                                  __attribute__((unused)) cdtime_t time)
{
    metric_family_t *tmp = realloc(g_fams, sizeof(*tmp)*(g_fams_num +1));
    if (tmp == NULL)
        return -1;

    char *dup_name = strdup(fam->name);
    if (dup_name == NULL)
        return -1;

    g_fams = tmp;
    g_fams[g_fams_num].name = dup_name;
    g_fams[g_fams_num].help = NULL;
    g_fams[g_fams_num].unit = NULL;
    g_fams[g_fams_num].type = fam->type;
    g_fams[g_fams_num].metric = fam->metric;
    fam->metric = (metric_list_t){0};
    g_fams_num++;

    return 0;
}

DEF_TEST(metric_parser)
{
    struct {
        char *input;
        size_t num;
        metric_family_t *fams;
    } cases[] = {
        {
            .input = "a 100",
            .num = 1,
            .fams = (metric_family_t []) {
                {
                    .name = "a",
                    .type = METRIC_TYPE_UNKNOWN,
                    .metric = (metric_list_t) {
                        .num = 1,
                        .ptr = (metric_t []) {
                            {
                                .value = {
                                    .unknown.type = UNKNOWN_FLOAT64,
                                    .unknown.float64 = 100
                                },
                            },
                        },
                    },
                },
            },
        },
        {
            .input = "# TYPE a counter\n"
                     "# HELP a help\n"
                     "a 1",
            .num = 1,
            .fams = (metric_family_t []) {
                {
                    .name = "a",
                    .type = METRIC_TYPE_COUNTER,
                    .metric = (metric_list_t) {
                        .num = 1,
                        .ptr = (metric_t []) {
                            {
                                .value = {
                                    .counter.type = COUNTER_UINT64,
                                    .counter.uint64 = 1
                                },
                            },
                        },
                    },
                },
            },
        },
        {
            .input = "# TYPE a gauge\n"
                     "# HELP a help\n"
                     "a 3.14",
            .num = 1,
            .fams = (metric_family_t []) {
                {
                    .name = "a",
                    .type = METRIC_TYPE_GAUGE,
                    .metric = (metric_list_t) {
                        .num = 1,
                        .ptr = (metric_t []) {
                            {
                                .value = {
                                    .gauge.type = GAUGE_FLOAT64,
                                    .gauge.float64 = 3.14
                                },
                            },
                        },
                    },
                },
            },
        },
        {
            .input = "# TYPE bc counter\n"
                     "# HELP bc help\n"
                     "bc{type=\"a\"} 1\n"
                     "bc{type=\"b\"} 2\n"
                     "bc{type=\"c\"} 3",
            .num = 1,
            .fams = (metric_family_t []) {
                {
                    .name = "bc",
                    .type = METRIC_TYPE_COUNTER,
                    .metric = (metric_list_t) {
                        .num = 3,
                        .ptr = (metric_t []) {
                            {
                                .label = {
                                    .num = 1,
                                    .ptr = (label_pair_t []) {
                                        {
                                            .name = "type",
                                            .value = "a"
                                        },
                                    },
                                },
                                .value = {
                                    .counter.type = COUNTER_UINT64,
                                    .counter.uint64 = 1
                                },
                            },
                            {
                                .label = {
                                    .num = 1,
                                    .ptr = (label_pair_t []) {
                                        {
                                            .name = "type",
                                            .value = "b"
                                        },
                                    },
                                },
                                .value = {
                                    .counter.type = COUNTER_UINT64,
                                    .counter.uint64 = 2
                                },
                            },
                            {
                                .label = {
                                    .num = 1,
                                    .ptr = (label_pair_t []) {
                                        {
                                            .name = "type",
                                            .value = "c"
                                        },
                                    },
                                },
                                .value = {
                                    .counter.type = COUNTER_UINT64,
                                    .counter.uint64 = 3
                                },
                            },
                        },
                    },
                },
            },
        },
        {
            .input = "# TYPE a gauge\n"
                     "# HELP a help\n"
                     "a 3.14\n"
                     "# TYPE b counter\n"
                     "# HELP b help\n"
                     "b 3.14",
            .num = 2,
            .fams = (metric_family_t []) {
                {
                    .name = "a",
                    .type = METRIC_TYPE_GAUGE,
                    .metric = (metric_list_t) {
                        .num = 1,
                        .ptr = (metric_t []) {
                            {
                                .value = {
                                    .gauge.type = GAUGE_FLOAT64,
                                    .gauge.float64 = 3.14
                                },
                            },
                        },
                    },
                },
                {
                    .name = "b",
                    .type = METRIC_TYPE_COUNTER,
                    .metric = (metric_list_t) {
                        .num = 1,
                        .ptr = (metric_t []) {
                            {
                                .value = {
                                    .counter.type = COUNTER_FLOAT64,
                                    .counter.float64 = 3.14
                                },
                            },
                        },
                    },
                },
            },
        },
        {
            .input = "# TYPE a counter\n"
                     "# HELP a help\n"
                     "a{ too = \"low\" } 1\n"
                     "a\t{\t\ttoo\t=\t\t\"high\"\t}\t2\t\n"
                     "a\t {\t too\t =\t \"bad\" \t} \t 3 ",
            .num = 1,
            .fams = (metric_family_t []) {
                {
                    .name = "a",
                    .type = METRIC_TYPE_COUNTER,
                    .metric = (metric_list_t) {
                        .num = 3,
                        .ptr = (metric_t []) {
                            {
                                .label = {
                                    .num = 1,
                                    .ptr = (label_pair_t []) {
                                        {
                                            .name = "too",
                                            .value = "low"
                                        },
                                    },
                                },
                                .value = {
                                    .counter.type = COUNTER_UINT64,
                                    .counter.uint64 = 1
                                },
                            },
                            {
                                .label = {
                                    .num = 1,
                                    .ptr = (label_pair_t []) {
                                        {
                                            .name = "too",
                                            .value = "high"
                                        },
                                    },
                                },
                                .value = {
                                    .counter.type = COUNTER_UINT64,
                                    .counter.uint64 = 2
                                },
                            },
                            {
                                .label = {
                                    .num = 1,
                                    .ptr = (label_pair_t []) {
                                        {
                                            .name = "too",
                                            .value = "bad"
                                        },
                                    },
                                },
                                .value = {
                                    .counter.type = COUNTER_UINT64,
                                    .counter.uint64 = 3
                                },
                            },
                        },
                    },
                },
            },
        },
    };

    for (size_t i = 0; i < (sizeof(cases) / sizeof(cases[0])); i++) {
        metric_parser_t *mp = metric_parser_alloc(NULL, NULL);

        int status =  metric_parse_buffer(mp, cases[i].input, strlen(cases[i].input));
        status |= metric_parse_buffer(mp, NULL, 0);

        EXPECT_EQ_INT(0, status);

        metric_parser_dispatch(mp, dispatch_metric_family, NULL, 0);

        EXPECT_EQ_FAM_LIST(cases[i].fams, cases[i].num, g_fams, g_fams_num);

        metric_parser_free(mp);

        for (size_t j = 0; j < g_fams_num; j++) {
            free(g_fams[j].name);
            metric_family_metric_reset(&g_fams[j]);
        }

        free(g_fams);
        g_fams = NULL;
        g_fams_num = 0;
    }

    return 0;
}

int main(void)
{
    RUN_TEST(metric_parser);

    END_TEST;
}
