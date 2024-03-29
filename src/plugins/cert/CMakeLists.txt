plugin_option(PLUGIN_CERT "certificate metrics" ON)

set(BUILD_PLUGIN_CERT 0)

if(PLUGIN_CERT)
    find_package(LibSsl)
    if (LIBSSL_FOUND)
        if (LIBSSL_VERSION VERSION_GREATER_EQUAL "1.1.1")
            set(BUILD_PLUGIN_CERT 1)
        else()
            set(BUILD_PLUGIN_CERT_REASON "libssl less than 1.1.1" PARENT_SCOPE)
        endif()
    else()
        set(BUILD_PLUGIN_CERT_REASON "libssl not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_CERT ${BUILD_PLUGIN_CERT} PARENT_SCOPE)

if(BUILD_PLUGIN_CERT)
    set(PLUGIN_CERT_SRC cert.c)
    add_library(cert MODULE ${PLUGIN_CERT_SRC})
    target_link_libraries(cert PRIVATE libutils libmetric LibSsl::LibSsl)
    set_target_properties(cert PROPERTIES PREFIX "")
    install(TARGETS cert DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-cert.5 ncollectd-cert.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-cert.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
