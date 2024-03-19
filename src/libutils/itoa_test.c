// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libutils/itoa.h"
#include "libtest/testing.h"

DEF_TEST(uitoa)
{
    char buffer[ITOA_MAX] = {0};

    OK(uitoa(0, buffer) == 1);
    EXPECT_EQ_STR("0", buffer);

    OK(uitoa(1, buffer) == 1);
    EXPECT_EQ_STR("1", buffer);

    OK(uitoa(100, buffer) == 3);
    EXPECT_EQ_STR("100", buffer);

    OK(uitoa(UINT32_MAX, buffer) == 10);
    EXPECT_EQ_STR("4294967295", buffer);

    OK(uitoa(UINT64_MAX, buffer) == 20);
    EXPECT_EQ_STR("18446744073709551615", buffer);

    return 0;
}

DEF_TEST(itoa)
{
    char buffer[ITOA_MAX] = {0};

    OK(itoa(INT64_MIN, buffer) == 20);
    EXPECT_EQ_STR("-9223372036854775808", buffer);

    OK(itoa(INT32_MIN, buffer) == 11);
    EXPECT_EQ_STR("-2147483648", buffer);

    OK(itoa(-999, buffer) == 4);
    EXPECT_EQ_STR("-999", buffer);

    OK(itoa(-100, buffer) == 4);
    EXPECT_EQ_STR("-100", buffer);

    OK(itoa(-1, buffer) == 2);
    EXPECT_EQ_STR("-1", buffer);

    OK(itoa(0, buffer) == 1);
    EXPECT_EQ_STR("0", buffer);

    OK(itoa(1, buffer) == 1);
    EXPECT_EQ_STR("1", buffer);

    OK(itoa(100, buffer) == 3);
    EXPECT_EQ_STR("100", buffer);

    OK(itoa(999, buffer) == 3);
    EXPECT_EQ_STR("999", buffer);

    OK(itoa(INT32_MAX, buffer) == 10);
    EXPECT_EQ_STR("2147483647", buffer);

    OK(itoa(INT64_MAX, buffer) == 19);
    EXPECT_EQ_STR("9223372036854775807", buffer);

    return 0;
}

int main(void)
{
    RUN_TEST(uitoa);
    RUN_TEST(itoa);

    END_TEST;
}
