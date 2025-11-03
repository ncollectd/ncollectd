plugin_option(PLUGIN_WRITE_HTTP "Write Http output plugin" ON)

set(BUILD_PLUGIN_WRITE_HTTP 0)

if(PLUGIN_WRITE_HTTP)
    find_package(LibCurl)
    if(LIBCURL_FOUND)
        set(BUILD_PLUGIN_WRITE_HTTP 1)
    else()
        set(BUILD_PLUGIN_WRITE_HTTP_REASON "libcurl not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_WRITE_HTTP ${BUILD_PLUGIN_WRITE_HTTP} PARENT_SCOPE)

if(BUILD_PLUGIN_WRITE_HTTP)
    set(PLUGIN_WRITE_HTTP_SRC write_http.c curl_stats.c curl_stats.h)
    add_library(write_http MODULE ${PLUGIN_WRITE_HTTP_SRC})
    target_link_libraries(write_http PRIVATE libmetric libutils libformat libcompress LibCurl::LibCurl)
    set_target_properties(write_http PROPERTIES PREFIX "")
    install(TARGETS write_http DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-write_http.5 ncollectd-write_http.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-write_http.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
