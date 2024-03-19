plugin_option(PLUGIN_WRITE_EXPORTER "Exporter write plugin" ON)

set(BUILD_PLUGIN_WRITE_EXPORTER 0)

if(PLUGIN_WRITE_EXPORTER)
    find_package(LibMicrohttpd)
    if (LIBMICROHTTPD_FOUND)
        set(BUILD_PLUGIN_WRITE_EXPORTER 1)
    else()
        set(BUILD_PLUGIN_WRITE_EXPORTER_REASON "libmicrohttpd not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_WRITE_EXPORTER ${BUILD_PLUGIN_WRITE_EXPORTER} PARENT_SCOPE)

if(BUILD_PLUGIN_WRITE_EXPORTER)
    set(PLUGIN_WRITE_EXPORTER_SRC write_exporter.c)
    add_library(write_exporter MODULE ${PLUGIN_WRITE_EXPORTER_SRC})
    target_link_libraries(write_exporter PRIVATE libutils libmetric libxson libformat LibMicrohttpd::LibMicrohttpd)
    set_target_properties(write_exporter PROPERTIES PREFIX "")
    install(TARGETS write_exporter DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-write_exporter.5 ncollectd-write_exporter.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-write_exporter.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
