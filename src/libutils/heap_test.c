// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2013 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

#include "ncollectd.h"
#include "libtest/testing.h"
#include "libutils/heap.h"

static int compare(void const *v0, void const *v1)
{
    int const *i0 = v0;
    int const *i1 = v1;

    if ((*i0) < (*i1))
        return -1;
    else if ((*i0) > (*i1))
        return 1;
    else
        return 0;
}

DEF_TEST(simple)
{
    int values[] = {9, 5, 6, 1, 3, 4, 0, 8, 2, 7};
    c_heap_t *h;

    CHECK_NOT_NULL(h = c_heap_create(compare));
    for (int i = 0; i < 10; i++)
        CHECK_ZERO(c_heap_insert(h, &values[i]));

    for (int i = 0; i < 5; i++) {
        int *ret = NULL;
        CHECK_NOT_NULL(ret = c_heap_get_root(h));
        OK(*ret == i);
    }

    CHECK_ZERO(c_heap_insert(h, &values[6] /* = 0 */));
    CHECK_ZERO(c_heap_insert(h, &values[3] /* = 1 */));
    CHECK_ZERO(c_heap_insert(h, &values[8] /* = 2 */));
    CHECK_ZERO(c_heap_insert(h, &values[4] /* = 3 */));
    CHECK_ZERO(c_heap_insert(h, &values[5] /* = 4 */));

    for (int i = 0; i < 10; i++) {
        int *ret = NULL;
        CHECK_NOT_NULL(ret = c_heap_get_root(h));
        OK(*ret == i);
    }

    c_heap_destroy(h);
    return 0;
}

int main(void)
{
    RUN_TEST(simple);

    END_TEST;
}
