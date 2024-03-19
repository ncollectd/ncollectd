plugin_option(PLUGIN_UNAME "Get uname info" ON)

set(BUILD_PLUGIN_UNAME 0)

if(PLUGIN_UNAME)
    set(BUILD_PLUGIN_UNAME 1)
endif()

set(BUILD_PLUGIN_UNAME ${BUILD_PLUGIN_UNAME} PARENT_SCOPE)

if(BUILD_PLUGIN_UNAME)
    set(PLUGIN_UNAME_SRC uname.c)
    add_library(uname MODULE ${PLUGIN_UNAME_SRC})
    target_link_libraries(uname PRIVATE libutils libmetric)
    set_target_properties(uname PROPERTIES PREFIX "")

#    add_executable(test_plugin_uname EXCLUDE_FROM_ALL uname_test.c ${PLUGIN_UNAME_SRC})
#    target_link_libraries(test_plugin_uname libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_uname)
#    add_test(NAME test_plugin_uname COMMAND test_plugin_uname WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS uname DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-uname.5 ncollectd-uname.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-uname.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
