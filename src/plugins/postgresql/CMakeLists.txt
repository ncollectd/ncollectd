plugin_option(PLUGIN_POSTGRESQL "PostgreSQL database statistics" ON)

set(BUILD_PLUGIN_POSTGRESQL 0)

if(PLUGIN_POSTGRESQL)
    find_package(LibPq)
    if (LIBPQ_FOUND)
        set(BUILD_PLUGIN_POSTGRESQL 1)
    else()
        set(BUILD_PLUGIN_POSTGRESQL_REASON "libpq not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_POSTGRESQL ${BUILD_PLUGIN_POSTGRESQL} PARENT_SCOPE)

if(BUILD_PLUGIN_POSTGRESQL)
    set(PLUGIN_POSTGRESQL_SRC postgresql.c pg_stats.h pg_fam.h
                              pg_database.c pg_user.c pg_activity.c pg_replication.c
                              pg_archiver.c pg_bgwriter.c pg_slru.c pg_io.c)
    add_library(postgresql MODULE ${PLUGIN_POSTGRESQL_SRC})
    target_include_directories(postgresql PRIVATE ${LIBPOSTGRESQL_INCLUDE_DIRS})
    target_link_libraries(postgresql PRIVATE libutils libmetric libdbquery LibPq::LibPq)
    set_target_properties(postgresql PROPERTIES PREFIX "")
    install(TARGETS postgresql DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-postgresql.5 ncollectd-postgresql.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-postgresql.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
