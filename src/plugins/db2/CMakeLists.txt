plugin_option(PLUGIN_DB2 "DB2 database statistics" ON)

set(BUILD_PLUGIN_DB2 0)

if(PLUGIN_DB2)
    find_package(LibDb2)
    if (LIBDB2_FOUND)
        set(BUILD_PLUGIN_DB2 1)
    else()
        set(BUILD_PLUGIN_DB2_REASON "libdb2 not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_DB2 ${BUILD_PLUGIN_DB2} PARENT_SCOPE)

if(BUILD_PLUGIN_DB2)
    set(PLUGIN_DB2_SRC db2.c)
    add_library(db2 MODULE ${PLUGIN_DB2_SRC})
    target_link_libraries(db2 PRIVATE libutils libmetric libdbquery LibDb2::LibDb2)
    set_target_properties(db2 PROPERTIES PREFIX "")
    install(TARGETS db2 DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-db2.5 ncollectd-db2.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-db2.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
