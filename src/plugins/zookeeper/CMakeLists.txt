plugin_option(PLUGIN_ZOOKEEPER "Zookeeper statistics" ON)

set(BUILD_PLUGIN_ZOOKEEPER 0)

if(PLUGIN_ZOOKEEPER)
    set(BUILD_PLUGIN_ZOOKEEPER 1)
endif()

set(BUILD_PLUGIN_ZOOKEEPER ${BUILD_PLUGIN_ZOOKEEPER} PARENT_SCOPE)

if(BUILD_PLUGIN_ZOOKEEPER)
    gperf_generate(zookeeper_stats.gperf "${CMAKE_CURRENT_BINARY_DIR}/zookeeper_stats.h" ZOOKEEPER_STATS_H)
    set(PLUGIN_ZOOKEEPER_SRC zookeeper.c)
    add_library(zookeeper MODULE ${PLUGIN_ZOOKEEPER_SRC} ${ZOOKEEPER_STATS_H})
    target_link_libraries(zookeeper PRIVATE libutils libmetric)
    set_target_properties(zookeeper PROPERTIES PREFIX "")

#    add_executable(test_plugin_zookeeper EXCLUDE_FROM_ALL zookeeper_test.c ${PLUGIN_ZOOKEEPER_SRC})
#    target_link_libraries(test_plugin_zookeeper libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_zookeeper)
#    add_test(NAME test_plugin_zookeeper COMMAND test_plugin_zookeeper WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS zookeeper DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-zookeeper.5 ncollectd-zookeeper.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-zookeeper.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
