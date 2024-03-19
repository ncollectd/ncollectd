plugin_option(PLUGIN_MATCH_REGEX "Match Regex" ON)

set(BUILD_PLUGIN_MATCH_REGEX 0)

if(PLUGIN_MATCH_REGEX)
    set(BUILD_PLUGIN_MATCH_REGEX 1)
endif()

set(BUILD_PLUGIN_MATCH_REGEX ${BUILD_PLUGIN_MATCH_REGEX} PARENT_SCOPE)

if(BUILD_PLUGIN_MATCH_REGEX)
    set(PLUGIN_MATCH_REGEX_SRC match_regex.c)
    add_library(match_regex MODULE ${PLUGIN_MATCH_REGEX_SRC})
    target_link_libraries(match_regex PRIVATE libutils libmetric)
    set_target_properties(match_regex PROPERTIES PREFIX "")

#    add_executable(test_plugin_match_regex EXCLUDE_FROM_ALL match_regex_test.c ${PLUGIN_MATCH_REGEX_SRC})
#    target_link_libraries(test_plugin_match_regex libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_match_regex)
#    add_test(NAME test_plugin_match_regex COMMAND test_plugin_match_regex WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS match_regex DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-match_regex.5 ncollectd-match_regex.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-match_regex.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
