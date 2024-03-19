plugin_option(PLUGIN_NFS "NFS client statistics" ON)

set(BUILD_PLUGIN_NFS 0)

if(PLUGIN_NFS)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_NFS 1)
    else()
        set(BUILD_PLUGIN_NFS_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_NFS ${BUILD_PLUGIN_NFS} PARENT_SCOPE)

if(BUILD_PLUGIN_NFS)
    set(PLUGIN_NFS_SRC nfs.c)
    add_library(nfs MODULE ${PLUGIN_NFS_SRC})
    target_link_libraries(nfs PRIVATE libutils libmetric)
    set_target_properties(nfs PROPERTIES PREFIX "")

    add_executable(test_plugin_nfs EXCLUDE_FROM_ALL nfs_test.c ${PLUGIN_NFS_SRC})
    target_link_libraries(test_plugin_nfs libtest libutils libmetric -lm)
    add_dependencies(build_tests test_plugin_nfs)
    add_test(NAME test_plugin_nfs COMMAND test_plugin_nfs WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS nfs DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-nfs.5 ncollectd-nfs.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-nfs.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
