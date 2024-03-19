plugin_option(PLUGIN_MEMCACHED "memcached statistics" ON)

set(BUILD_PLUGIN_MEMCACHED 0)

if(PLUGIN_MEMCACHED)
    set(BUILD_PLUGIN_MEMCACHED 1)
# if BUILD_WITH_LIBSOCKET
# memcached_la_LIBADD += -lsocket
endif()

set(BUILD_PLUGIN_MEMCACHED ${BUILD_PLUGIN_MEMCACHED} PARENT_SCOPE)

if(BUILD_PLUGIN_MEMCACHED)
    gperf_generate(memcached_stats.gperf "${CMAKE_CURRENT_BINARY_DIR}/memcached_stats.h" MEMCACHED_STATS_H)
    set(PLUGIN_MEMCACHED_SRC memcached.c memcached_fam.h ${MEMCACHED_STATS_H})
    add_library(memcached MODULE ${PLUGIN_MEMCACHED_SRC})
    target_link_libraries(memcached PRIVATE libutils libmetric)
    set_target_properties(memcached PROPERTIES PREFIX "")

#    add_executable(test_plugin_memcached EXCLUDE_FROM_ALL memcached_test.c ${PLUGIN_MEMCACHED_SRC})
#    target_link_libraries(test_plugin_memcached libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_memcached)
#    add_test(NAME test_plugin_memcached COMMAND test_plugin_memcached WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS memcached DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-memcached.5 ncollectd-memcached.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-memcached.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
