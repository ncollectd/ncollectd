plugin_option(PLUGIN_SNMP "SNMP querying plugin" ON)

set(BUILD_PLUGIN_SNMP 0)

if(PLUGIN_SNMP)
    find_package(LibNetsnmp)
    if(LIBNETSNMP_FOUND)
        set(BUILD_PLUGIN_SNMP 1)
    else()
        set(BUILD_PLUGIN_SNMP_REASON "libnetsnmp not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_SNMP ${BUILD_PLUGIN_SNMP} PARENT_SCOPE)

if(BUILD_PLUGIN_SNMP)
    set(PLUGIN_SNMP_SRC snmp.c)
    add_library(snmp MODULE ${PLUGIN_SNMP_SRC})
    target_link_libraries(snmp PRIVATE libmetric libutils LibNetsnmp::LibNetsnmp)
    set_target_properties(snmp PROPERTIES PREFIX "")

#    add_executable(test_plugin_snmp EXCLUDE_FROM_ALL snmp_test.c ${PLUGIN_SNMP_SRC})
#    target_link_libraries(test_plugin_snmp libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_snmp)
#    add_test(NAME test_plugin_snmp COMMAND test_plugin_snmp WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS snmp DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-snmp.5 ncollectd-snmp.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-snmp.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
