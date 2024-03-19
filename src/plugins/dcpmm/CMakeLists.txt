plugin_option(PLUGIN_DCPMM "Intel(R) Optane(TM) DC Persistent Memory performance and health statistics" ON)

set(BUILD_PLUGIN_DCPMM 0)

if(PLUGIN_DCPMM)
    find_package(LibIpmctl)
    if (LIBIPMCTL_FOUND)
        set(BUILD_PLUGIN_DCPMM 1)
    else()
        set(BUILD_PLUGIN_DCPMM_REASON "libipmctl not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_DCPMM ${BUILD_PLUGIN_DCPMM} PARENT_SCOPE)

if(BUILD_PLUGIN_DCPMM)
    set(PLUGIN_DCPMM_SRC dcpmm.c)
    add_library(dcpmm MODULE ${PLUGIN_DCPMM_SRC})
    target_link_libraries(dcpmm PRIVATE libutils libmetric LibIpmctl::LibIpmctl)
    set_target_properties(dcpmm PROPERTIES PREFIX "")
    install(TARGETS dcpmm DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-dcpmm.5 ncollectd-dcpmm.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-dcpmm.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
