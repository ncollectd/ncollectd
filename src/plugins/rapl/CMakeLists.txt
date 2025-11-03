plugin_option(PLUGIN_RAPL "Running Average Power Limit (RAPL) metrics" ON)

set(BUILD_PLUGIN_RAPL 0)

if(PLUGIN_RAPL)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_RAPL 1)
    else()
        set(BUILD_PLUGIN_RAPL_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_RAPL ${BUILD_PLUGIN_RAPL} PARENT_SCOPE)

if(BUILD_PLUGIN_RAPL)
    set(PLUGIN_RAPL_SRC rapl.c)
    add_library(rapl MODULE ${PLUGIN_RAPL_SRC})
    target_link_libraries(rapl PRIVATE libmetric libutils)
    set_target_properties(rapl PROPERTIES PREFIX "")

    add_executable(test_plugin_rapl EXCLUDE_FROM_ALL rapl_test.c ${PLUGIN_RAPL_SRC})
    target_link_libraries(test_plugin_rapl libtest libmetric libutils -lm)
    add_dependencies(build_tests test_plugin_rapl)
    add_test(NAME test_plugin_rapl COMMAND test_plugin_rapl WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS rapl DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-rapl.5 ncollectd-rapl.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-rapl.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
