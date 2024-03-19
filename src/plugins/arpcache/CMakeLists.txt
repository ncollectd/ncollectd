plugin_option(PLUGIN_ARPCACHE "ARP cache statistics" ON)

set(BUILD_PLUGIN_ARPCACHE 0)

if(PLUGIN_ARPCACHE)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_ARPCACHE 1)
    else()
        set(BUILD_PLUGIN_ARPCACHE_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_ARPCACHE ${BUILD_PLUGIN_ARPCACHE} PARENT_SCOPE)

if(BUILD_PLUGIN_ARPCACHE)
    set(PLUGIN_ARPCACHE_SRC arpcache.c)
    add_library(arpcache MODULE ${PLUGIN_ARPCACHE_SRC})
    target_link_libraries(arpcache PRIVATE libutils libmetric)
    set_target_properties(arpcache PROPERTIES PREFIX "")

    add_executable(test_plugin_arpcache EXCLUDE_FROM_ALL arpcache_test.c ${PLUGIN_ARPCACHE_SRC})
    target_link_libraries(test_plugin_arpcache libtest libutils libmetric -lm)
    add_dependencies(build_tests test_plugin_arpcache)
    add_test(NAME test_plugin_arpcache COMMAND test_plugin_arpcache WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS arpcache DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-arpcache.5 ncollectd-arpcache.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-arpcache.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
