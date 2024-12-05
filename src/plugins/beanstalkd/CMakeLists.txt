plugin_option(PLUGIN_BEANSTALKD "Beanstalkd statistics" ON)

set(BUILD_PLUGIN_BEANSTALKD 0)

if(PLUGIN_BEANSTALKD)
    set(BUILD_PLUGIN_BEANSTALKD 1)
endif()

set(BUILD_PLUGIN_BEANSTALKD ${BUILD_PLUGIN_BEANSTALKD} PARENT_SCOPE)

if(BUILD_PLUGIN_BEANSTALKD)
    gperf_generate(beanstalkd_stats.gperf "${CMAKE_CURRENT_BINARY_DIR}/beanstalkd_stats.h" BEANSTALKD_STATS_H)
    set(PLUGIN_BEANSTALKD_SRC beanstalkd.c beanstalkd_stats.h ${BEANSTALKD_STATS_H})
    set(PLUGIN_BEANSTALKD_LIBS libutils libmetric)
    add_library(beanstalkd MODULE ${PLUGIN_BEANSTALKD_SRC})
    target_link_libraries(beanstalkd PRIVATE ${PLUGIN_BEANSTALKD_LIBS})
    set_target_properties(beanstalkd PROPERTIES PREFIX "")

    add_executable(test_plugin_beanstalkd EXCLUDE_FROM_ALL beanstalkd_test.c ${PLUGIN_BEANSTALKD_SRC})
    target_link_libraries(test_plugin_beanstalkd libtest libutils libmetric -lm -lpthread)
    add_dependencies(build_tests test_plugin_beanstalkd)
    add_test(NAME test_plugin_beanstalkd COMMAND test_plugin_beanstalkd WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS beanstalkd DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-beanstalkd.5 ncollectd-beanstalkd.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-beanstalkd.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
