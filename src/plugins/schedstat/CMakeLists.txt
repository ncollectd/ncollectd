plugin_option(PLUGIN_SCHEDSTAT "schedstat statistics" ON)

set(BUILD_PLUGIN_SCHEDSTAT 0)

if(PLUGIN_SCHEDSTAT)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_SCHEDSTAT 1)
    else()
        set(BUILD_PLUGIN_SCHEDSTAT_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_SCHEDSTAT ${BUILD_PLUGIN_SCHEDSTAT} PARENT_SCOPE)

if(BUILD_PLUGIN_SCHEDSTAT)
    set(PLUGIN_SCHEDSTAT_SRC schedstat.c)
    add_library(schedstat MODULE ${PLUGIN_SCHEDSTAT_SRC})
    target_link_libraries(schedstat PRIVATE libutils libmetric)
    set_target_properties(schedstat PROPERTIES PREFIX "")

    add_executable(test_plugin_schedstat EXCLUDE_FROM_ALL schedstat_test.c ${PLUGIN_SCHEDSTAT_SRC})
    target_link_libraries(test_plugin_schedstat libtest libutils libmetric -lm)
    add_dependencies(build_tests test_plugin_schedstat)
    add_test(NAME test_plugin_schedstat COMMAND test_plugin_schedstat WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS schedstat DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-schedstat.5 ncollectd-schedstat.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-schedstat.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
