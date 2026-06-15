// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libtest/testing.h"

extern void module_register(void);

DEF_TEST(test01)
{
    EXPECT_EQ_INT(0, plugin_test_do_read("src/plugins/cpu/test01/proc", NULL, NULL,
                                         "src/plugins/cpu/test01/expect.txt"));
    return 0;
}

DEF_TEST(test02)
{

    char *config = "report-topology true\n";
    config_item_t *ci = config_parse_buffer(config, strlen(config));
    CHECK_NOT_NULL(ci);

    EXPECT_EQ_INT(0, plugin_test_do_read("src/plugins/cpu/test02/proc",
                                         "src/plugins/cpu/test02/sys", ci,
                                         "src/plugins/cpu/test02/expect.txt"));

    config_free(ci);

    return 0;
}

int main(void)
{
    module_register();

    RUN_TEST(test01);
    RUN_TEST(test02);

    END_TEST;
}
