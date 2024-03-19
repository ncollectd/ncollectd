plugin_option(PLUGIN_WRITE_LOG "Log output plugin" ON)

set(BUILD_PLUGIN_WRITE_LOG 0)

if(PLUGIN_WRITE_LOG)
    set(BUILD_PLUGIN_WRITE_LOG 1)
endif()

set(BUILD_PLUGIN_WRITE_LOG ${BUILD_PLUGIN_WRITE_LOG} PARENT_SCOPE)

if(BUILD_PLUGIN_WRITE_LOG)
    set(PLUGIN_WRITE_LOG_SRC write_log.c)
    add_library(write_log MODULE ${PLUGIN_WRITE_LOG_SRC})
#    target_link_options(load PRIVATE -O2)
    target_link_libraries(write_log PRIVATE libutils libmetric libxson libformat)
#    target_link_options(write_log PRIVATE "-Wl,--version-script=${CMAKE_SOURCE_DIR}/src/plugins.script")
#    set_target_properties(write_log PROPERTIES LINK_DEPENDS "${CMAKE_SOURCE_DIR}/src/plugins.script")
    set_target_properties(write_log PROPERTIES PREFIX "")

    install(TARGETS write_log DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-write_log.5 ncollectd-write_log.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-write_log.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
