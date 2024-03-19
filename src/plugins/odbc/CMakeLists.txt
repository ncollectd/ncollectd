plugin_option(PLUGIN_ODBC "ODBC database statistics" ON)

set(BUILD_PLUGIN_ODBC 0)

if(PLUGIN_ODBC)
    find_package(LibOdbc)
    if (LIBODBC_FOUND)
        set(BUILD_PLUGIN_ODBC 1)
    else()
        set(BUILD_PLUGIN_ODBC_REASON "libodbc not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_ODBC ${BUILD_PLUGIN_ODBC} PARENT_SCOPE)

if(BUILD_PLUGIN_ODBC)
    set(PLUGIN_ODBC_SRC odbc.c)
    add_library(odbc MODULE ${PLUGIN_ODBC_SRC})
    target_link_libraries(odbc PRIVATE libutils libmetric libdbquery LibOdbc::LibOdbc)
    set_target_properties(odbc PROPERTIES PREFIX "")
    install(TARGETS odbc DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-odbc.5 ncollectd-odbc.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-odbc.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
