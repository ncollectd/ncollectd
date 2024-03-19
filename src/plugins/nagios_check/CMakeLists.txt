plugin_option(PLUGIN_NAGIOS_CHECK "Execution of nagios checks" ON)

set(BUILD_PLUGIN_NAGIOS_CHECK 0)

if(PLUGIN_NAGIOS_CHECK)
    set(BUILD_PLUGIN_NAGIOS_CHECK 1)
endif()

set(BUILD_PLUGIN_NAGIOS_CHECK ${BUILD_PLUGIN_NAGIOS_CHECK} PARENT_SCOPE)

if(BUILD_PLUGIN_NAGIOS_CHECK)
    set(PLUGIN_NAGIOS_CHECK_SRC nagios_check.c)
    add_library(nagios_check MODULE ${PLUGIN_NAGIOS_CHECK_SRC})
    target_link_libraries(nagios_check PRIVATE libutils libmetric)
    set_target_properties(nagios_check PROPERTIES PREFIX "")

#    add_executable(test_plugin_nagios_check EXCLUDE_FROM_ALL nagios_check_test.c ${PLUGIN_NAGIOS_CHECK_SRC})
#    target_link_libraries(test_plugin_nagios_check libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_nagios_check)
#    add_test(NAME test_plugin_nagios_check COMMAND test_plugin_nagios_check WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS nagios_check DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-nagios_check.5 ncollectd-nagios_check.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-nagios_check.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
