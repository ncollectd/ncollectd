// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2013 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

/*
 * Explicit order is required or _FILE_OFFSET_BITS will have definition mismatches on Solaris
 * See Github Issue #3193 for details
 */
#include "libutils/common.h"
#include "libtest/testing.h"

DEF_TEST(sstrncpy)
{
    unsigned char buffer[16] = "";
    char *ptr = (char *)&buffer[4];
    char *ret;

    buffer[0] = buffer[1] = buffer[2] = buffer[3] = 0xff;
    buffer[12] = buffer[13] = buffer[14] = buffer[15] = 0xff;

    ret = sstrncpy(ptr, "foobar", 8);
    OK(ret == ptr);
    EXPECT_EQ_STR("foobar", ptr);
    OK(buffer[3] == buffer[12]);

    ret = sstrncpy(ptr, "abc", 8);
    OK(ret == ptr);
    EXPECT_EQ_STR("abc", ptr);
    OK(buffer[3] == buffer[12]);

    ret = sstrncpy(ptr, "ncollectd", 8);
    OK(ret == ptr);
    OK(ptr[7] == 0);
    EXPECT_EQ_STR("ncollec", ptr);
    OK(buffer[3] == buffer[12]);

    return 0;
}

DEF_TEST(sstrdup)
{
    char *ptr;

    ptr = sstrdup("ncollectd");
    OK(ptr != NULL);
    EXPECT_EQ_STR("ncollectd", ptr);

    free(ptr);

    ptr = sstrdup(NULL);
    OK(ptr == NULL);

    return 0;
}

DEF_TEST(strsplit)
{
    char buffer[32];
    char *fields[8];
    int status;

    strncpy(buffer, "foo bar", sizeof(buffer));
    status = strsplit(buffer, fields, 8);
    OK(status == 2);
    EXPECT_EQ_STR("foo", fields[0]);
    EXPECT_EQ_STR("bar", fields[1]);

    strncpy(buffer, "foo \t bar", sizeof(buffer));
    status = strsplit(buffer, fields, 8);
    OK(status == 2);
    EXPECT_EQ_STR("foo", fields[0]);
    EXPECT_EQ_STR("bar", fields[1]);

    strncpy(buffer, "one two\tthree\rfour\nfive", sizeof(buffer));
    status = strsplit(buffer, fields, 8);
    OK(status == 5);
    EXPECT_EQ_STR("one", fields[0]);
    EXPECT_EQ_STR("two", fields[1]);
    EXPECT_EQ_STR("three", fields[2]);
    EXPECT_EQ_STR("four", fields[3]);
    EXPECT_EQ_STR("five", fields[4]);

    strncpy(buffer, "\twith trailing\n", sizeof(buffer));
    status = strsplit(buffer, fields, 8);
    OK(status == 2);
    EXPECT_EQ_STR("with", fields[0]);
    EXPECT_EQ_STR("trailing", fields[1]);

    strncpy(buffer, "1 2 3 4 5 6 7 8 9 10 11 12 13", sizeof(buffer));
    status = strsplit(buffer, fields, 8);
    OK(status == 8);
    EXPECT_EQ_STR("7", fields[6]);
    EXPECT_EQ_STR("8", fields[7]);

    strncpy(buffer, "single", sizeof(buffer));
    status = strsplit(buffer, fields, 8);
    OK(status == 1);
    EXPECT_EQ_STR("single", fields[0]);

    strncpy(buffer, "", sizeof(buffer));
    status = strsplit(buffer, fields, 8);
    OK(status == 0);

    return 0;
}

