plugin_option(PLUGIN_LDAP "LDAP statistics" ON)

set(BUILD_PLUGIN_LDAP 0)

if(PLUGIN_LDAP)
    find_package(LibLdap)
    if (LIBLDAP_FOUND)
        set(BUILD_PLUGIN_LDAP 1)
    else()
        set(BUILD_PLUGIN_LDAP_REASON "libldap no found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_LDAP ${BUILD_PLUGIN_LDAP} PARENT_SCOPE)

if(BUILD_PLUGIN_LDAP)
    set(PLUGIN_LDAP_SRC ldap.c)
    add_library(ldap MODULE ${PLUGIN_LDAP_SRC})
    target_link_libraries(ldap PRIVATE libutils libmetric LibLdap::LibLdap)
    set_target_properties(ldap PROPERTIES PREFIX "")
    install(TARGETS ldap DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-ldap.5 ncollectd-ldap.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-ldap.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
