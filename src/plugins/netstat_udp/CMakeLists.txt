plugin_option(PLUGIN_NETSTAT_UDP "UDP network statistics" ON)

set(BUILD_PLUGIN_NETSTAT_UDP 0)

if(PLUGIN_NETSTAT_UDP)
    if(BUILD_NETBSD)
        set(BUILD_PLUGIN_NETSTAT_UDP 1)
    else()
        set(BUILD_PLUGIN_NETSTAT_UDP_REASON "NetBSD only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_NETSTAT_UDP ${BUILD_PLUGIN_NETSTAT_UDP} PARENT_SCOPE)

if(BUILD_PLUGIN_NETSTAT_UDP)
    set(PLUGIN_NETSTAT_UDP_SRC netstat_udp.c)
    add_library(netstat_udp MODULE ${PLUGIN_NETSTAT_UDP_SRC})
    target_link_libraries(netstat_udp PRIVATE libutils libmetric)
    set_target_properties(netstat_udp PROPERTIES PREFIX "")

#    add_executable(test_plugin_netstat_udp EXCLUDE_FROM_ALL netstat_udp_test.c ${PLUGIN_NETSTAT_UDP_SRC})
#    target_link_libraries(test_plugin_netstat_udp libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_netstat_udp)
#    add_test(NAME test_plugin_netstat_udp COMMAND test_plugin_netstat_udp WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS netstat_udp DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-netstat_udp.5 ncollectd-netstat_udp.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-netstat_udp.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
