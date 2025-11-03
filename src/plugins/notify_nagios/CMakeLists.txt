plugin_option(PLUGIN_NOTIFY_NAGIOS "Nagios notification plugin" ON)

set(BUILD_PLUGIN_NOTIFY_NAGIOS 0)

if(PLUGIN_NOTIFY_NAGIOS)
    set(BUILD_PLUGIN_NOTIFY_NAGIOS 1)
endif()

set(BUILD_PLUGIN_NOTIFY_NAGIOS ${BUILD_PLUGIN_NOTIFY_NAGIOS} PARENT_SCOPE)

if(BUILD_PLUGIN_NOTIFY_NAGIOS)
    set(PLUGIN_NOTIFY_NAGIOS_SRC notify_nagios.c)
    add_library(notify_nagios MODULE ${PLUGIN_NOTIFY_NAGIOS_SRC})
    target_link_libraries(notify_nagios PRIVATE libmetric libutils)
    set_target_properties(notify_nagios PROPERTIES PREFIX "")

#    add_executable(test_plugin_notify_nagios EXCLUDE_FROM_ALL notify_nagios_test.c ${PLUGIN_NOTIFY_NAGIOS_SRC})
#    target_link_libraries(test_plugin_notify_nagios libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_notify_nagios)
#    add_test(NAME test_plugin_notify_nagios COMMAND test_plugin_notify_nagios WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS notify_nagios DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-notify_nagios.5 ncollectd-notify_nagios.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-notify_nagios.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
