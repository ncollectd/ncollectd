// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libtest/testing.h"

extern void module_register(void);

DEF_TEST(test02)
{
    EXPECT_EQ_INT(0, plugin_test_do_read("src/plugins/cifs/test01/proc", NULL, NULL,
                                         "src/plugins/cifs/test01/expect.txt"));
    return 0;
}

DEF_TEST(test01)
{
    EXPECT_EQ_INT(0, plugin_test_do_read("src/plugins/cifs/test02/proc", NULL, NULL,
                                         "src/plugins/cifs/test02/expect.txt"));
    return 0;
}

int main(void)
{
    module_register();

    RUN_TEST(test01);
    RUN_TEST(test02);

    END_TEST;
}
