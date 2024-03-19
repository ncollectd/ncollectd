// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libtest/testing.h"

extern void module_register(void);

DEF_TEST(test01)
{
    EXPECT_EQ_INT(0, plugin_test_do_read("src/plugins/drbd/test01/proc", NULL, NULL,
                                         "src/plugins/drbd/test01/expect.txt"));
    return 0;
}

DEF_TEST(test02)
{
    EXPECT_EQ_INT(0, plugin_test_do_read("src/plugins/drbd/test02/proc", NULL, NULL,
                                         "src/plugins/drbd/test02/expect.txt"));
    return 0;
}

DEF_TEST(test03)
{
    EXPECT_EQ_INT(0, plugin_test_do_read("src/plugins/drbd/test03/proc", NULL, NULL,
                                         "src/plugins/drbd/test03/expect.txt"));
    return 0;
}

DEF_TEST(test04)
{
    EXPECT_EQ_INT(0, plugin_test_do_read("src/plugins/drbd/test04/proc", NULL, NULL,
                                         "src/plugins/drbd/test04/expect.txt"));
    return 0;
}

int main(void)
{
    module_register();

    RUN_TEST(test01);
    RUN_TEST(test02);
    RUN_TEST(test03);
    RUN_TEST(test04);

    END_TEST;
}