DEF_TEST(strjoin)
{
    struct {
        char **fields;
        size_t fields_num;
        char *separator;

        int want_return;
        char *want_buffer;
    } cases[] = {
            /* Normal case. */
            {(char *[]){"foo", "bar"}, 2, "!", 7, "foo!bar"},
            /* One field only. */
            {(char *[]){"foo"}, 1, "!", 3, "foo"},
            /* No fields at all. */
            {NULL, 0, "!", 0, ""},
            /* Longer separator. */
            {(char *[]){"foo", "bar"}, 2, "rcht", 10, "foorchtbar"},
            /* Empty separator. */
            {(char *[]){"foo", "bar"}, 2, "", 6, "foobar"},
            /* NULL separator. */
            {(char *[]){"foo", "bar"}, 2, NULL, 6, "foobar"},
            /* buffer not large enough -> string is truncated. */
            {(char *[]){"aaaaaa", "bbbbbb", "c!"}, 3, "-", 16, "aaaaaa-bbbbbb-c"},
            /* buffer not large enough -> last field fills buffer completely. */
            {(char *[]){"aaaaaaa", "bbbbbbb", "!"}, 3, "-", 17, "aaaaaaa-bbbbbbb"},
            /* buffer not large enough -> string does *not* end in separator. */
            {(char *[]){"aaaa", "bbbb", "cccc", "!"}, 4, "-", 16, "aaaa-bbbb-cccc"},
            /* buffer not large enough -> string does not end with partial
                 separator. */
            {(char *[]){"aaaaaa", "bbbbbb", "!"}, 3, "+-", 17, "aaaaaa+-bbbbbb"},
    };

    for (size_t i = 0; i < STATIC_ARRAY_SIZE(cases); i++) {
        char buffer[16];
        int status;

        memset(buffer, 0xFF, sizeof(buffer));
        status = strjoin(buffer, sizeof(buffer), cases[i].fields,
                                 cases[i].fields_num, cases[i].separator);
        EXPECT_EQ_INT(cases[i].want_return, status);
        EXPECT_EQ_STR(cases[i].want_buffer, buffer);

        /* use (NULL, 0) to determine required buffer size. */
        EXPECT_EQ_INT(cases[i].want_return,
                      strjoin(NULL, 0, cases[i].fields, cases[i].fields_num, cases[i].separator));
    }

    return 0;
}

DEF_TEST(escape_slashes)
{
    struct {
        char *str;
        char *want;
    } cases[] = {
            {"foo/bar/baz", "foo_bar_baz"},
            {"/like/a/path", "like_a_path"},
            {"trailing/slash/", "trailing_slash_"},
            {"foo//bar", "foo__bar"},
    };

    for (size_t i = 0; i < STATIC_ARRAY_SIZE(cases); i++) {
        char buffer[32] = {0};

        strncpy(buffer, cases[i].str, sizeof(buffer) - 1);
        OK(escape_slashes(buffer, sizeof(buffer)) == 0);
        EXPECT_EQ_STR(cases[i].want, buffer);
    }

    return 0;
}

DEF_TEST(escape_string)
{
    struct {
        char *str;
        char *want;
    } cases[] = {
            {"foobar", "foobar"},
            {"f00bar", "f00bar"},
            {"foo bar", "\"foo bar\""},
            {"foo \"bar\"", "\"foo \\\"bar\\\"\""},
            {"012345678901234", "012345678901234"},
            {"012345 78901234", "\"012345 789012\""},
            {"012345 78901\"34", "\"012345 78901\""},
    };

    for (size_t i = 0; i < STATIC_ARRAY_SIZE(cases); i++) {
        char buffer[16] = {0};

        strncpy(buffer, cases[i].str, sizeof(buffer) - 1);
        OK(escape_string(buffer, sizeof(buffer)) == 0);
        EXPECT_EQ_STR(cases[i].want, buffer);
    }

    return 0;
}

DEF_TEST(strunescape)
{
    char buffer[32] = {0};
    int status;

    strncpy(buffer, "foo\\tbar", sizeof(buffer) - 1);
    status = strunescape(buffer, sizeof(buffer));
    OK(status == 0);
    EXPECT_EQ_STR("foo\tbar", buffer);

    strncpy(buffer, "\\tfoo\\r\\n", sizeof(buffer) - 1);
    status = strunescape(buffer, sizeof(buffer));
    OK(status == 0);
    EXPECT_EQ_STR("\tfoo\r\n", buffer);

    strncpy(buffer, "With \\\"quotes\\\"", sizeof(buffer) - 1);
    status = strunescape(buffer, sizeof(buffer));
    OK(status == 0);
    EXPECT_EQ_STR("With \"quotes\"", buffer);

    /* Backslash before null byte */
    strncpy(buffer, "\\tbackslash end\\", sizeof(buffer) - 1);
    status = strunescape(buffer, sizeof(buffer));
    OK(status != 0);
    EXPECT_EQ_STR("\tbackslash end", buffer);
    return 0;

    /* Backslash at buffer end */
    strncpy(buffer, "\\t3\\56", sizeof(buffer) - 1);
    status = strunescape(buffer, 4);
    OK(status != 0);
    OK(buffer[0] == '\t');
    OK(buffer[1] == '3');
    OK(buffer[2] == 0);
    OK(buffer[3] == 0);
    OK(buffer[4] == '5');
    OK(buffer[5] == '6');
    OK(buffer[6] == '7');

    return 0;
}

int main(void)
{
    RUN_TEST(sstrncpy);
    RUN_TEST(sstrdup);
    RUN_TEST(strsplit);
    RUN_TEST(strjoin);
    RUN_TEST(escape_slashes);
    RUN_TEST(escape_string);
    RUN_TEST(strunescape);

    END_TEST;
}
