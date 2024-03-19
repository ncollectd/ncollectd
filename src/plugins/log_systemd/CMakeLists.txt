plugin_option(PLUGIN_LOG_SYSTEMD "Systemd logging plugin" ON)

set(BUILD_PLUGIN_LOG_SYSTEMD 0)

if(PLUGIN_LOG_SYSTEMD)
    find_package(LibSystemd)
    if (LIBSYSTEMD_FOUND)
        set(BUILD_PLUGIN_LOG_SYSTEMD 1)
    else()
        set(BUILD_PLUGIN_LOG_SYSTEMD_REASON "libsystemd not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_LOG_SYSTEMD ${BUILD_PLUGIN_LOG_SYSTEMD} PARENT_SCOPE)

if(BUILD_PLUGIN_LOG_SYSTEMD)
    set(PLUGIN_LOG_SYSTEMD_SRC log_systemd.c)
    add_library(log_systemd MODULE ${PLUGIN_LOG_SYSTEMD_SRC})
    target_link_libraries(log_systemd PRIVATE libutils libmetric LibSystemd::LibSystemd)
    set_target_properties(log_systemd PROPERTIES PREFIX "")
    install(TARGETS log_systemd DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-log_systemd.5 ncollectd-log_systemd.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-log_systemd.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
