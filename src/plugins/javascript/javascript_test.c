// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libtest/testing.h"

extern void module_register(void);

DEF_TEST(test01)
{
    config_item_t ci = (config_item_t) {
        .key = "plugin",
        .values_num = 1,
        .values = (config_value_t[]) {{.type = CONFIG_TYPE_STRING, .value.string ="javascript"}},
        .children_num = 1,
        .children = (config_item_t[]) {
            {
                .key = "instance",
                .values_num = 1,
                .values = (config_value_t[]) {
                    {.type = CONFIG_TYPE_STRING, .value.string = "local"},
                },
                .children_num = 1,
                .children = (config_item_t[]) {
                    {
                        .key = "script",
                        .values_num = 1,
                        .values = (config_value_t[]) {
                            {.type = CONFIG_TYPE_STRING, .value.string = "src/plugins/javascript/test01/test01.js"},
                        }
                    }
                }
            }
        }
    };

    EXPECT_EQ_INT(0, plugin_test_do_read(NULL, NULL, &ci, "src/plugins/javascript/test01/expect.txt"));

    return 0;
}

int main(void)
{
    module_register();

    RUN_TEST(test01);

    plugin_test_reset();

    END_TEST;
}
