plugin_option(PLUGIN_STATSD "StatsD plugin" ON)

set(BUILD_PLUGIN_STATSD 0)

if(PLUGIN_STATSD)
    set(BUILD_PLUGIN_STATSD 1)
endif()

set(BUILD_PLUGIN_STATSD ${BUILD_PLUGIN_STATSD} PARENT_SCOPE)

if(BUILD_PLUGIN_STATSD)
    set(PLUGIN_STATSD_SRC statsd.c)
    add_library(statsd MODULE ${PLUGIN_STATSD_SRC})
    target_link_libraries(statsd PRIVATE libutils libmetric)
    set_target_properties(statsd PROPERTIES PREFIX "")

#    add_executable(test_plugin_statsd EXCLUDE_FROM_ALL statsd_test.c ${PLUGIN_STATSD_SRC})
#    target_link_libraries(test_plugin_statsd libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_statsd)
#    add_test(NAME test_plugin_statsd COMMAND test_plugin_statsd WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS statsd DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-statsd.5 ncollectd-statsd.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-statsd.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
