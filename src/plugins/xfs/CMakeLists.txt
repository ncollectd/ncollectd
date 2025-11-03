plugin_option(PLUGIN_XFS "XFS filesystem statistics" ON)

set(BUILD_PLUGIN_XFS 0)

if(PLUGIN_XFS)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_XFS 1)
    else()
        set(BUILD_PLUGIN_XFS_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_XFS ${BUILD_PLUGIN_XFS} PARENT_SCOPE)

if(BUILD_PLUGIN_XFS)
    gperf_generate(xfs_stats.gperf "${CMAKE_CURRENT_BINARY_DIR}/xfs_stats.h" XFS_STATS_H)
    set(PLUGIN_XFS_SRC xfs.c xfs_stats_fam.h)
    add_library(xfs MODULE ${PLUGIN_XFS_SRC} ${XFS_STATS_H})
    target_link_libraries(xfs PRIVATE libmetric libutils)
    set_target_properties(xfs PROPERTIES PREFIX "")

#    add_executable(test_plugin_xfs EXCLUDE_FROM_ALL xfs_test.c ${PLUGIN_XFS_SRC})
#    target_link_libraries(test_plugin_xfs libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_xfs)
#    add_test(NAME test_plugin_xfs COMMAND test_plugin_xfs WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS xfs DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-xfs.5 ncollectd-xfs.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-xfs.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
