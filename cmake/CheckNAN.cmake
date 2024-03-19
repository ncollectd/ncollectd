include(CheckCSourceRuns)

function(check_nan)
# whether NAN is defined by default
    check_c_source_runs( "
#include <stdlib.h>
#include <math.h>
static double foo = NAN;
int main(void)
{
    if (isnan (foo))
        return 0;
    return 1;
}
" NAN_STATIC_DEFAULT)

    if(NAN_STATIC_DEFAULT)
        set(NAN_STATIC_DEFAULT 1 PARENT_SCOPE)
        return()
    endif()
# whether NAN is defined by __USE_ISOC99
    check_c_source_runs( "
#include <stdlib.h>
#define __USE_ISOC99 1
#include <math.h>
static double foo = NAN;
int main(void)
{
    if (isnan (foo))
        return 0;
    return 1;
} " NAN_STATIC_ISOC)

    if(NAN_STATIC_ISOC)
        set(NAN_STATIC_ISOC 1 PARENT_SCOPE)
        return()
    endif()
# whether NAN can be defined by 0/0
    check_c_source_runs( "
#include <stdlib.h>
#include <math.h>
#ifdef NAN
# undef NAN
#endif
#define NAN (0.0 / 0.0)
#ifndef isnan
# define isnan(f) ((f) != (f))
#endif
static double foo = NAN;
int main(void)
{
    if (isnan (foo))
        return 0;
    return 1;
} " NAN_ZERO_ZERO)
    if(NAN_ZERO_ZERO)
        set(NAN_ZERO_ZERO 1 PARENT_SCOPE)
        return()
    endif()

    message(FATAL_ERROR "Didn't find out how to statically initialize variables to NAN.")
endfunction()
