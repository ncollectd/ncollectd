plugin_option(PLUGIN_INFO "Define static Metrics" ON)

set(BUILD_PLUGIN_INFO 0)

if(PLUGIN_INFO)
    set(BUILD_PLUGIN_INFO 1)
endif()

set(BUILD_PLUGIN_INFO ${BUILD_PLUGIN_INFO} PARENT_SCOPE)

if(BUILD_PLUGIN_INFO)
    set(PLUGIN_INFO_SRC info.c)
    add_library(info MODULE ${PLUGIN_INFO_SRC})
    target_link_libraries(info PRIVATE libmetric libutils)
    set_target_properties(info PROPERTIES PREFIX "")

#    add_executable(test_plugin_info EXCLUDE_FROM_ALL info_test.c ${PLUGIN_INFO_SRC})
#    target_link_libraries(test_plugin_info libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_info)
#    add_test(NAME test_plugin_info COMMAND test_plugin_info WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS info DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-info.5 ncollectd-info.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-info.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
