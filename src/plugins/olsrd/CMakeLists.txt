plugin_option(PLUGIN_OLSRD "olsrd statistics" ON)

set(BUILD_PLUGIN_OLSRD 0)

if(PLUGIN_OLSRD)
    set(BUILD_PLUGIN_OLSRD 1)
endif()

set(BUILD_PLUGIN_OLSRD ${BUILD_PLUGIN_OLSRD} PARENT_SCOPE)

if(BUILD_PLUGIN_OLSRD)
    set(PLUGIN_OLSRD_SRC olsrd.c)
    add_library(olsrd MODULE ${PLUGIN_OLSRD_SRC})
    target_link_libraries(olsrd PRIVATE libutils libmetric)
    set_target_properties(olsrd PROPERTIES PREFIX "")

#    add_executable(test_plugin_olsrd EXCLUDE_FROM_ALL olsrd_test.c ${PLUGIN_OLSRD_SRC})
#    target_link_libraries(test_plugin_olsrd libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_olsrd)
#    add_test(NAME test_plugin_olsrd COMMAND test_plugin_olsrd WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS olsrd DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-olsrd.5 ncollectd-olsrd.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-olsrd.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
