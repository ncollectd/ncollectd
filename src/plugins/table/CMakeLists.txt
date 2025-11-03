plugin_option(PLUGIN_TABLE "Parsing of tabular data" ON)

set(BUILD_PLUGIN_TABLE 0)

if(PLUGIN_TABLE)
    set(BUILD_PLUGIN_TABLE 1)
endif()

set(BUILD_PLUGIN_TABLE ${BUILD_PLUGIN_TABLE} PARENT_SCOPE)

if(BUILD_PLUGIN_TABLE)
    set(PLUGIN_TABLE_SRC table.c)
    add_library(table MODULE ${PLUGIN_TABLE_SRC})
    target_link_libraries(table PRIVATE libmetric libutils)
    set_target_properties(table PROPERTIES PREFIX "")

#    add_executable(test_plugin_table EXCLUDE_FROM_ALL table_test.c ${PLUGIN_TABLE_SRC})
#    target_link_libraries(test_plugin_table libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_table)
#    add_test(NAME test_plugin_table COMMAND test_plugin_table WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS table DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-table.5 ncollectd-table.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-table.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
