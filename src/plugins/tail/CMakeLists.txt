plugin_option(PLUGIN_TAIL "Parsing files" ON)

set(BUILD_PLUGIN_TAIL 0)

if(PLUGIN_TAIL)
    set(BUILD_PLUGIN_TAIL 1)
endif()

set(BUILD_PLUGIN_TAIL ${BUILD_PLUGIN_TAIL} PARENT_SCOPE)

if(BUILD_PLUGIN_TAIL)
    set(PLUGIN_TAIL_SRC tail.c)
    add_library(tail MODULE ${PLUGIN_TAIL_SRC})
    target_link_libraries(tail PRIVATE libmetric libutils)
    set_target_properties(tail PROPERTIES PREFIX "")

#    add_executable(test_plugin_tail EXCLUDE_FROM_ALL tail_test.c ${PLUGIN_TAIL_SRC})
#    target_link_libraries(test_plugin_tail libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_tail)
#    add_test(NAME test_plugin_tail COMMAND test_plugin_tail WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS tail DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-tail.5 ncollectd-tail.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-tail.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
