plugin_option(PLUGIN_CPUSLEEP "CPU sleep statistics" ON)

include(CheckCSourceCompiles)

set(BUILD_PLUGIN_CPUSLEEP 0)

if(PLUGIN_CPUSLEEP)
    if(BUILD_LINUX)
        check_c_source_compiles("
#include <time.h>
int main(void)
{
    struct timespec b, m;
    clock_gettime(CLOCK_BOOTTIME, &b );
    clock_gettime(CLOCK_MONOTONIC, &m );
    return 0;
}" HAVE_CLOCK_BOOTTIME_AND_MONOTONIC)
        if(HAVE_CLOCK_BOOTTIME_AND_MONOTONIC)
            set(BUILD_PLUGIN_CPUSLEEP 1)
        else()
            set(BUILD_PLUGIN_CPUSLEEP_REASON "not have clock_gettime with CLOCK_BOOTTIME & CLOCK_MONOTONIC" PARENT_SCOPE)
        endif()
    else()
        set(BUILD_PLUGIN_CPUSLEEP_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_CPUSLEEP ${BUILD_PLUGIN_CPUSLEEP} PARENT_SCOPE)

if(BUILD_PLUGIN_CPUSLEEP)
    set(PLUGIN_CPUSLEEP_SRC cpusleep.c)
    add_library(cpusleep MODULE ${PLUGIN_CPUSLEEP_SRC})
    target_link_libraries(cpusleep PRIVATE libutils libmetric)
    set_target_properties(cpusleep PROPERTIES PREFIX "")

#    add_executable(test_plugin_cpusleep EXCLUDE_FROM_ALL cpusleep_test.c ${PLUGIN_CPUSLEEP_SRC})
#    target_link_libraries(test_plugin_cpusleep libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_cpusleep)
#    add_test(NAME test_plugin_cpusleep COMMAND test_plugin_cpusleep WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS cpusleep DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-cpusleep.5 ncollectd-cpusleep.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-cpusleep.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
