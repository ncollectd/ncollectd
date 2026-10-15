// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2026 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libtest/testing.h"
#include "libconfig/config.h"

extern void module_register(void);

DEF_TEST(test01)
{
    char *config = "collect all";
    config_item_t *ci = config_parse_buffer(config, strlen(config));
    CHECK_NOT_NULL(ci);

    EXPECT_EQ_INT(0, plugin_test_do_read(NULL, "src/plugins/xfs/test01/sys", ci,
                                         "src/plugins/xfs/test01/expect.txt"));

    config_free(ci);

    return 0;
}

int main(void)
{
    module_register();

    RUN_TEST(test01);

    END_TEST;
}
