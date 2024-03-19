plugin_option(PLUGIN_MOSQUITTO "Mosquitto stastistics" ON)

set(BUILD_PLUGIN_MOSQUITTO 0)

if(PLUGIN_MOSQUITTO)
    find_package(LibMosquitto)
    if (LIBMOSQUITTO_FOUND)
        set(BUILD_PLUGIN_MOSQUITTO 1)
    else()
        set(BUILD_PLUGIN_MOSQUITTO_REASON "libmosquitto not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_MOSQUITTO ${BUILD_PLUGIN_MOSQUITTO} PARENT_SCOPE)

if(BUILD_PLUGIN_MOSQUITTO)
    set(PLUGIN_MOSQUITTO_SRC mosquitto.c)
    add_library(mosquitto MODULE ${PLUGIN_MOSQUITTO_SRC})
    target_link_libraries(mosquitto PRIVATE libutils libmetric LibMosquitto::LibMosquitto)
    set_target_properties(mosquitto PROPERTIES PREFIX "")
    install(TARGETS mosquitto DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-mosquitto.5 ncollectd-mosquitto.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-mosquitto.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
