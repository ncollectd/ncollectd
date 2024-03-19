plugin_option(PLUGIN_ROUTEROS "RouterOS plugin" ON)

set(BUILD_PLUGIN_ROUTEROS 0)

if(PLUGIN_ROUTEROS)
    set(BUILD_PLUGIN_ROUTEROS 1)
endif()

set(BUILD_PLUGIN_ROUTEROS ${BUILD_PLUGIN_ROUTEROS} PARENT_SCOPE)

if(BUILD_PLUGIN_ROUTEROS)
    set(PLUGIN_ROUTEROS_SRC routeros.c
                            ros.c ros_api.h ros_md5.c ros_md5.h
                            ros_parse.c ros_parse.h ros_interface.c
                            ros_registration_table.c ros_system_health.c ros_system_resource.c)
    add_library(routeros MODULE ${PLUGIN_ROUTEROS_SRC})
    target_link_libraries(routeros PRIVATE libutils libmetric)
    set_target_properties(routeros PROPERTIES PREFIX "")

#    add_executable(test_plugin_routeros EXCLUDE_FROM_ALL routeros_test.c ${PLUGIN_ROUTEROS_SRC})
#    target_link_libraries(test_plugin_routeros libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_routeros)
#    add_test(NAME test_plugin_routeros COMMAND test_plugin_routeros WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS routeros DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-routeros.5 ncollectd-routeros.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-routeros.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
