plugin_option(PLUGIN_NOTIFY_EXEC "Execute an external program or handle notifications" ON)

set(BUILD_PLUGIN_NOTIFY_EXEC 0)

if(PLUGIN_NOTIFY_EXEC)
    set(BUILD_PLUGIN_NOTIFY_EXEC 1)
endif()

set(BUILD_PLUGIN_NOTIFY_EXEC ${BUILD_PLUGIN_NOTIFY_EXEC} PARENT_SCOPE)

if(BUILD_PLUGIN_NOTIFY_EXEC)
    set(PLUGIN_NOTIFY_EXEC_SRC notify_exec.c)
    add_library(notify_exec MODULE ${PLUGIN_NOTIFY_EXEC_SRC})
    target_link_libraries(notify_exec PRIVATE libutils libmetric libformat)
    set_target_properties(notify_exec PROPERTIES PREFIX "")

#    add_executable(test_plugin_notify_exec EXCLUDE_FROM_ALL notify_exec_test.c ${PLUGIN_NOTIFY_EXEC_SRC})
#    target_link_libraries(test_plugin_notify_exec libtest libutils libmetric m)
#    add_dependencies(build_tests test_plugin_notify_exec)
#    add_test(NAME test_plugin_notify_exec COMMAND test_plugin_notify_exec WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS notify_exec DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-notify_exec.5 ncollectd-notify_exec.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-notify_exec.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
