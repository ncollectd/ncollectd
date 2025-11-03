plugin_option(PLUGIN_TURBOSTAT "Advanced statistic on Intel cpu states" ON)

include(CheckIncludeFile)

set(BUILD_PLUGIN_TURBOSTAT 0)

if(PLUGIN_TURBOSTAT)
    if(BUILD_LINUX)
        check_include_file(cpuid.h HAVE_CPUID_H)
        if(HAVE_CPUID_H)
            set(BUILD_PLUGIN_TURBOSTAT 1)
        else()
            set(BUILD_PLUGIN_TURBOSTAT_REASON "cpuid.h not found" PARENT_SCOPE)
        endif()
    else()
        set(BUILD_PLUGIN_TURBOSTAT_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_TURBOSTAT ${BUILD_PLUGIN_TURBOSTAT} PARENT_SCOPE)

if(BUILD_PLUGIN_TURBOSTAT)
    set(PLUGIN_TURBOSTAT_SRC turbostat.c msr-index.h)
    add_library(turbostat MODULE ${PLUGIN_TURBOSTAT_SRC})
    target_link_libraries(turbostat PRIVATE libmetric libutils)
    set_target_properties(turbostat PROPERTIES PREFIX "")

#    add_executable(test_plugin_turbostat EXCLUDE_FROM_ALL turbostat_test.c ${PLUGIN_TURBOSTAT_SRC})
#    target_link_libraries(test_plugin_turbostat libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_turbostat)
#    add_test(NAME test_plugin_turbostat COMMAND test_plugin_turbostat WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS turbostat DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-turbostat.5 ncollectd-turbostat.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-turbostat.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
