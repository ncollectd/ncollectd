plugin_option(PLUGIN_RTCACHE "Route cache statistics" ON)

set(BUILD_PLUGIN_RTCACHE 0)

if(PLUGIN_RTCACHE)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_RTCACHE 1)
    else()
        set(BUILD_PLUGIN_RTCACHE_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_RTCACHE ${BUILD_PLUGIN_RTCACHE} PARENT_SCOPE)

if(BUILD_PLUGIN_RTCACHE)
    set(PLUGIN_RTCACHE_SRC rtcache.c)
    add_library(rtcache MODULE ${PLUGIN_RTCACHE_SRC})
    target_link_libraries(rtcache PRIVATE libmetric libutils)
    set_target_properties(rtcache PROPERTIES PREFIX "")

    add_executable(test_plugin_rtcache EXCLUDE_FROM_ALL rtcache_test.c ${PLUGIN_RTCACHE_SRC})
    target_link_libraries(test_plugin_rtcache libtest libmetric libutils -lm)
    add_dependencies(build_tests test_plugin_rtcache)
    add_test(NAME test_plugin_rtcache COMMAND test_plugin_rtcache WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS rtcache DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-rtcache.5 ncollectd-rtcache.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-rtcache.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
