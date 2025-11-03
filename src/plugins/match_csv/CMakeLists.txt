plugin_option(PLUGIN_MATCH_CSV "Match CSV" ON)

set(BUILD_PLUGIN_MATCH_CSV 0)

if(PLUGIN_MATCH_CSV)
    set(BUILD_PLUGIN_MATCH_CSV 1)
endif()

set(BUILD_PLUGIN_MATCH_CSV ${BUILD_PLUGIN_MATCH_CSV} PARENT_SCOPE)

if(BUILD_PLUGIN_MATCH_CSV)
    set(PLUGIN_MATCH_CSV_SRC match_csv.c)
    add_library(match_csv MODULE ${PLUGIN_MATCH_CSV_SRC})
    target_link_libraries(match_csv PRIVATE libmetric libutils)
    set_target_properties(match_csv PROPERTIES PREFIX "")

#    add_executable(test_plugin_match_csv EXCLUDE_FROM_ALL match_csv_test.c ${PLUGIN_MATCH_CSV_SRC})
#    target_link_libraries(test_plugin_match_csv libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_match_csv)
#    add_test(NAME test_plugin_match_csv COMMAND test_plugin_match_csv WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS match_csv DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-match_csv.5 ncollectd-match_csv.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-match_csv.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
