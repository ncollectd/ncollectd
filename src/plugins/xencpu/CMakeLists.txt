plugin_option(PLUGIN_XENCPU "Xen Host CPU usage" ON)

set(BUILD_PLUGIN_XENCPU 0)

if(PLUGIN_XENCPU)
    find_package(LibXenctrl)
    if (LIBXENCTRL_FOUND)
        set(BUILD_PLUGIN_XENCPU 1)
    else()
        set(BUILD_PLUGIN_XENCPU_REASON "libxenctl not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_XENCPU ${BUILD_PLUGIN_XENCPU} PARENT_SCOPE)

if(BUILD_PLUGIN_XENCPU)
    set(PLUGIN_XENCPU_SRC xencpu.c)
    add_library(xencpu MODULE ${PLUGIN_XENCPU_SRC})
    target_link_libraries(xencpu PRIVATE libmetric libutils LibXenctrl::LibXenctrl)
    set_target_properties(xencpu PROPERTIES PREFIX "")
    install(TARGETS xencpu DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-xencpu.5 ncollectd-xencpu.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-xencpu.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
