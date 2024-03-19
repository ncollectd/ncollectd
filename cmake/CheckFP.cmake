include(CheckCSourceRuns)

function(check_fp)
    # if doubles are stored in x86 representation
    check_c_source_runs("
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
int main(void)
{
    uint64_t i0;
    uint64_t i1;
    uint8_t c[8];
    double d;

    d = 8.642135e130;
    memcpy ((void *) &i0, (void *) &d, 8);

    i1 = i0;
    memcpy ((void *) c, (void *) &i1, 8);

    if ((c[0] == 0x2f) && (c[1] == 0x25)
        && (c[2] == 0xc0) && (c[3] == 0xc7)
        && (c[4] == 0x43) && (c[5] == 0x2b)
        && (c[6] == 0x1f) && (c[7] == 0x5b))
        return 0;
    return 1;
} " FP_LAYOUT_NEED_NOTHING)

    if(FP_LAYOUT_NEED_NOTHING)
        set(FP_LAYOUT_NEED_NOTHING 1 PARENT_SCOPE)
        return()
    endif()
# if endianflip converts to x86 representation
    check_c_source_runs("
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#define endianflip(A) ((((uint64_t)(A) & 0xff00000000000000LL) >> 56) | \
                       (((uint64_t)(A) & 0x00ff000000000000LL) >> 40) | \
                       (((uint64_t)(A) & 0x0000ff0000000000LL) >> 24) | \
                       (((uint64_t)(A) & 0x000000ff00000000LL) >> 8)  | \
                       (((uint64_t)(A) & 0x00000000ff000000LL) << 8)  | \
                       (((uint64_t)(A) & 0x0000000000ff0000LL) << 24) | \
                       (((uint64_t)(A) & 0x000000000000ff00LL) << 40) | \
                       (((uint64_t)(A) & 0x00000000000000ffLL) << 56))
int main(void)
{
    uint64_t i0;
    uint64_t i1;
    uint8_t c[8];
    double d;

    d = 8.642135e130;
    memcpy ((void *) &i0, (void *) &d, 8);

    i1 = endianflip (i0);
    memcpy ((void *) c, (void *) &i1, 8);

    if ((c[0] == 0x2f) && (c[1] == 0x25)
        && (c[2] == 0xc0) && (c[3] == 0xc7)
        && (c[4] == 0x43) && (c[5] == 0x2b)
        && (c[6] == 0x1f) && (c[7] == 0x5b))
        return 0;
    return 1;
} " FP_LAYOUT_NEED_ENDIANFLIP)

    if(FP_LAYOUT_NEED_ENDIANFLIP)
        set(FP_LAYOUT_NEED_ENDIANFLIP 1 PARENT_SCOPE)
        return()
    endif()
# if intswap converts to x86 representation
    check_c_source_runs("
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#define intswap(A)    ((((uint64_t)(A) & 0xffffffff00000000LL) >> 32) | \
                       (((uint64_t)(A) & 0x00000000ffffffffLL) << 32))

int main(void)
{
    uint64_t i0;
    uint64_t i1;
    uint8_t c[8];
    double d;

    d = 8.642135e130;
    memcpy ((void *) &i0, (void *) &d, 8);

    i1 = intswap (i0);
    memcpy ((void *) c, (void *) &i1, 8);

    if ((c[0] == 0x2f) && (c[1] == 0x25)
        && (c[2] == 0xc0) && (c[3] == 0xc7)
        && (c[4] == 0x43) && (c[5] == 0x2b)
        && (c[6] == 0x1f) && (c[7] == 0x5b))
        return 0;
    return 1;
" FP_LAYOUT_NEED_INTSWAP)

    if(FP_LAYOUT_NEED_INTSWAP)
        set(FP_LAYOUT_NEED_INTSWAP 1 PARENT_SCOPE)
        return()
    endif()

    message(FATAL_ERROR "Didn't find out how doubles are stored in memory.")
endfunction()
