plugin_option(PLUGIN_EPICS "Redis plugin" ON)

set(BUILD_PLUGIN_EPICS 0)

if(PLUGIN_EPICS)
    find_package(LibEpics)
    if (LIBEPICS_FOUND)
        set(BUILD_PLUGIN_EPICS 1)
    else()
        set(BUILD_PLUGIN_EPICS_REASON "epics not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_EPICS ${BUILD_PLUGIN_EPICS} PARENT_SCOPE)

if(BUILD_PLUGIN_EPICS)
    set(PLUGIN_EPICS_SRC epics.c)
    add_library(epics MODULE ${PLUGIN_EPICS_SRC})
    target_link_libraries(epics PRIVATE libutils libmetric LibEpics::LibEpics)
    set_target_properties(epics PROPERTIES PREFIX "")
    install(TARGETS epics DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-epics.5 ncollectd-epics.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-epics.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
