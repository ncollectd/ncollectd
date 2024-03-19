plugin_option(PLUGIN_DRBD "DRBD statistics" ON)

set(BUILD_PLUGIN_DRBD 0)

if(PLUGIN_DRBD)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_DRBD 1)
    else()
        set(BUILD_PLUGIN_DRBD_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_DRBD ${BUILD_PLUGIN_DRBD} PARENT_SCOPE)

if(BUILD_PLUGIN_DRBD)
    set(PLUGIN_DRBD_SRC drbd.c)
    add_library(drbd MODULE ${PLUGIN_DRBD_SRC})
    target_link_libraries(drbd PRIVATE libutils libmetric)
    set_target_properties(drbd PROPERTIES PREFIX "")

    add_executable(test_plugin_drbd EXCLUDE_FROM_ALL drbd_test.c ${PLUGIN_DRBD_SRC})
    target_link_libraries(test_plugin_drbd libtest libutils libmetric -lm)
    add_dependencies(build_tests test_plugin_drbd)
    add_test(NAME test_plugin_drbd COMMAND test_plugin_drbd WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS drbd DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-drbd.5 ncollectd-drbd.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-drbd.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
