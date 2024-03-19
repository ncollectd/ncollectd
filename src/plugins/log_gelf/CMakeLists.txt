plugin_option(PLUGIN_LOG_GELF "GELF logging plugin" ON)

set(BUILD_PLUGIN_LOG_GELF 0)

if(PLUGIN_LOG_GELF)
    set(BUILD_PLUGIN_LOG_GELF 1)
endif()

set(BUILD_PLUGIN_LOG_GELF ${BUILD_PLUGIN_LOG_GELF} PARENT_SCOPE)

if(BUILD_PLUGIN_LOG_GELF)
    set(PLUGIN_LOG_GELF_SRC log_gelf.c)
    add_library(log_gelf MODULE ${PLUGIN_LOG_GELF_SRC})
    target_link_libraries(log_gelf PRIVATE libutils libxson libcompress)
    set_target_properties(log_gelf PROPERTIES PREFIX "")
    install(TARGETS log_gelf DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-log_gelf.5 ncollectd-log_gelf.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-log_gelf.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
