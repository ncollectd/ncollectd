plugin_option(PLUGIN_EXEC "Execution of external programs" ON)

set(BUILD_PLUGIN_EXEC 0)

if(PLUGIN_EXEC)
    set(BUILD_PLUGIN_EXEC 1)
endif()

set(BUILD_PLUGIN_EXEC ${BUILD_PLUGIN_EXEC} PARENT_SCOPE)

if(BUILD_PLUGIN_EXEC)
    set(PLUGIN_EXEC_SRC exec.c)
    add_library(exec MODULE ${PLUGIN_EXEC_SRC})
    target_link_libraries(exec PRIVATE libmetric libutils)
    set_target_properties(exec PROPERTIES PREFIX "")

#    add_executable(test_plugin_exec EXCLUDE_FROM_ALL exec_test.c ${PLUGIN_EXEC_SRC})
#    target_link_libraries(test_plugin_exec libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_exec)
#    add_test(NAME test_plugin_exec COMMAND test_plugin_exec WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS exec DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-exec.5 ncollectd-exec.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-exec.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
