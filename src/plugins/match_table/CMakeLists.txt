plugin_option(PLUGIN_MATCH_TABLE "Parsing of tabular data" ON)

set(BUILD_PLUGIN_MATCH_TABLE 0)

if(PLUGIN_MATCH_TABLE)
    set(BUILD_PLUGIN_MATCH_TABLE 1)
endif()

set(BUILD_PLUGIN_MATCH_TABLE ${BUILD_PLUGIN_MATCH_TABLE} PARENT_SCOPE)

if(BUILD_PLUGIN_MATCH_TABLE)
    set(PLUGIN_MATCH_TABLE_SRC match_table.c)
    add_library(match_table MODULE ${PLUGIN_MATCH_TABLE_SRC})
    target_link_libraries(match_table PRIVATE libutils libmetric)
    set_target_properties(match_table PROPERTIES PREFIX "")

#    add_executable(test_plugin_match_table EXCLUDE_FROM_ALL match_table_test.c ${PLUGIN_MATCH_TABLE_SRC})
#    target_link_libraries(test_plugin_match_table libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_match_table)
#    add_test(NAME test_plugin_match_table COMMAND test_plugin_match_table WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS match_table DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-match_table.5 ncollectd-match_table.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-match_table.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
