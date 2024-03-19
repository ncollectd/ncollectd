plugin_option(PLUGIN_NFCONNTRACK "nfsconntrack statistics" ON)

set(BUILD_PLUGIN_NFCONNTRACK 0)

if(PLUGIN_NFCONNTRACK)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_NFCONNTRACK 1)
    else()
        set(BUILD_PLUGIN_NFCONNTRACK_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_NFCONNTRACK ${BUILD_PLUGIN_NFCONNTRACK} PARENT_SCOPE)

if(BUILD_PLUGIN_NFCONNTRACK)
    set(PLUGIN_NFCONNTRACK_SRC nfconntrack.c)
    add_library(nfconntrack MODULE ${PLUGIN_NFCONNTRACK_SRC})
    target_link_libraries(nfconntrack PRIVATE libutils libmetric)
    set_target_properties(nfconntrack PROPERTIES PREFIX "")

    add_executable(test_plugin_nfconntrack EXCLUDE_FROM_ALL nfconntrack_test.c ${PLUGIN_NFCONNTRACK_SRC})
    target_link_libraries(test_plugin_nfconntrack libtest libutils libmetric -lm)
    add_dependencies(build_tests test_plugin_nfconntrack)
    add_test(NAME test_plugin_nfconntrack COMMAND test_plugin_nfconntrack WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS nfconntrack DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-nfconntrack.5 ncollectd-nfconntrack.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-nfconntrack.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
