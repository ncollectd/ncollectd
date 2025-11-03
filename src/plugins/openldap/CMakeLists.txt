plugin_option(PLUGIN_OPENLDAP "OpenLDAP statistics" ON)

set(BUILD_PLUGIN_OPENLDAP 0)

if(PLUGIN_OPENLDAP)
    find_package(LibLdap)
    if (LIBLDAP_FOUND)
        set(BUILD_PLUGIN_OPENLDAP 1)
    else()
        set(BUILD_PLUGIN_OPENLDAP_REASON "libldap no found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_OPENLDAP ${BUILD_PLUGIN_OPENLDAP} PARENT_SCOPE)

if(BUILD_PLUGIN_OPENLDAP)
    set(PLUGIN_OPENLDAP_SRC openldap.c)
    add_library(openldap MODULE ${PLUGIN_OPENLDAP_SRC})
    target_link_libraries(openldap PRIVATE libmetric libutils LibLdap::LibLdap)
    set_target_properties(openldap PROPERTIES PREFIX "")
    install(TARGETS openldap DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-openldap.5 ncollectd-openldap.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-openldap.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
