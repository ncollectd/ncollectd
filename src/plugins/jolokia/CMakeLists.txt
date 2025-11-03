plugin_option(PLUGIN_JOLOKIA "Nginx statistics" ON)

set(BUILD_PLUGIN_JOLOKIA 0)

if(PLUGIN_JOLOKIA)
    find_package(LibCurl)
    if(LIBCURL_FOUND)
        set(BUILD_PLUGIN_JOLOKIA 1)
    else()
        set(BUILD_PLUGIN_JOLOKIA_REASON "libcurl not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_JOLOKIA ${BUILD_PLUGIN_JOLOKIA} PARENT_SCOPE)

if(BUILD_PLUGIN_JOLOKIA)
    set(PLUGIN_JOLOKIA_SRC jolokia.c)
    add_library(jolokia MODULE ${PLUGIN_JOLOKIA_SRC})
    target_link_libraries(jolokia PRIVATE libmetric libutils libxson LibCurl::LibCurl)
    set_target_properties(jolokia PROPERTIES PREFIX "")
    install(TARGETS jolokia DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-jolokia.5 ncollectd-jolokia.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-jolokia.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
