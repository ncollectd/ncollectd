plugin_option(PLUGIN_PERL "Embed a Perl interpreter" ON)

include(CheckCSourceRuns)
include(CMakePushCheckState)

set(BUILD_PLUGIN_PERL 0)

if(PLUGIN_PERL)
    find_package(LibPerl)
    if(LIBPERL_FOUND)
        cmake_push_check_state(RESET)
        set(CMAKE_REQUIRED_INCLUDES ${LIBPERL_INCLUDE_PATH})
        set(CMAKE_REQUIRED_LIBRARIES ${LIBPERL_LIBRARY})
        check_c_source_runs("
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
int main(void) {
#if defined(USE_ITHREADS)
    return 0;
#else
    return 1;
#endif
}" HAVE_PERL_ITHREADS)
        cmake_pop_check_state()
        if(HAVE_PERL_ITHREADS)
            set(BUILD_PLUGIN_PERL 1)
        else()
            set(BUILD_PLUGIN_PERL_REASON "perl does not have ithreads" PARENT_SCOPE)
        endif()
    else()
        set(BUILD_PLUGIN_PERL_REASON "libperl not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_PERL ${BUILD_PLUGIN_PERL} PARENT_SCOPE)

if(BUILD_PLUGIN_PERL)
    set(PLUGIN_PERL_SRC perl.c)
    add_library(perl MODULE ${PLUGIN_PERL_SRC})
    target_link_libraries(perl PRIVATE libmetric libutils LibPerl::LibPerl)
    set_target_properties(perl PROPERTIES PREFIX "")

    add_executable(test_plugin_perl EXCLUDE_FROM_ALL perl_test.c ${PLUGIN_PERL_SRC})
    target_link_libraries(test_plugin_perl libtest libmetric libutils LibPerl::LibPerl Threads::Threads)
    add_dependencies(build_tests test_plugin_perl)
    add_test(NAME test_plugin_perl COMMAND test_plugin_perl WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS perl DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-perl.5 ncollectd-perl.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-perl.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
