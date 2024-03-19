plugin_option(PLUGIN_NFSD "NFS server statistics" ON)

set(BUILD_PLUGIN_NFSD 0)

if(PLUGIN_NFSD)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_NFSD 1)
    else()
        set(BUILD_PLUGIN_NFSD_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_NFSD ${BUILD_PLUGIN_NFSD} PARENT_SCOPE)

if(BUILD_PLUGIN_NFSD)
    set(PLUGIN_NFSD_SRC nfsd.c)
    add_library(nfsd MODULE ${PLUGIN_NFSD_SRC})
    target_link_libraries(nfsd PRIVATE libutils libmetric)
    set_target_properties(nfsd PROPERTIES PREFIX "")

    add_executable(test_plugin_nfsd EXCLUDE_FROM_ALL nfsd_test.c ${PLUGIN_NFSD_SRC})
    target_link_libraries(test_plugin_nfsd libtest libutils libmetric -lm)
    add_dependencies(build_tests test_plugin_nfsd)
    add_test(NAME test_plugin_nfsd COMMAND test_plugin_nfsd WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS nfsd DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-nfsd.5 ncollectd-nfsd.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-nfsd.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
