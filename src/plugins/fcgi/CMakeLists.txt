plugin_option(PLUGIN_FCGI "Generic FCGI statistics" ON)

set(BUILD_PLUGIN_FCGI 0)

if(PLUGIN_FCGI)
    set(BUILD_PLUGIN_FCGI 1)
endif()

set(BUILD_PLUGIN_FCGI ${BUILD_PLUGIN_FCGI} PARENT_SCOPE)

if(BUILD_PLUGIN_FCGI)
    set(PLUGIN_FCGI_SRC fcgi.c)
    add_library(fcgi MODULE ${PLUGIN_FCGI_SRC})
    target_link_libraries(fcgi PRIVATE libutils libmetric)
    set_target_properties(fcgi PROPERTIES PREFIX "")

#    add_executable(test_plugin_fcgi EXCLUDE_FROM_ALL fcgi_test.c ${PLUGIN_FCGI_SRC})
#    target_link_libraries(test_plugin_fcgi libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_fcgi)
#    add_test(NAME test_plugin_fcgi COMMAND test_plugin_fcgi WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS fcgi DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-fcgi.5 ncollectd-fcgi.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-fcgi.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
