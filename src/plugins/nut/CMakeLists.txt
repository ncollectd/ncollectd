plugin_option(PLUGIN_NUT "Network UPS tools statistics" ON)

set(BUILD_PLUGIN_NUT 0)

if(PLUGIN_NUT)
    find_package(LibUpsclient)
    if (LIBUPSCLIENT_FOUND)
        set(BUILD_PLUGIN_NUT 1)
    else()
        set(BUILD_PLUGIN_NUT_REASON "libupsclient not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_NUT ${BUILD_PLUGIN_NUT} PARENT_SCOPE)

if(BUILD_PLUGIN_NUT)
    set(PLUGIN_NUT_SRC nut.c)
    add_library(nut MODULE ${PLUGIN_NUT_SRC})
    target_link_libraries(nut PRIVATE libutils libmetric LibUpsclient::LibUpsclient)
    set_target_properties(nut PROPERTIES PREFIX "")
    install(TARGETS nut DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-nut.5 ncollectd-nut.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-nut.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
