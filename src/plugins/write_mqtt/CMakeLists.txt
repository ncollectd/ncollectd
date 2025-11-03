plugin_option(PLUGIN_WRITE_MQTT "MQTT output plugin" ON)

set(BUILD_PLUGIN_WRITE_MQTT 0)

if(PLUGIN_WRITE_MQTT)
    find_package(LibMosquitto)
    if (LIBMOSQUITTO_FOUND)
        set(BUILD_PLUGIN_WRITE_MQTT 1)
    else()
        set(BUILD_PLUGIN_WRITE_MQTT_REASON "libmosquitto not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_WRITE_MQTT ${BUILD_PLUGIN_WRITE_MQTT} PARENT_SCOPE)

if(BUILD_PLUGIN_WRITE_MQTT)
    set(PLUGIN_WRITE_MQTT_SRC write_mqtt.c)
    add_library(write_mqtt MODULE ${PLUGIN_WRITE_MQTT_SRC})
    target_link_libraries(write_mqtt PRIVATE libmetric libutils libxson libformat LibMosquitto::LibMosquitto)
    set_target_properties(write_mqtt PROPERTIES PREFIX "")
    install(TARGETS write_mqtt DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-write_mqtt.5 ncollectd-write_mqtt.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-write_mqtt.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
