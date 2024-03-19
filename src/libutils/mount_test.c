// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2013 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

#include "ncollectd.h"
#include "libutils/common.h"
#include "libutils/mount.h"
#include "libtest/testing.h"

DEF_TEST(cu_mount_checkoption)
{
    char line_opts[] = "foo=one,bar=two,qux=three";
    char *foo = strstr(line_opts, "foo");
    char *bar = strstr(line_opts, "bar");
    char *qux = strstr(line_opts, "qux");

    char line_bool[] = "one,two,three";
    char *one = strstr(line_bool, "one");
    char *two = strstr(line_bool, "two");
    char *three = strstr(line_bool, "three");

    /* Normal operation */
    OK(foo == cu_mount_checkoption(line_opts, "foo", 0));
    OK(bar == cu_mount_checkoption(line_opts, "bar", 0));
    OK(qux == cu_mount_checkoption(line_opts, "qux", 0));
    OK(NULL == cu_mount_checkoption(line_opts, "unknown", 0));

    OK(one == cu_mount_checkoption(line_bool, "one", 0));
    OK(two == cu_mount_checkoption(line_bool, "two", 0));
    OK(three == cu_mount_checkoption(line_bool, "three", 0));
    OK(NULL == cu_mount_checkoption(line_bool, "four", 0));

    /* Shorter and longer parts */
    OK(foo == cu_mount_checkoption(line_opts, "fo", 0));
    OK(bar == cu_mount_checkoption(line_opts, "bar=", 0));
    OK(qux == cu_mount_checkoption(line_opts, "qux=thr", 0));

    OK(one == cu_mount_checkoption(line_bool, "o", 0));
    OK(two == cu_mount_checkoption(line_bool, "tw", 0));
    OK(three == cu_mount_checkoption(line_bool, "thr", 0));

    /* "full" flag */
    OK(one == cu_mount_checkoption(line_bool, "one", 1));
    OK(two == cu_mount_checkoption(line_bool, "two", 1));
    OK(three == cu_mount_checkoption(line_bool, "three", 1));
    OK(NULL == cu_mount_checkoption(line_bool, "four", 1));

    OK(NULL == cu_mount_checkoption(line_bool, "o", 1));
    OK(NULL == cu_mount_checkoption(line_bool, "tw", 1));
    OK(NULL == cu_mount_checkoption(line_bool, "thr", 1));

    return 0;
}

DEF_TEST(cu_mount_getoptionvalue)
{
    char line_opts[] = "foo=one,bar=two,qux=three";
    char line_bool[] = "one,two,three";
    char *v;

    EXPECT_EQ_STR("one", v = cu_mount_getoptionvalue(line_opts, "foo="));
    free(v);
    EXPECT_EQ_STR("two", v = cu_mount_getoptionvalue(line_opts, "bar="));
    free(v);
    EXPECT_EQ_STR("three", v = cu_mount_getoptionvalue(line_opts, "qux="));
    free(v);
    OK(NULL == (v = cu_mount_getoptionvalue(line_opts, "unknown=")));
    free(v);

    EXPECT_EQ_STR("", v = cu_mount_getoptionvalue(line_bool, "one"));
    free(v);
    EXPECT_EQ_STR("", v = cu_mount_getoptionvalue(line_bool, "two"));
    free(v);
    EXPECT_EQ_STR("", v = cu_mount_getoptionvalue(line_bool, "three"));
    free(v);
    OK(NULL == (v = cu_mount_getoptionvalue(line_bool, "four")));
    free(v);

    return 0;
}

int main(void)
{
    RUN_TEST(cu_mount_checkoption);
    RUN_TEST(cu_mount_getoptionvalue);

    END_TEST;
}
