plugin_option(PLUGIN_IPSTATS "IP packet statistics" ON)

set(BUILD_PLUGIN_IPSTATS 0)

if(PLUGIN_IPSTATS)
    if(BUILD_FREEBSD)
        set(BUILD_PLUGIN_IPSTATS 1)
    else()
        set(BUILD_PLUGIN_IPSTATS_REASON "FreeBSD only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_IPSTATS ${BUILD_PLUGIN_IPSTATS} PARENT_SCOPE)

if(BUILD_PLUGIN_IPSTATS)
    set(PLUGIN_IPSTATS_SRC ipstats.c)
    add_library(ipstats MODULE ${PLUGIN_IPSTATS_SRC})
    target_link_libraries(ipstats PRIVATE libutils libmetric)
    set_target_properties(ipstats PROPERTIES PREFIX "")

#    add_executable(test_plugin_ipstats EXCLUDE_FROM_ALL ipstats_test.c ${PLUGIN_IPSTATS_SRC})
#    target_link_libraries(test_plugin_ipstats libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_ipstats)
#    add_test(NAME test_plugin_ipstats COMMAND test_plugin_ipstats WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS ipstats DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-ipstats.5 ncollectd-ipstats.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-ipstats.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
