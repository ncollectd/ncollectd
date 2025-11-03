plugin_option(PLUGIN_WRITE_TCP "TCP output plugin" ON)

set(BUILD_PLUGIN_WRITE_TCP 0)

if(PLUGIN_WRITE_TCP)
    set(BUILD_PLUGIN_WRITE_TCP 1)
endif()

set(BUILD_PLUGIN_WRITE_TCP ${BUILD_PLUGIN_WRITE_TCP} PARENT_SCOPE)

if(BUILD_PLUGIN_WRITE_TCP)
    set(PLUGIN_WRITE_TCP_SRC write_tcp.c)
    add_library(write_tcp MODULE ${PLUGIN_WRITE_TCP_SRC})
    target_link_libraries(write_tcp PRIVATE libmetric libutils libxson libformat)
    set_target_properties(write_tcp PROPERTIES PREFIX "")

#    add_executable(test_plugin_write_tcp EXCLUDE_FROM_ALL write_tcp_test.c ${PLUGIN_WRITE_TCP_SRC})
#    target_link_libraries(test_plugin_write_tcp libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_write_tcp)
#    add_test(NAME test_plugin_write_tcp COMMAND test_plugin_write_tcp WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS write_tcp DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-write_tcp.5 ncollectd-write_tcp.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-write_tcp.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
