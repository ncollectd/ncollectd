plugin_option(PLUGIN_WRITE_POSTGRESQL "Write metric to PostgreSQL database" ON)

set(BUILD_PLUGIN_WRITE_POSTGRESQL 0)

if(PLUGIN_WRITE_POSTGRESQL)
    find_package(LibPq)
    if (LIBPQ_FOUND)
        set(BUILD_PLUGIN_WRITE_POSTGRESQL 1)
    else()
        set(BUILD_PLUGIN_WRITE_POSTGRESQL_REASON "libpq not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_WRITE_POSTGRESQL ${BUILD_PLUGIN_WRITE_POSTGRESQL} PARENT_SCOPE)

if(BUILD_PLUGIN_WRITE_POSTGRESQL)
    set(PLUGIN_WRITE_POSTGRESQL_SRC write_postgresql.c)
    add_library(write_postgresql MODULE ${PLUGIN_WRITE_POSTGRESQL_SRC})
    target_link_libraries(write_postgresql PRIVATE libmetric libutils LibPq::LibPq)
    set_target_properties(write_postgresql PROPERTIES PREFIX "")
    install(TARGETS write_postgresql DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-write_postgresql.5 ncollectd-write_postgresql.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-write_postgresql.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
