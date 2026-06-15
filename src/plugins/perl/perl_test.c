// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libtest/testing.h"
#include "libconfig/config.h"

extern void module_register(void);

DEF_TEST(test01)
{
    char *config = "include-dir \"src/plugins/perl/lib\"\n"
                   "include-dir \"src/plugins/perl/test01\"\n"
                   "base-name \"NCollectd::Plugins\"\n"
                   "load-plugin \"Test01\"\n";
    config_item_t *ci = config_parse_buffer(config, strlen(config));
    CHECK_NOT_NULL(ci);

    EXPECT_EQ_INT(0, plugin_test_do_read(NULL, NULL, ci, "src/plugins/perl/test01/expect.txt"));

    config_free(ci);

    return 0;
}

int main(void)
{
    module_register();

    RUN_TEST(test01);

    plugin_test_reset();

    END_TEST;
}
