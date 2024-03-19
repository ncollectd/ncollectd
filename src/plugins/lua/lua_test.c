// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libtest/testing.h"

extern void module_register(void);

DEF_TEST(test01)
{
    config_item_t ci = (config_item_t) {
        .key = "plugin",
        .values_num = 1,
        .values = (config_value_t[]) {{.type = CONFIG_TYPE_STRING, .value.string ="lua"}},
        .children_num = 2,
        .children = (config_item_t[]) {
            {
                .key = "base-path",
                .values_num = 1,
                .values = (config_value_t[]) {
                    {.type = CONFIG_TYPE_STRING, .value.string = "src/plugins/lua/test01"},
                }
            },
            {
                .key = "script",
                .values_num = 1,
                .values = (config_value_t[]) {
                    {.type = CONFIG_TYPE_STRING, .value.string = "test01.lua"},
                }
            },
        }
    };

    EXPECT_EQ_INT(0, plugin_test_do_read(NULL, NULL, &ci, "src/plugins/lua/test01/expect.txt"));

    return 0;
}

int main(void)
{
    module_register();

    RUN_TEST(test01);

    plugin_test_reset();

    END_TEST;
}
