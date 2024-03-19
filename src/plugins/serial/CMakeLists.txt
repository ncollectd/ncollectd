plugin_option(PLUGIN_SERIAL "serial port traffic statistics" ON)

set(BUILD_PLUGIN_SERIAL 0)

if(PLUGIN_SERIAL)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_SERIAL 1)
    else()
        set(BUILD_PLUGIN_SERIAL_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_SERIAL ${BUILD_PLUGIN_SERIAL} PARENT_SCOPE)

if(BUILD_PLUGIN_SERIAL)
    set(PLUGIN_SERIAL_SRC serial.c)
    add_library(serial MODULE ${PLUGIN_SERIAL_SRC})
    target_link_libraries(serial PRIVATE libutils libmetric)
    set_target_properties(serial PROPERTIES PREFIX "")

    add_executable(test_plugin_serial EXCLUDE_FROM_ALL serial_test.c ${PLUGIN_SERIAL_SRC})
    target_link_libraries(test_plugin_serial libtest libutils libmetric)
    add_dependencies(build_tests test_plugin_serial)
    add_test(NAME test_plugin_serial COMMAND test_plugin_serial WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS serial DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-serial.5 ncollectd-serial.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-serial.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
