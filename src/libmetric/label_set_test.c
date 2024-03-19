// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "libmetric/label_set.h"
#include "libtest/testing.h"

DEF_TEST(label_set_add)
{
    struct {
        label_set_t labels;
        label_set_t want_labels;
    } cases[] = {
        {
            .labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"node",   "4"},
                },
                .num = 1,
            },
            .want_labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"node",   "4"}
                },
                .num = 1,
            },
        },
        {
            .labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"book",   "0"},
                    {"core",   "1"}
                },
                .num = 2,
            },
            .want_labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"book",   "0"},
                    {"core",   "1"}
                },
                .num = 2,
            },
        },
        {
            .labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"core",   "1"},
                    {"book",   "0"}
                },
                .num = 2,
            },
            .want_labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"book",   "0"},
                    {"core",   "1"}
                },
                .num = 2,
            },
        },
        {
            .labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"book",   "0"},
                    {"core",   "1"},
                    {"cpu",    "2"}
                },
                .num = 3,
            },
            .want_labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"book",   "0"},
                    {"core",   "1"},
                    {"cpu",    "2"}
                },
                .num = 3,
            },
        },
        {
            .labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"cpu",    "2"},
                    {"book",   "0"},
                    {"core",   "1"}
                },
                .num = 3,
            },
            .want_labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"book",   "0"},
                    {"core",   "1"},
                    {"cpu",    "2"}
                },
                .num = 3,
            },
        },
        {
            .labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"cpu",    "2"},
                    {"core",   "1"},
                    {"book",   "0"}
                },
                .num = 3,
            },
            .want_labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"book",   "0"},
                    {"core",   "1"},
                    {"cpu",    "2"}
                },
                .num = 3,
            },
        },
        {
            .labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"drawer", "3"},
                    {"cpu",    "2"},
                    {"core",   "1"},
                    {"book",   "0"}
                },
                .num = 4,
            },
            .want_labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"book",   "0"},
                    {"core",   "1"},
                    {"cpu",    "2"},
                    {"drawer", "3"}
                },
                .num = 4,
            },
        },
        {
            .labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"drawer", "3"},
                    {"core",   "1"},
                    {"cpu",    "2"},
                    {"book",   "0"}
                },
                .num = 4,
            },
            .want_labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"book",   "0"},
                    {"core",   "1"},
                    {"cpu",    "2"},
                    {"drawer", "3"}
                },
                .num = 4,
            },
        },
        {
            .labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"book",   "0"},
                    {"drawer", "3"},
                    {"core",   "1"},
                    {"cpu",    "2"},
                },
                .num = 4,
            },
            .want_labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"book",   "0"},
                    {"core",   "1"},
                    {"cpu",    "2"},
                    {"drawer", "3"}
                },
                .num = 4,
            },
        },
        {
            .labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"node",   "4"},
                    {"socket", "5"},
                    {"cpu",    "2"},
                    {"drawer", "3"},
                    {"book",   "0"},
                    {"core",   "1"}
                },
                .num = 6,
            },
            .want_labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"book",   "0"},
                    {"core",   "1"},
                    {"cpu",    "2"},
                    {"drawer", "3"},
                    {"node",   "4"},
                    {"socket", "5"}
                },
                .num = 6,
            },
        },
        {
            .labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"book",   "0"},
                    {"core",   "1"},
                    {"cpu",    "2"},
                    {"drawer", "3"},
                    {"node",   "4"},
                    {"socket", "5"}
                },
                .num = 6,
            },
            .want_labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"book",   "0"},
                    {"core",   "1"},
                    {"cpu",    "2"},
                    {"drawer", "3"},
                    {"node",   "4"},
                    {"socket", "5"}
                },
                .num = 6,
            },
        },
        {
            .labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"type",   "test"},
                    {"common", "label"},
                },
                .num = 2,
            },
            .want_labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"common", "label"},
                    {"type",   "test"},
                },
                .num = 2,
            }
        },
        {
            .labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"common", "label"},
                    {"type",   "test"},
                },
                .num = 2,
            },
            .want_labels = (label_set_t){
                .ptr = (label_pair_t[]){
                    {"common", "label"},
                    {"type",   "test"},
                },
                .num = 2,
            }
        },
    };

    for (size_t i = 0; i < (sizeof(cases) / sizeof(cases[0])); i++) {
        label_set_t labels = {0};

        for (size_t j = 0; j < cases[i].labels.num; j++) {
            label_set_add(&labels, true, cases[i].labels.ptr[j].name, cases[i].labels.ptr[j].value);
        }

        EXPECT_EQ_INT(cases[i].want_labels.num, labels.num);

        for (size_t j = 0; j < labels.num; j++) {
            EXPECT_EQ_STR(cases[i].want_labels.ptr[j].name, labels.ptr[j].name);
        }

        for (size_t j = 0; j < cases[i].want_labels.num; j++) {
            label_pair_t *pair = label_set_read(labels, cases[i].want_labels.ptr[j].name);
            EXPECT_EQ_STR(cases[i].want_labels.ptr[j].value, pair->value);
        }

        label_set_reset(&labels);
    }

    return 0;
}

int main(void)
{
    RUN_TEST(label_set_add);

    END_TEST;
}
