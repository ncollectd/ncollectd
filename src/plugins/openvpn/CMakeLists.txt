plugin_option(PLUGIN_OPENVPN "OpenVPN client statistics" ON)

set(BUILD_PLUGIN_OPENVPN 0)

if(PLUGIN_OPENVPN)
    set(BUILD_PLUGIN_OPENVPN 1)
endif()

set(BUILD_PLUGIN_OPENVPN ${BUILD_PLUGIN_OPENVPN} PARENT_SCOPE)

if(BUILD_PLUGIN_OPENVPN)
    set(PLUGIN_OPENVPN_SRC openvpn.c)
    add_library(openvpn MODULE ${PLUGIN_OPENVPN_SRC})
    target_link_libraries(openvpn PRIVATE libutils libmetric)
    set_target_properties(openvpn PROPERTIES PREFIX "")

#    add_executable(test_plugin_openvpn EXCLUDE_FROM_ALL openvpn_test.c ${PLUGIN_OPENVPN_SRC})
#    target_link_libraries(test_plugin_openvpn libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_openvpn)
#    add_test(NAME test_plugin_openvpn COMMAND test_plugin_openvpn WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS openvpn DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-openvpn.5 ncollectd-openvpn.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-openvpn.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
