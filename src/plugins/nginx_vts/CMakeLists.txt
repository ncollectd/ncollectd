plugin_option(PLUGIN_NGINX_VTS "Nginx virtual host traffic status module statistics" ON)

set(BUILD_PLUGIN_NGINX_VTS 0)

if(PLUGIN_NGINX_VTS)
    find_package(LibCurl)
    if(LIBCURL_FOUND)
        set(BUILD_PLUGIN_NGINX_VTS 1)
    else()
        set(BUILD_PLUGIN_NGINX_VTS_REASON "libcurl not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_NGINX_VTS ${BUILD_PLUGIN_NGINX_VTS} PARENT_SCOPE)

if(BUILD_PLUGIN_NGINX_VTS)
    set(PLUGIN_NGINX_VTS_SRC nginx_vts.c)
    add_library(nginx_vts MODULE ${PLUGIN_NGINX_VTS_SRC})
    target_link_libraries(nginx_vts PRIVATE libmetric libutils libxson LibCurl::LibCurl)
    set_target_properties(nginx_vts PROPERTIES PREFIX "")
    install(TARGETS nginx_vts DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-nginx_vts.5 ncollectd-nginx_vts.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-nginx_vts.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
