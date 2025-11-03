plugin_option(PLUGIN_GPS "GPS plugin" ON)

set(BUILD_PLUGIN_GPS 0)

if(PLUGIN_GPS)
    find_package(LibGps)
    if (LIBGPS_FOUND)
        set(BUILD_PLUGIN_GPS 1)
    else()
        set(BUILD_PLUGIN_GPS_REASON "lbgps not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_GPS ${BUILD_PLUGIN_GPS} PARENT_SCOPE)

if(BUILD_PLUGIN_GPS)
    set(PLUGIN_GPS_SRC gps.c)
    add_library(gps MODULE ${PLUGIN_GPS_SRC})
    target_link_libraries(gps PRIVATE libmetric libutils LibGps::LibGps)
    set_target_properties(gps PROPERTIES PREFIX "")
    install(TARGETS gps DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-gps.5 ncollectd-gps.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-gps.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
