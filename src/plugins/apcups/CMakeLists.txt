plugin_option(PLUGIN_APCUPS "Statistics of UPSes by APC" ON)

set(BUILD_PLUGIN_APCUPS 0)

if(PLUGIN_APCUPS)
    set(BUILD_PLUGIN_APCUPS 1)
endif()

set(BUILD_PLUGIN_APCUPS ${BUILD_PLUGIN_APCUPS} PARENT_SCOPE)

if(BUILD_PLUGIN_APCUPS)
    set(PLUGIN_APCUPS_SRC apcups.c)
    set(PLUGIN_APCUPS_LIBS libmetric libutils)

    add_library(apcups MODULE ${PLUGIN_APCUPS_SRC})
    target_link_libraries(apcups PRIVATE ${PLUGIN_APCUPS_LIBS})
    set_target_properties(apcups PROPERTIES PREFIX "")

    install(TARGETS apcups DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-apcups.5 ncollectd-apcups.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-apcups.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
