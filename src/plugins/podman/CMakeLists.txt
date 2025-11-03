plugin_option(PLUGIN_PODMAN "Podman containers statistics" ON)

set(BUILD_PLUGIN_PODMAN 0)

if(PLUGIN_PODMAN)
    find_package(LibCurl)
    if(LIBCURL_FOUND)
        set(BUILD_PLUGIN_PODMAN 1)
    else()
        set(BUILD_PLUGIN_PODMAN_REASON "libcurl not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_PODMAN ${BUILD_PLUGIN_PODMAN} PARENT_SCOPE)

if(BUILD_PLUGIN_PODMAN)
    set(PLUGIN_PODMAN_SRC podman.c)
    add_library(podman MODULE ${PLUGIN_PODMAN_SRC})
    target_link_libraries(podman PRIVATE libmetric libutils libxson LibCurl::LibCurl)
    set_target_properties(podman PROPERTIES PREFIX "")
    install(TARGETS podman DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-podman.5 ncollectd-podman.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-podman.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
