plugin_option(PLUGIN_PROTOCOLS "Protocol (IP, TCP, ...) statistics" ON)

set(BUILD_PLUGIN_PROTOCOLS 0)

if(PLUGIN_PROTOCOLS)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_PROTOCOLS 1)
    else()
        set(BUILD_PLUGIN_PROTOCOLS_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_PROTOCOLS ${BUILD_PLUGIN_PROTOCOLS} PARENT_SCOPE)

if(BUILD_PLUGIN_PROTOCOLS)
    gperf_generate(netstat.gperf "${CMAKE_CURRENT_BINARY_DIR}/netstat.c" NETSTAT_C)
    gperf_generate(snmp.gperf "${CMAKE_CURRENT_BINARY_DIR}/snmp.c" SNMP_C)
    gperf_generate(snmp6.gperf "${CMAKE_CURRENT_BINARY_DIR}/snmp6.c" SNMP6_C)
    gperf_generate(sctp.gperf "${CMAKE_CURRENT_BINARY_DIR}/sctp.c" SCTP_C)
    set(PLUGIN_PROTOCOLS_SRC protocols.c flags.h
                             netstat_fam.c netstat_fam.h
                             snmp_fam.c snmp_fam.h
                             snmp6_fam.c snmp6_fam.h
                             sctp_fam.c sctp_fam.h)
    add_library(protocols MODULE ${PLUGIN_PROTOCOLS_SRC} ${NETSTAT_C} ${SNMP_C} ${SNMP6_C} ${SCTP_C})
    target_link_libraries(protocols PRIVATE libutils libmetric)
    set_target_properties(protocols PROPERTIES PREFIX "")

#    add_executable(test_plugin_protocols EXCLUDE_FROM_ALL protocols_test.c ${PLUGIN_PROTOCOLS_SRC})
#    target_link_libraries(test_plugin_protocols libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_protocols)
#    add_test(NAME test_plugin_protocols COMMAND test_plugin_protocols WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS protocols DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-protocols.5 ncollectd-protocols.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-protocols.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
