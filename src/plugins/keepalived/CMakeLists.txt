plugin_option(PLUGIN_KEEPALIVED "keepalived statistics" ON)

set(BUILD_PLUGIN_KEEPALIVED 0)

if(PLUGIN_KEEPALIVED)
    set(BUILD_PLUGIN_KEEPALIVED 1)
endif()

set(BUILD_PLUGIN_KEEPALIVED ${BUILD_PLUGIN_KEEPALIVED} PARENT_SCOPE)

if(BUILD_PLUGIN_KEEPALIVED)
    set(PLUGIN_KEEPALIVED_SRC keepalived.c)
    add_library(keepalived MODULE ${PLUGIN_KEEPALIVED_SRC})
    target_link_libraries(keepalived PRIVATE libutils libmetric)
    set_target_properties(keepalived PROPERTIES PREFIX "")

#    add_executable(test_plugin_keepalived EXCLUDE_FROM_ALL keepalived_test.c ${PLUGIN_KEEPALIVED_SRC})
#    target_link_libraries(test_plugin_keepalived libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_keepalived)
#    add_test(NAME test_plugin_keepalived COMMAND test_plugin_keepalived WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS keepalived DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-keepalived.5 ncollectd-keepalived.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-keepalived.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
