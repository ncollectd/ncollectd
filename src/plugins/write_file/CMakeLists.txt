plugin_option(PLUGIN_WRITE_FILE "File output plugin" ON)

set(BUILD_PLUGIN_WRITE_FILE 0)

if(PLUGIN_WRITE_FILE)
    set(BUILD_PLUGIN_WRITE_FILE 1)
endif()

set(BUILD_PLUGIN_WRITE_FILE ${BUILD_PLUGIN_WRITE_FILE} PARENT_SCOPE)

if(BUILD_PLUGIN_WRITE_FILE)
    set(PLUGIN_WRITE_FILE_SRC write_file.c)
    add_library(write_file MODULE ${PLUGIN_WRITE_FILE_SRC})
    target_link_libraries(write_file PRIVATE libmetric libutils libxson libformat)
    set_target_properties(write_file PROPERTIES PREFIX "")

#    add_executable(test_plugin_write_file EXCLUDE_FROM_ALL write_file_test.c ${PLUGIN_WRITE_FILE_SRC})
#    target_link_libraries(test_plugin_write_file libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_write_file)
#    add_test(NAME test_plugin_write_file COMMAND test_plugin_write_file WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS write_file DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-write_file.5 ncollectd-write_file.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-write_file.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
