plugin_option(PLUGIN_MATCH_JSONPATH "Match with jsonpath" ON)

set(BUILD_PLUGIN_MATCH_JSONPATH 0)

if(PLUGIN_MATCH_JSONPATH)
    set(BUILD_PLUGIN_MATCH_JSONPATH 1)
endif()

set(BUILD_PLUGIN_MATCH_JSONPATH ${BUILD_PLUGIN_MATCH_JSONPATH} PARENT_SCOPE)

if(BUILD_PLUGIN_MATCH_JSONPATH)
    set(JSONPATH_SCAN_C "${CMAKE_CURRENT_BINARY_DIR}/jsonpath_scan.c")
    set(JSONPATH_SCAN_H "${CMAKE_CURRENT_BINARY_DIR}/jsonpath_scan.h")
    set(JSONPATH_GRAM_C "${CMAKE_CURRENT_BINARY_DIR}/jsonpath_gram.c")
    set(JSONPATH_GRAM_H "${CMAKE_CURRENT_BINARY_DIR}/jsonpath_gram.h")

    flex_target(JSONPATH_SCANNER "${CMAKE_CURRENT_SOURCE_DIR}/jsonpath_scan.l"
                "${JSONPATH_SCAN_C}" DEFINES_FILE "${JSONPATH_SCAN_H}")
    bison_target(JSONPATH_PARSER "${CMAKE_CURRENT_SOURCE_DIR}/jsonpath_gram.y"
                 "${JSONPATH_GRAM_C}" DEFINES_FILE "${JSONPATH_GRAM_H}")
    add_flex_bison_dependency(JSONPATH_SCANNER JSONPATH_PARSER)

    set(PLUGIN_MATCH_JSONPATH_SRC match_jsonpath.c jsonpath.c jsonpath.h jsonpath_list.h
                                  jsonpath_internal.h jsonpath_exec.c
                                 "${JSONPATH_SCAN_C}" "${JSONPATH_SCAN_H}"
                                 "${JSONPATH_GRAM_C}" "${JSONPATH_GRAM_H}")

    add_library(match_jsonpath MODULE ${PLUGIN_MATCH_JSONPATH_SRC})
    target_link_libraries(match_jsonpath PRIVATE libmetric libutils libxson)
    set_target_properties(match_jsonpath PROPERTIES PREFIX "")

    add_executable(test_plugin_match_jsonpath EXCLUDE_FROM_ALL jsonpath_test.c ${PLUGIN_MATCH_JSONPATH_SRC})
    target_link_libraries(test_plugin_match_jsonpath libtest libmetric libutils libxson m)
    add_dependencies(build_tests test_plugin_match_jsonpath)
    add_test(NAME test_plugin_match_jsonpath COMMAND test_plugin_match_jsonpath WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS match_jsonpath DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-match_jsonpath.5 ncollectd-match_jsonpath.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-match_jsonpath.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
