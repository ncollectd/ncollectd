// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libutils/dtoa.h"
#include "libtest/testing.h"

DEF_TEST(dtoa)
{
    char buffer[DTOA_MAX] = {0};

    OK(dtoa(-1E0, buffer, sizeof(buffer)) == 2);
    EXPECT_EQ_STR("-1", buffer);

    OK(dtoa(-1, buffer, sizeof(buffer)) == 2);
    EXPECT_EQ_STR("-1", buffer);

    OK(dtoa(-0, buffer, sizeof(buffer)) == 1);
    EXPECT_EQ_STR("0", buffer);

    OK(dtoa(0, buffer, sizeof(buffer)) == 1);
    EXPECT_EQ_STR("0", buffer);

    OK(dtoa(1E0, buffer, sizeof(buffer)) == 1);
    EXPECT_EQ_STR("1", buffer);

    OK(dtoa(1, buffer, sizeof(buffer)) == 1);
    EXPECT_EQ_STR("1", buffer);

    OK(dtoa(1.7976931348623157E308, buffer, sizeof(buffer)) == 23);
    EXPECT_EQ_STR("1.7976931348623157e+308", buffer);

    OK(dtoa(4.9E-324, buffer, sizeof(buffer)) == 6);
    EXPECT_EQ_STR("5e-324", buffer);

    OK(dtoa(5E-324, buffer, sizeof(buffer)) == 6);
    EXPECT_EQ_STR("5e-324", buffer);

    OK(dtoa(-INFINITY, buffer, sizeof(buffer)) == 4);
    EXPECT_EQ_STR("-inf", buffer);

    OK(dtoa(INFINITY, buffer, sizeof(buffer)) == 3);
    EXPECT_EQ_STR("inf", buffer);

    OK(dtoa(-NAN, buffer, sizeof(buffer)) == 4);
    EXPECT_EQ_STR("-nan", buffer);

    OK(dtoa(NAN, buffer, sizeof(buffer)) == 3);
    EXPECT_EQ_STR("nan", buffer);

    OK(dtoa(1.0E7, buffer, sizeof(buffer)) == 8);
    EXPECT_EQ_STR("10000000", buffer);

    OK(dtoa(9999999.999999998, buffer, sizeof(buffer)) == 17);
    EXPECT_EQ_STR("9999999.999999998", buffer);

    OK(dtoa(0.001, buffer, sizeof(buffer)) == 5);
    EXPECT_EQ_STR("0.001", buffer);

    OK(dtoa(9.999999999999998E-4, buffer, sizeof(buffer)) == 21);
    EXPECT_EQ_STR("0.0009999999999999998", buffer);

    OK(dtoa(2.2250738585072014E-308, buffer, sizeof(buffer)) == 23);
    EXPECT_EQ_STR("2.2250738585072014e-308", buffer);

    OK(dtoa(2.2250738585072014E-308, buffer, sizeof(buffer)) == 23);
    EXPECT_EQ_STR("2.2250738585072014e-308", buffer);

    OK(dtoa(-2.109808898695963E16, buffer, sizeof(buffer)) == 22);
    EXPECT_EQ_STR("-2.109808898695963e+16", buffer);

    OK(dtoa(4.940656E-318, buffer, sizeof(buffer)) == 13);
    EXPECT_EQ_STR("4.940656e-318", buffer);

    OK(dtoa(1.18575755E-316, buffer, sizeof(buffer)) == 15);
    EXPECT_EQ_STR("1.18575755e-316", buffer);

    OK(dtoa(2.989102097996E-312, buffer, sizeof(buffer)) == 19);
    EXPECT_EQ_STR("2.989102097996e-312", buffer);

    OK(dtoa(9.0608011534336E15, buffer, sizeof(buffer)) == 19);
    EXPECT_EQ_STR("9.0608011534336e+15", buffer);

    OK(dtoa(4.708356024711512E18, buffer, sizeof(buffer)) == 21);
    EXPECT_EQ_STR("4.708356024711512e+18", buffer);

    OK(dtoa(9.409340012568248E18, buffer, sizeof(buffer)) == 21);
    EXPECT_EQ_STR("9.409340012568248e+18", buffer);

    OK(dtoa(1.8531501765868567E21, buffer, sizeof(buffer)) == 22);
    EXPECT_EQ_STR("1.8531501765868567e+21", buffer);

    OK(dtoa(-3.347727380279489E33, buffer, sizeof(buffer)) == 22);
    EXPECT_EQ_STR("-3.347727380279489e+33", buffer);

    OK(dtoa(1.9430376160308388E16, buffer, sizeof(buffer)) == 22);
    EXPECT_EQ_STR("1.9430376160308388e+16", buffer);

    OK(dtoa(-6.9741824662760956E19, buffer, sizeof(buffer)) == 23);
    EXPECT_EQ_STR("-6.9741824662760956e+19", buffer);

    OK(dtoa(4.3816050601147837E18, buffer, sizeof(buffer)) == 22);
    EXPECT_EQ_STR("4.3816050601147837e+18", buffer);

    OK(dtoa(9.007199254740991E15, buffer, sizeof(buffer)) == 16);
    EXPECT_EQ_STR("9007199254740991", buffer);

    OK(dtoa(9.007199254740992E15, buffer, sizeof(buffer)) == 16);
    EXPECT_EQ_STR("9007199254740992", buffer);

    OK(dtoa(1E0, buffer, sizeof(buffer)) == 1);
    EXPECT_EQ_STR("1", buffer);

    OK(dtoa(1.2E1, buffer, sizeof(buffer)) == 2);
    EXPECT_EQ_STR("12", buffer);

    OK(dtoa(1.23E2, buffer, sizeof(buffer)) == 3);
    EXPECT_EQ_STR("123", buffer);

    OK(dtoa(1.234E3, buffer, sizeof(buffer)) == 4);
    EXPECT_EQ_STR("1234", buffer);

    OK(dtoa(1.2345E4, buffer, sizeof(buffer)) == 5);
    EXPECT_EQ_STR("12345", buffer);

    OK(dtoa(1.23456E5, buffer, sizeof(buffer)) == 6);
    EXPECT_EQ_STR("123456", buffer);

    OK(dtoa(1.234567E6, buffer, sizeof(buffer)) == 7);
    EXPECT_EQ_STR("1234567", buffer);

    OK(dtoa(1.2345678E7, buffer, sizeof(buffer)) == 8);
    EXPECT_EQ_STR("12345678", buffer);

    OK(dtoa(1.23456789E8, buffer, sizeof(buffer)) == 9);
    EXPECT_EQ_STR("123456789", buffer);

    OK(dtoa(1.23456789E9, buffer, sizeof(buffer)) == 10);
    EXPECT_EQ_STR("1234567890", buffer);

    OK(dtoa(1.234567895E9, buffer, sizeof(buffer)) == 10);
    EXPECT_EQ_STR("1234567895", buffer);

    OK(dtoa(1.2345678901E10, buffer, sizeof(buffer)) == 11);
    EXPECT_EQ_STR("12345678901", buffer);

    OK(dtoa(1.23456789012E11, buffer, sizeof(buffer)) == 12);
    EXPECT_EQ_STR("123456789012", buffer);

    OK(dtoa(1.234567890123E12, buffer, sizeof(buffer)) == 13);
    EXPECT_EQ_STR("1234567890123", buffer);

    OK(dtoa(1.2345678901234E13, buffer, sizeof(buffer)) == 14);
    EXPECT_EQ_STR("12345678901234", buffer);

    OK(dtoa(1.23456789012345E14, buffer, sizeof(buffer)) == 15);
    EXPECT_EQ_STR("123456789012345", buffer);

    OK(dtoa(1.234567890123456E15, buffer, sizeof(buffer)) == 16);
    EXPECT_EQ_STR("1234567890123456", buffer);

    OK(dtoa(1E0, buffer, sizeof(buffer)) == 1);
    EXPECT_EQ_STR("1", buffer);

    OK(dtoa(1E1, buffer, sizeof(buffer)) == 2);
    EXPECT_EQ_STR("10", buffer);

    OK(dtoa(1E2, buffer, sizeof(buffer)) == 3);
    EXPECT_EQ_STR("100", buffer);

    OK(dtoa(1E3, buffer, sizeof(buffer)) == 4);
    EXPECT_EQ_STR("1000", buffer);

    OK(dtoa(1E4, buffer, sizeof(buffer)) == 5);
    EXPECT_EQ_STR("10000", buffer);

    OK(dtoa(1E5, buffer, sizeof(buffer)) == 6);
    EXPECT_EQ_STR("100000", buffer);

    OK(dtoa(1E6, buffer, sizeof(buffer)) == 7);
    EXPECT_EQ_STR("1000000", buffer);

    OK(dtoa(1E7, buffer, sizeof(buffer)) == 8);
    EXPECT_EQ_STR("10000000", buffer);

    OK(dtoa(1E8, buffer, sizeof(buffer)) == 9);
    EXPECT_EQ_STR("100000000", buffer);

    OK(dtoa(1E9, buffer, sizeof(buffer)) == 10);
    EXPECT_EQ_STR("1000000000", buffer);

    OK(dtoa(1E10, buffer, sizeof(buffer)) == 11);
    EXPECT_EQ_STR("10000000000", buffer);

    OK(dtoa(1E11, buffer, sizeof(buffer)) == 12);
    EXPECT_EQ_STR("100000000000", buffer);

    OK(dtoa(1E12, buffer, sizeof(buffer)) == 13);
    EXPECT_EQ_STR("1000000000000", buffer);

    OK(dtoa(1E13, buffer, sizeof(buffer)) == 14);
    EXPECT_EQ_STR("10000000000000", buffer);

    OK(dtoa(1E14, buffer, sizeof(buffer)) == 15);
    EXPECT_EQ_STR("100000000000000", buffer);

    OK(dtoa(1E15, buffer, sizeof(buffer)) == 16);
    EXPECT_EQ_STR("1000000000000000", buffer);

    OK(dtoa(1.000000000000001E15, buffer, sizeof(buffer)) == 16);
    EXPECT_EQ_STR("1000000000000001", buffer);

    OK(dtoa(1.00000000000001E15, buffer, sizeof(buffer)) == 16);
    EXPECT_EQ_STR("1000000000000010", buffer);

    OK(dtoa(1.0000000000001E15, buffer, sizeof(buffer)) == 16);
    EXPECT_EQ_STR("1000000000000100", buffer);

    OK(dtoa(1.000000000001E15, buffer, sizeof(buffer)) == 16);
    EXPECT_EQ_STR("1000000000001000", buffer);

    OK(dtoa(1.00000000001E15, buffer, sizeof(buffer)) == 16);
    EXPECT_EQ_STR("1000000000010000", buffer);

    OK(dtoa(1.0000000001E15, buffer, sizeof(buffer)) == 16);
    EXPECT_EQ_STR("1000000000100000", buffer);

    OK(dtoa(1.000000001E15, buffer, sizeof(buffer)) == 16);
    EXPECT_EQ_STR("1000000001000000", buffer);

    OK(dtoa(1.00000001E15, buffer, sizeof(buffer)) == 16);
    EXPECT_EQ_STR("1000000010000000", buffer);

    OK(dtoa(1.0000001E15, buffer, sizeof(buffer)) == 16);
    EXPECT_EQ_STR("1000000100000000", buffer);

    OK(dtoa(1.000001E15, buffer, sizeof(buffer)) == 16);
    EXPECT_EQ_STR("1000001000000000", buffer);

    OK(dtoa(1.00001E15, buffer, sizeof(buffer)) == 16);
    EXPECT_EQ_STR("1000010000000000", buffer);

    OK(dtoa(1.0001E15, buffer, sizeof(buffer)) == 16);
    EXPECT_EQ_STR("1000100000000000", buffer);

    OK(dtoa(1.001E15, buffer, sizeof(buffer)) == 16);
    EXPECT_EQ_STR("1001000000000000", buffer);

    OK(dtoa(1.01E15, buffer, sizeof(buffer)) == 16);
    EXPECT_EQ_STR("1010000000000000", buffer);

    OK(dtoa(1.1E15, buffer, sizeof(buffer)) == 16);
    EXPECT_EQ_STR("1100000000000000", buffer);

    OK(dtoa(8E0, buffer, sizeof(buffer)) == 1);
    EXPECT_EQ_STR("8", buffer);

    OK(dtoa(6.4E1, buffer, sizeof(buffer)) == 2);
    EXPECT_EQ_STR("64", buffer);

    OK(dtoa(5.12E2, buffer, sizeof(buffer)) == 3);
    EXPECT_EQ_STR("512", buffer);

    OK(dtoa(8.192E3, buffer, sizeof(buffer)) == 4);
    EXPECT_EQ_STR("8192", buffer);

    OK(dtoa(6.5536E4, buffer, sizeof(buffer)) == 5);
    EXPECT_EQ_STR("65536", buffer);

    OK(dtoa(5.24288E5, buffer, sizeof(buffer)) == 6);
    EXPECT_EQ_STR("524288", buffer);

    OK(dtoa(8.388608E6, buffer, sizeof(buffer)) == 7);
    EXPECT_EQ_STR("8388608", buffer);

    OK(dtoa(6.7108864E7, buffer, sizeof(buffer)) == 8);
    EXPECT_EQ_STR("67108864", buffer);

    OK(dtoa(5.36870912E8, buffer, sizeof(buffer)) == 9);
    EXPECT_EQ_STR("536870912", buffer);

    OK(dtoa(8.589934592E9, buffer, sizeof(buffer)) == 10);
    EXPECT_EQ_STR("8589934592", buffer);

    OK(dtoa(6.8719476736E10, buffer, sizeof(buffer)) == 11);
    EXPECT_EQ_STR("68719476736", buffer);

    OK(dtoa(5.49755813888E11, buffer, sizeof(buffer)) == 12);
    EXPECT_EQ_STR("549755813888", buffer);

    OK(dtoa(8.796093022208E12, buffer, sizeof(buffer)) == 13);
    EXPECT_EQ_STR("8796093022208", buffer);

    OK(dtoa(7.0368744177664E13, buffer, sizeof(buffer)) == 14);
    EXPECT_EQ_STR("70368744177664", buffer);

    OK(dtoa(5.62949953421312E14, buffer, sizeof(buffer)) == 15);
    EXPECT_EQ_STR("562949953421312", buffer);

    OK(dtoa(9.007199254740992E15, buffer, sizeof(buffer)) == 16);
    EXPECT_EQ_STR("9007199254740992", buffer);

    OK(dtoa(8E3, buffer, sizeof(buffer)) == 4);
    EXPECT_EQ_STR("8000", buffer);

    OK(dtoa(6.4E4, buffer, sizeof(buffer)) == 5);
    EXPECT_EQ_STR("64000", buffer);

    OK(dtoa(5.12E5, buffer, sizeof(buffer)) == 6);
    EXPECT_EQ_STR("512000", buffer);

    OK(dtoa(8.192E6, buffer, sizeof(buffer)) == 7);
    EXPECT_EQ_STR("8192000", buffer);

    OK(dtoa(6.5536E7, buffer, sizeof(buffer)) == 8);
    EXPECT_EQ_STR("65536000", buffer);

    OK(dtoa(5.24288E8, buffer, sizeof(buffer)) == 9);
    EXPECT_EQ_STR("524288000", buffer);

    OK(dtoa(8.388608E9, buffer, sizeof(buffer)) == 10);
    EXPECT_EQ_STR("8388608000", buffer);

    OK(dtoa(6.7108864E10, buffer, sizeof(buffer)) == 11);
    EXPECT_EQ_STR("67108864000", buffer);

    OK(dtoa(5.36870912E11, buffer, sizeof(buffer)) == 12);
    EXPECT_EQ_STR("536870912000", buffer);

    OK(dtoa(8.589934592E12, buffer, sizeof(buffer)) == 13);
    EXPECT_EQ_STR("8589934592000", buffer);

    OK(dtoa(6.8719476736E13, buffer, sizeof(buffer)) == 14);
    EXPECT_EQ_STR("68719476736000", buffer);

    OK(dtoa(5.49755813888E14, buffer, sizeof(buffer)) == 15);
    EXPECT_EQ_STR("549755813888000", buffer);

    OK(dtoa(8.796093022208E15, buffer, sizeof(buffer)) == 16);
    EXPECT_EQ_STR("8796093022208000", buffer);

    OK(dtoa(2.9802322387695312E-8, buffer, sizeof(buffer)) == 22);
    EXPECT_EQ_STR("2.9802322387695312e-08", buffer);

    OK(dtoa(5.764607523034235E39, buffer, sizeof(buffer)) == 21);
    EXPECT_EQ_STR("5.764607523034235e+39", buffer);

    OK(dtoa(1.152921504606847E40, buffer, sizeof(buffer)) == 21);
    EXPECT_EQ_STR("1.152921504606847e+40", buffer);

    OK(dtoa(2.305843009213694E40, buffer, sizeof(buffer)) == 21);
    EXPECT_EQ_STR("2.305843009213694e+40", buffer);

    OK(dtoa(1E0, buffer, sizeof(buffer)) == 1);
    EXPECT_EQ_STR("1", buffer);

    OK(dtoa(1.2E0, buffer, sizeof(buffer)) == 3);
    EXPECT_EQ_STR("1.2", buffer);

    OK(dtoa(1.23E0, buffer, sizeof(buffer)) == 4);
    EXPECT_EQ_STR("1.23", buffer);

    OK(dtoa(1.234E0, buffer, sizeof(buffer)) == 5);
    EXPECT_EQ_STR("1.234", buffer);

    OK(dtoa(1.2345E0, buffer, sizeof(buffer)) == 6);
    EXPECT_EQ_STR("1.2345", buffer);

    OK(dtoa(1.23456E0, buffer, sizeof(buffer)) == 7);
    EXPECT_EQ_STR("1.23456", buffer);

    OK(dtoa(1.234567E0, buffer, sizeof(buffer)) == 8);
    EXPECT_EQ_STR("1.234567", buffer);

    OK(dtoa(1.2345678E0, buffer, sizeof(buffer)) == 9);
    EXPECT_EQ_STR("1.2345678", buffer);

    OK(dtoa(1.23456789E0, buffer, sizeof(buffer)) == 10);
    EXPECT_EQ_STR("1.23456789", buffer);

    OK(dtoa(1.234567895E0, buffer, sizeof(buffer)) == 11);
    EXPECT_EQ_STR("1.234567895", buffer);

    OK(dtoa(1.2345678901E0, buffer, sizeof(buffer)) == 12);
    EXPECT_EQ_STR("1.2345678901", buffer);

    OK(dtoa(1.23456789012E0, buffer, sizeof(buffer)) == 13);
    EXPECT_EQ_STR("1.23456789012", buffer);

    OK(dtoa(1.234567890123E0, buffer, sizeof(buffer)) == 14);
    EXPECT_EQ_STR("1.234567890123", buffer);

    OK(dtoa(1.2345678901234E0, buffer, sizeof(buffer)) == 15);
    EXPECT_EQ_STR("1.2345678901234", buffer);

    OK(dtoa(1.23456789012345E0, buffer, sizeof(buffer)) == 16);
    EXPECT_EQ_STR("1.23456789012345", buffer);

    OK(dtoa(1.234567890123456E0, buffer, sizeof(buffer)) == 17);
    EXPECT_EQ_STR("1.234567890123456", buffer);

    OK(dtoa(1.2345678901234567E0, buffer, sizeof(buffer)) == 18);
    EXPECT_EQ_STR("1.2345678901234567", buffer);

    OK(dtoa(4.294967294E0, buffer, sizeof(buffer)) == 11);
    EXPECT_EQ_STR("4.294967294", buffer);

    OK(dtoa(4.294967295E0, buffer, sizeof(buffer)) == 11);
    EXPECT_EQ_STR("4.294967295", buffer);

    OK(dtoa(4.294967296E0, buffer, sizeof(buffer)) == 11);
    EXPECT_EQ_STR("4.294967296", buffer);

    OK(dtoa(4.294967297E0, buffer, sizeof(buffer)) == 11);
    EXPECT_EQ_STR("4.294967297", buffer);

    OK(dtoa(4.294967298E0, buffer, sizeof(buffer)) == 11);
    EXPECT_EQ_STR("4.294967298", buffer);

    OK(dtoa(1.7800590868057611E-307, buffer, sizeof(buffer)) == 23);
    EXPECT_EQ_STR("1.7800590868057611e-307", buffer);

    OK(dtoa(2.8480945388892175E-306, buffer, sizeof(buffer)) == 23);
    EXPECT_EQ_STR("2.8480945388892175e-306", buffer);

    OK(dtoa(2.446494580089078E-296, buffer, sizeof(buffer)) == 22);
    EXPECT_EQ_STR("2.446494580089078e-296", buffer);

    OK(dtoa(4.8929891601781557E-296, buffer, sizeof(buffer)) == 23);
    EXPECT_EQ_STR("4.8929891601781557e-296", buffer);

    OK(dtoa(1.8014398509481984E16, buffer, sizeof(buffer)) == 22);
    EXPECT_EQ_STR("1.8014398509481984e+16", buffer);

    OK(dtoa(3.6028797018963964E16, buffer, sizeof(buffer)) == 22);
    EXPECT_EQ_STR("3.6028797018963964e+16", buffer);

    OK(dtoa(2.900835519859558E-216, buffer, sizeof(buffer)) == 22);
    EXPECT_EQ_STR("2.900835519859558e-216", buffer);

    OK(dtoa(5.801671039719115E-216, buffer, sizeof(buffer)) == 22);
    EXPECT_EQ_STR("5.801671039719115e-216", buffer);

    OK(dtoa(3.196104012172126E-27, buffer, sizeof(buffer)) == 21);
    EXPECT_EQ_STR("3.196104012172126e-27", buffer);

    return 0;
}

int main(void)
{
    RUN_TEST(dtoa);

    END_TEST;
}
