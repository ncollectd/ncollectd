// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libtest/testing.h"

extern void module_register(void);

DEF_TEST(test01)
{
    EXPECT_EQ_INT(0, plugin_test_do_read(NULL, "src/plugins/bonding/test01/sys", NULL,
                                         "src/plugins/bonding/test01/expect.txt"));
    return 0;
}

int main(void)
{
    module_register();

    RUN_TEST(test01);

    END_TEST;
}
