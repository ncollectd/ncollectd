plugin_option(PLUGIN_MODBUS "MODBUS plugin" ON)

set(BUILD_PLUGIN_MODBUS 0)

if(PLUGIN_MODBUS)
    find_package(LibModbus)
    if (LIBMODBUS_FOUND)
        set(BUILD_PLUGIN_MODBUS 1)
    else()
        set(BUILD_PLUGIN_MODBUS_REASON "libmodbus not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_MODBUS ${BUILD_PLUGIN_MODBUS} PARENT_SCOPE)

if(BUILD_PLUGIN_MODBUS)
    set(PLUGIN_MODBUS_SRC modbus.c)
    add_library(modbus MODULE ${PLUGIN_MODBUS_SRC})
    target_link_libraries(modbus PRIVATE libutils libmetric LibModbus::LibModbus)
    set_target_properties(modbus PROPERTIES PREFIX "")
    install(TARGETS modbus DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-modbus.5 ncollectd-modbus.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-modbus.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
