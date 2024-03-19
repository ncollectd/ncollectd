plugin_option(PLUGIN_LOG_FILE "File logging plugin" ON)

set(BUILD_PLUGIN_LOG_FILE 0)

if(PLUGIN_LOG_FILE)
    set(BUILD_PLUGIN_LOG_FILE 1)
endif()

set(BUILD_PLUGIN_LOG_FILE ${BUILD_PLUGIN_LOG_FILE} PARENT_SCOPE)

if(BUILD_PLUGIN_LOG_FILE)
    set(PLUGIN_LOG_FILE_SRC log_file.c)
    add_library(log_file MODULE ${PLUGIN_LOG_FILE_SRC})
#    target_link_options(load PRIVATE -O2)
    target_link_libraries(log_file PRIVATE libutils libmetric libxson libformat)
#    target_link_options(log_file PRIVATE "-Wl,--version-script=${CMAKE_SOURCE_DIR}/src/plugins.script")
#    set_target_properties(log_file PROPERTIES LINK_DEPENDS "${CMAKE_SOURCE_DIR}/src/plugins.script")
    set_target_properties(log_file PROPERTIES PREFIX "")

    install(TARGETS log_file DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-log_file.5 ncollectd-log_file.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-log_file.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
