// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2020 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

#include "libutils/strbuf.h"
#include "libtest/testing.h"

#include <errno.h>
#include <stdbool.h>

#define STATIC_BUFFER_SIZE 9

int test_buffer(strbuf_t *buf, bool is_static)
{
    CHECK_ZERO(strbuf_print(buf, "foo"));
    EXPECT_EQ_STR("foo", buf->ptr);

    CHECK_ZERO(strbuf_print(buf, "bar"));
    EXPECT_EQ_STR("foobar", buf->ptr);

    if (is_static)
        CHECK_ZERO(!strbuf_putint(buf, 9000));
    else
        CHECK_ZERO(strbuf_putint(buf, 9000));
    char const *want = is_static ? "foobar" : "foobar9000";
    EXPECT_EQ_STR(want, buf->ptr);

    if (is_static) {
        EXPECT_EQ_INT(ENOMEM, strbuf_print(buf, "buffer already filled"));
        EXPECT_EQ_STR("foobar", buf->ptr);
    }

    strbuf_reset(buf);
    CHECK_ZERO(strlen(buf->ptr));

    if (is_static)
        CHECK_ZERO(!strbuf_print(buf, "new content"));
    else
        CHECK_ZERO(strbuf_print(buf, "new content"));
    want = is_static ? "" : "new content";
    EXPECT_EQ_STR(want, buf->ptr);

    strbuf_reset(buf);
    CHECK_ZERO(strlen(buf->ptr));

    CHECK_ZERO(strbuf_printn(buf, "foobar", 3));
    EXPECT_EQ_STR("foo", buf->ptr);

    return 0;
}

DEF_TEST(fixed_heap)
{
    char mem[STATIC_BUFFER_SIZE];
    strbuf_t buf = STRBUF_CREATE_FIXED(mem, sizeof(mem));

    int status = test_buffer(&buf, true);

    strbuf_destroy(&buf);
    return status;
}

DEF_TEST(dynamic_stack)
{
    strbuf_t buf = {0};
    buf = STRBUF_CREATE;

    int status = test_buffer(&buf, false);

    strbuf_destroy(&buf);
    return status;
}

DEF_TEST(fixed_stack)
{
    /* This somewhat unusual syntax ensures that `sizeof(b)` will return a wrong
     * number (size of the pointer, not the buffer; usually 4 or 8, depending on
     * architecture), failing the test. */
    char *b = (char[STATIC_BUFFER_SIZE]){0};
    size_t sz = STATIC_BUFFER_SIZE;
    strbuf_t buf = {0};
    buf = STRBUF_CREATE_FIXED(b, sz);

    int status = test_buffer(&buf, true);

    strbuf_destroy(&buf);
    return status;
}

DEF_TEST(static_stack)
{
    char b[STATIC_BUFFER_SIZE];
    strbuf_t buf = {0};
    buf = STRBUF_CREATE_STATIC(b);

    int status = test_buffer(&buf, true);

    strbuf_destroy(&buf);
    return status;
}

DEF_TEST(print_escaped)
{
    struct {
        char const *s;
        char const *need_escape;
        char escape_char;
        char const *want;
    } cases[] = {
            {
                    .s = "normal string",
                    .need_escape = "\\\"\n\r\t",
                    .escape_char = '\\',
                    .want = "normal string",
            },
            {
                    .s = "\"special\"\n",
                    .need_escape = "\\\"\n\r\t",
                    .escape_char = '\\',
                    .want = "\\\"special\\\"\\n",
            },
            {
                    /* string gets truncated */
                    .s = "0123456789ABCDEF",
                    .need_escape = ">",
                    .escape_char = '<',
                    .want = "0123456789ABCDEF",
            },
            {
                    /* string gets truncated */
                    .s = "0123456789>BCDEF",
                    .need_escape = ">",
                    .escape_char = '<',
                    .want = "0123456789<>BCDEF",
            },
            {
                    /* truncation between escape_char and to-be-escaped char. */
                    .s = "0123456789ABCD>F",
                    .need_escape = ">",
                    .escape_char = '<',
                    .want = "0123456789ABCD<>F",
            },
    };

    for (size_t i = 0; i < (sizeof(cases) / sizeof(cases[0])); i++) {
        char mem[32] = {0};
        strbuf_t buf = STRBUF_CREATE_STATIC(mem);

        CHECK_ZERO(strbuf_print_escaped(&buf, cases[i].s, cases[i].need_escape,
                                                          cases[i].escape_char));
        EXPECT_EQ_STR(cases[i].want, buf.ptr);

        strbuf_destroy(&buf);
    }

    return 0;
}

int main(void)
{
    RUN_TEST(fixed_heap);
    RUN_TEST(dynamic_stack);
    RUN_TEST(fixed_stack);
    RUN_TEST(static_stack);
    RUN_TEST(print_escaped);

    END_TEST;
}
