plugin_option(PLUGIN_DS389 "DS389 statistics" ON)

set(BUILD_PLUGIN_DS389 0)

if(PLUGIN_DS389)
    find_package(LibLdap)
    if (LIBLDAP_FOUND)
        set(BUILD_PLUGIN_DS389 1)
    else()
        set(BUILD_PLUGIN_DS389_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_DS389 ${BUILD_PLUGIN_DS389} PARENT_SCOPE)

if(BUILD_PLUGIN_DS389)
    set(PLUGIN_DS389_SRC ds389.c)
    add_library(ds389 MODULE ${PLUGIN_DS389_SRC})
    target_link_libraries(ds389 PRIVATE libutils libmetric LibLdap::LibLdap)
    set_target_properties(ds389 PROPERTIES PREFIX "")
    install(TARGETS ds389 DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-ds389.5 ncollectd-ds389.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-ds389.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
