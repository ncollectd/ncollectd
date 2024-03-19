plugin_option(PLUGIN_WRITE_UDP "UDP output plugin" ON)

set(BUILD_PLUGIN_WRITE_UDP 0)

if(PLUGIN_WRITE_UDP)
    set(BUILD_PLUGIN_WRITE_UDP 1)
# if BUILD_WITH_LIBSOCKET
# write_udp_la_LIBADD += -lsocket
endif()

set(BUILD_PLUGIN_WRITE_UDP ${BUILD_PLUGIN_WRITE_UDP} PARENT_SCOPE)

if(BUILD_PLUGIN_WRITE_UDP)
    set(PLUGIN_WRITE_UDP_SRC write_udp.c)
    add_library(write_udp MODULE ${PLUGIN_WRITE_UDP_SRC})
    target_link_libraries(write_udp PRIVATE libutils libmetric libxson libformat)
    set_target_properties(write_udp PROPERTIES PREFIX "")

#    add_executable(test_plugin_write_udp EXCLUDE_FROM_ALL write_udp_test.c ${PLUGIN_WRITE_UDP_SRC})
#    target_link_libraries(test_plugin_write_udp libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_write_udp)
#    add_test(NAME test_plugin_write_udp COMMAND test_plugin_write_udp WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS write_udp DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-write_udp.5 ncollectd-write_udp.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-write_udp.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
