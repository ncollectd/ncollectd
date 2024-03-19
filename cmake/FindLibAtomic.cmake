include(FindPackageHandleStandardArgs)
include(CheckCSourceCompiles)
include(CMakePushCheckState)

check_c_source_compiles("
#include <stdatomic.h>
int main() {
    atomic_ullong counter = 0;
    atomic_fetch_add(&counter, 1);
    return 0;
}" ATOMIC_LIBRARY_NATIVE)

if(ATOMIC_LIBRARY_NATIVE)
    set(LIBATOMIC_FOUND FALSE)
else()
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_LIBRARIES "-latomic")
    check_c_source_compiles("
#include <stdatomic.h>
int main() {
    atomic_ullong counter = 0;
    atomic_fetch_add(&counter, 1);
    return 0;
}" ATOMIC_IN_LIBRARY)
    cmake_pop_check_state()

    if(ATOMIC_IN_LIBRARY)
        set(LIBATOMIC_LIBRARIES atomic)
        find_package_handle_standard_args(LibAtomic DEFAULT_MSG LIBATOMIC_LIBRARIES)
        mark_as_advanced(LIBATOMIC_LIBRARIES)
        if(NOT TARGET LibAtomic::LibAtomic)
            add_library(LibAtomic::LibAtomic INTERFACE IMPORTED)
            set_target_properties(LibAtomic::LibAtomic PROPERTIES
                                  INTERFACE_LINK_LIBRARIES "${LIBATOMIC_LIBRARIES}")
        endif()
    endif()
endif()
