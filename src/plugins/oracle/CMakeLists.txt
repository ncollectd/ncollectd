plugin_option(PLUGIN_ORACLE "Oracle database statistics" ON)

set(BUILD_PLUGIN_ORACLE 0)

if(PLUGIN_ORACLE)
    find_package(LibOci)
    if (LIBOCI_FOUND)
        set(BUILD_PLUGIN_ORACLE 1)
    else()
        set(BUILD_PLUGIN_ORACLE_REASON "oci libs not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_ORACLE ${BUILD_PLUGIN_ORACLE} PARENT_SCOPE)

if(BUILD_PLUGIN_ORACLE)
    set(PLUGIN_ORACLE_SRC oracle.c)
    add_library(oracle MODULE ${PLUGIN_ORACLE_SRC})
    target_link_libraries(oracle PRIVATE libutils libmetric libdbquery LibOci::LibOci)
    set_target_properties(oracle PROPERTIES PREFIX "")
    install(TARGETS oracle DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-oracle.5 ncollectd-oracle.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-oracle.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
