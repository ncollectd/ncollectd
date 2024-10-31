plugin_option(PLUGIN_JOURNAL "Journal statistics" ON)

set(BUILD_PLUGIN_JOURNAL 0)

if(PLUGIN_JOURNAL)
    find_package(LibSystemd)
    if (LIBSYSTEMD_FOUND)
        set(BUILD_PLUGIN_JOURNAL 1)
    else()
        set(BUILD_PLUGIN_JOURNAL_REASON "libsystemd not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_JOURNAL ${BUILD_PLUGIN_JOURNAL} PARENT_SCOPE)

if(BUILD_PLUGIN_JOURNAL)
    set(PLUGIN_JOURNAL_SRC journal.c)
    add_library(journal MODULE ${PLUGIN_JOURNAL_SRC})
    set(PLUGIN_JOURNAL_LIBS libutils libmetric)
    target_link_libraries(journal PRIVATE libutils libmetric LibSystemd::LibSystemd)
    target_compile_definitions(journal PUBLIC ${PLUGIN_JOURNAL_DEFINITIONS})
    set_target_properties(journal PROPERTIES PREFIX "")

    install(TARGETS journal DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-journal.5 ncollectd-journal.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-journal.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
