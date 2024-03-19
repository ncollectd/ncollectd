plugin_option(PLUGIN_PING "Network latency statistics" ON)

set(BUILD_PLUGIN_PING 0)

if(PLUGIN_PING)
    set(BUILD_PLUGIN_PING 1)
endif()

set(BUILD_PLUGIN_PING ${BUILD_PLUGIN_PING} PARENT_SCOPE)

if(BUILD_PLUGIN_PING)
    set(PLUGIN_PING_SRC ping.c oping.c oping.h)
    add_library(ping MODULE ${PLUGIN_PING_SRC})
    target_link_libraries(ping PRIVATE libutils libmetric)
    set_target_properties(ping PROPERTIES PREFIX "")

#    add_executable(test_plugin_ping EXCLUDE_FROM_ALL ping_test.c ${PLUGIN_PING_SRC})
#    target_link_libraries(test_plugin_ping libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_ping)
#    add_test(NAME test_plugin_ping COMMAND test_plugin_ping WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS ping DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-ping.5 ncollectd-ping.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-ping.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
