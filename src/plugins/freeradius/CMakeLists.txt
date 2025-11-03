plugin_option(PLUGIN_FREERADIUS "Freeradius statistics" ON)

set(BUILD_PLUGIN_FREERADIUS 0)

if(PLUGIN_FREERADIUS)
    set(BUILD_PLUGIN_FREERADIUS 1)
endif()

set(BUILD_PLUGIN_FREERADIUS ${BUILD_PLUGIN_FREERADIUS} PARENT_SCOPE)

if(BUILD_PLUGIN_FREERADIUS)
    set(PLUGIN_FREERADIUS_SRC freeradius.c hmac.c hmac.h md5.c md5.h)
    set(PLUGIN_FREERADIUS_LIBS libmetric libutils)

    add_library(freeradius MODULE ${PLUGIN_FREERADIUS_SRC})
    target_link_libraries(freeradius PRIVATE ${PLUGIN_FREERADIUS_LIBS})
    set_target_properties(freeradius PROPERTIES PREFIX "")

    install(TARGETS freeradius DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-freeradius.5 ncollectd-freeradius.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-freeradius.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
