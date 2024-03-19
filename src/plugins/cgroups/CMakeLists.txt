plugin_option(PLUGIN_CGROUPS "CGroups usage accounting" ON)

include(CheckIncludeFile)

set(BUILD_PLUGIN_CGROUPS 0)

if(PLUGIN_CGROUPS)
    if(BUILD_LINUX)
        check_include_file(linux/config.h HAVE_LINUX_CONFIG_H)
        set(BUILD_PLUGIN_CGROUPS 1)
    else()
        set(BUILD_PLUGIN_CGROUPS_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_CGROUPS ${BUILD_PLUGIN_CGROUPS} PARENT_SCOPE)

if(BUILD_PLUGIN_CGROUPS)
    gperf_generate(memorystat.gperf "${CMAKE_CURRENT_BINARY_DIR}/memorystat.h" MEMORYSTAT_H)
    set(PLUGIN_CGROUPS_SRC cgroups.c)
    add_library(cgroups MODULE ${PLUGIN_CGROUPS_SRC} ${MEMORYSTAT_H})
    target_link_libraries(cgroups PRIVATE libutils libmetric)
    set_target_properties(cgroups PROPERTIES PREFIX "")

    if(HAVE_LINUX_CONFIG_H)
        target_compile_definitions(cgroups PUBLIC HAVE_LINUX_CONFIG_H)
    endif()

#    add_executable(test_plugin_cgroups EXCLUDE_FROM_ALL cgroups_test.c ${PLUGIN_CGROUPS_SRC})
#    target_link_libraries(test_plugin_cgroups libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_cgroups)
#    add_test(NAME test_plugin_cgroups COMMAND test_plugin_cgroups WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS cgroups DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-cgroups.5 ncollectd-cgroups.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-cgroups.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
