plugin_option(PLUGIN_WRITE_AMQP1 "AMQP 1.0 output plugin" ON)

set(BUILD_PLUGIN_WRITE_AMQP1 0)

if(PLUGIN_WRITE_AMQP1)
    find_package(LibQpidproton)
    if (LIBQPIDPROTON_FOUND)
        set(BUILD_PLUGIN_WRITE_AMQP1 1)
    else()
        set(BUILD_PLUGIN_WRITE_AMQP1_REASON "libqpidproton not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_WRITE_AMQP1 ${BUILD_PLUGIN_WRITE_AMQP1} PARENT_SCOPE)

if(BUILD_PLUGIN_WRITE_AMQP1)
    set(PLUGIN_WRITE_AMQP1_SRC write_amqp1.c)
    add_library(write_amqp1 MODULE ${PLUGIN_WRITE_AMQP1_SRC})
    target_link_libraries(write_amqp1 PRIVATE libmetric libutils libxson libformat LibQpidproton::LibQpidproton)
    set_target_properties(write_amqp1 PROPERTIES PREFIX "")
    install(TARGETS write_amqp1 DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-write_amqp1.5 ncollectd-write_amqp1.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-write_amqp1.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
