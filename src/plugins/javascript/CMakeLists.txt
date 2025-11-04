plugin_option(PLUGIN_JAVASCRIPT "Javascript plugin" ON)

set(BUILD_PLUGIN_JAVASCRIPT 0)

if(PLUGIN_JAVASCRIPT)
    set(BUILD_PLUGIN_JAVASCRIPT 1)
endif()

set(BUILD_PLUGIN_JAVASCRIPT ${BUILD_PLUGIN_JAVASCRIPT} PARENT_SCOPE)

if(BUILD_PLUGIN_JAVASCRIPT)
    set(PLUGIN_JAVASCRIPT_SRC javascript.c
                              jnotification.c jnotification.h
                              jmetricfamily.c jmetricfamily.h
                              jmetric.c jmetric.h
                              jutil.c jutil.h)
    add_library(javascript MODULE ${PLUGIN_JAVASCRIPT_SRC})
    target_link_libraries(javascript PRIVATE libquickjs libconfig libmetric libutils m)
    set_target_properties(javascript PROPERTIES PREFIX "")

    add_executable(test_plugin_javascript EXCLUDE_FROM_ALL javascript_test.c ${PLUGIN_JAVASCRIPT_SRC})
    target_link_libraries(test_plugin_javascript libtest libquickjs libconfig libmetric libutils m)
    add_dependencies(build_tests test_plugin_javascript)
    add_test(NAME test_plugin_javascript COMMAND test_plugin_javascript WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS javascript DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-javascript.5 ncollectd-javascript.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-javascript.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
