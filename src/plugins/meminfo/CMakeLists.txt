plugin_option(PLUGIN_MEMINFO "System memory statistics" ON)

set(BUILD_PLUGIN_MEMINFO 0)

if(PLUGIN_MEMINFO)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_MEMINFO 1)
    else()
        set(BUILD_PLUGIN_MEMINFO_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_MEMINFO ${BUILD_PLUGIN_MEMINFO} PARENT_SCOPE)

if(BUILD_PLUGIN_MEMINFO)
    gperf_generate(meminfo.gperf "${CMAKE_CURRENT_BINARY_DIR}/meminfo.h" MEMINFO_H)
    set(PLUGIN_MEMINFO_SRC meminfo.c)
    add_library(meminfo MODULE ${PLUGIN_MEMINFO_SRC} ${MEMINFO_H})
    target_link_libraries(meminfo PRIVATE libutils libmetric)
    set_target_properties(meminfo PROPERTIES PREFIX "")

    add_executable(test_plugin_meminfo EXCLUDE_FROM_ALL meminfo_test.c ${PLUGIN_MEMINFO_SRC})
    target_link_libraries(test_plugin_meminfo libtest libutils libmetric -lm)
    add_dependencies(build_tests test_plugin_meminfo)
    add_test(NAME test_plugin_meminfo COMMAND test_plugin_meminfo WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS meminfo DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-meminfo.5 ncollectd-meminfo.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-meminfo.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
