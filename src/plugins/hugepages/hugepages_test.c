// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com

#include "libtest/testing.h"
#include "libutils/common.h"

extern void module_register(void);

DEF_TEST(test01)
{
    config_item_t ci = (config_item_t) {
        .key = "plugin",
        .values_num = 1,
        .values = (config_value_t[]) {{.type = CONFIG_TYPE_STRING, .value.string ="hugepages"}},
        .children_num = 1,
        .children = (config_item_t[]) {
            {
                .key = "values-bytes",
                .values_num = 1,
                .values = (config_value_t[]) {
                    {
                        .type = CONFIG_TYPE_BOOLEAN,
                        .value.boolean = true
                    },
                }
            }
        }
    };

    EXPECT_EQ_INT(0, plugin_test_do_read(NULL, "src/plugins/hugepages/test01/sys", &ci, 
                                         "src/plugins/hugepages/test01/expect.txt"));
    return 0;
}

int main(void)
{
    module_register();

    RUN_TEST(test01);

    END_TEST;
}
