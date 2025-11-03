plugin_option(PLUGIN_NOTIFY_SNMP "SNMP Trap notifier" ON)

set(BUILD_PLUGIN_NOTIFY_SNMP 0)

if(PLUGIN_NOTIFY_SNMP)
    find_package(LibNetsnmp)
    if(LIBNETSNMP_FOUND)
        set(BUILD_PLUGIN_NOTIFY_SNMP 1)
    else()
        set(BUILD_PLUGIN_NOTIFY_SNMP_REASON "libnetsnmp not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_NOTIFY_SNMP ${BUILD_PLUGIN_NOTIFY_SNMP} PARENT_SCOPE)

if(BUILD_PLUGIN_NOTIFY_SNMP)
    set(PLUGIN_NOTIFY_SNMP_SRC notify_snmp.c)
    add_library(notify_snmp MODULE ${PLUGIN_NOTIFY_SNMP_SRC})
    target_link_libraries(notify_snmp PRIVATE libmetric libutils LibNetsnmp::LibNetsnmp)
    set_target_properties(notify_snmp PROPERTIES PREFIX "")

#    add_executable(test_plugin_notify_snmp EXCLUDE_FROM_ALL snmp_test.c ${PLUGIN_NOTIFY_SNMP_SRC})
#    target_link_libraries(test_plugin_notify_snmp libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_snmp)
#    add_test(NAME test_plugin_notify_snmp COMMAND test_plugin_snmp WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS notify_snmp DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-notify_snmp.5 ncollectd-notify_snmp.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-notify_snmp.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
