plugin_option(PLUGIN_SOCKSTAT "Socket cache statistics" ON)

set(BUILD_PLUGIN_SOCKSTAT 0)

if(PLUGIN_SOCKSTAT)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_SOCKSTAT 1)
    else()
        set(BUILD_PLUGIN_SOCKSTAT_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_SOCKSTAT ${BUILD_PLUGIN_SOCKSTAT} PARENT_SCOPE)

if(BUILD_PLUGIN_SOCKSTAT)
    set(PLUGIN_SOCKSTAT_SRC sockstat.c)
    add_library(sockstat MODULE ${PLUGIN_SOCKSTAT_SRC})
    target_link_libraries(sockstat PRIVATE libmetric libutils)
    set_target_properties(sockstat PROPERTIES PREFIX "")

    add_executable(test_plugin_sockstat EXCLUDE_FROM_ALL sockstat_test.c ${PLUGIN_SOCKSTAT_SRC})
    target_link_libraries(test_plugin_sockstat libtest libmetric libutils -lm)
    add_dependencies(build_tests test_plugin_sockstat)
    add_test(NAME test_plugin_sockstat COMMAND test_plugin_sockstat WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS sockstat DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-sockstat.5 ncollectd-sockstat.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-sockstat.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
