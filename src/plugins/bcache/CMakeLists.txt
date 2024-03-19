plugin_option(PLUGIN_BCACHE "Bcache statistics" ON)

set(BUILD_PLUGIN_BCACHE 0)

if(PLUGIN_BCACHE)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_BCACHE 1)
    else()
        set(BUILD_PLUGIN_BCACHE_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_BCACHE ${BUILD_PLUGIN_BCACHE} PARENT_SCOPE)

if(BUILD_PLUGIN_BCACHE)
    set(PLUGIN_BCACHE_SRC bcache.c)
    add_library(bcache MODULE ${PLUGIN_BCACHE_SRC})
    target_link_libraries(bcache PRIVATE libutils libmetric)
    set_target_properties(bcache PROPERTIES PREFIX "")

#    add_executable(test_plugin_bcache EXCLUDE_FROM_ALL bcache_test.c ${PLUGIN_BCACHE_SRC})
#    target_link_libraries(test_plugin_bcache libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_bcache)
#    add_test(NAME test_plugin_bcache COMMAND test_plugin_bcache WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS bcache DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-bcache.5 ncollectd-bcache.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-bcache.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
