plugin_option(PLUGIN_BIND "ISC Bind nameserver statistics" ON)

set(BUILD_PLUGIN_BIND 0)

if(PLUGIN_BIND)
    find_package(LibCurl)
    find_package(LibXml2)
    if(LIBCURL_FOUND AND LIBXML2_FOUND)
        set(BUILD_PLUGIN_BIND 1)
    else()
        set(BUILD_PLUGIN_BIND_REASON "libcurl not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_BIND ${BUILD_PLUGIN_BIND} PARENT_SCOPE)

if(BUILD_PLUGIN_BIND)
    gperf_generate(bind_fams.gperf "${CMAKE_CURRENT_BINARY_DIR}/bind_fams.c" BIND_FAMS_C)
    set(PLUGIN_BIND_SRC
        bind.c
        bind_fams.h
        bind_xml.c
        bind_xml.h
        bind_json.c
        bind_json.h)
    add_library(bind MODULE ${PLUGIN_BIND_SRC} ${BIND_FAMS_C})
    target_link_libraries(bind PRIVATE libmetric libutils libxson LibCurl::LibCurl LibXml2::LibXml2)
    set_target_properties(bind PROPERTIES PREFIX "")
    install(TARGETS bind DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-bind.5 ncollectd-bind.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-bind.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
