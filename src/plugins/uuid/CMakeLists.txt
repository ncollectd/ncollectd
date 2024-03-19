plugin_option(PLUGIN_UUID "UUID as hostname plugin" ON)

set(BUILD_PLUGIN_UUID 0)

if(PLUGIN_UUID)
    set(BUILD_PLUGIN_UUID 1)
endif()

set(BUILD_PLUGIN_UUID ${BUILD_PLUGIN_UUID} PARENT_SCOPE)

if(BUILD_PLUGIN_UUID)
    set(PLUGIN_UUID_SRC uuid.c)
    add_library(uuid MODULE ${PLUGIN_UUID_SRC})
    target_link_libraries(uuid PRIVATE libutils libmetric)
    set_target_properties(uuid PROPERTIES PREFIX "")

#    add_executable(test_plugin_uuid EXCLUDE_FROM_ALL uuid_test.c ${PLUGIN_UUID_SRC})
#    target_link_libraries(test_plugin_uuid libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_uuid)
#    add_test(NAME test_plugin_uuid COMMAND test_plugin_uuid WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS uuid DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-uuid.5 ncollectd-uuid.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-uuid.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
