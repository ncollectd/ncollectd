plugin_option(PLUGIN_HTTP "Generic web statistics" ON)

set(BUILD_PLUGIN_HTTP 0)

if(PLUGIN_HTTP)
    find_package(LibCurl)
    if(LIBCURL_FOUND)
        set(BUILD_PLUGIN_HTTP 1)
    else()
        set(BUILD_PLUGIN_HTTP_REASON "libcurl not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_HTTP ${BUILD_PLUGIN_HTTP} PARENT_SCOPE)

if(BUILD_PLUGIN_HTTP)
    set(PLUGIN_HTTP_SRC http.c curl_stats.c curl_stats.h)
    add_library(http MODULE ${PLUGIN_HTTP_SRC})
    target_link_libraries(http PRIVATE libutils libmetric LibCurl::LibCurl)
    set_target_properties(http PROPERTIES PREFIX "")
    install(TARGETS http DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-http.5 ncollectd-http.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-http.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
