plugin_option(PLUGIN_LOCKS "Locked file statistics" ON)

set(BUILD_PLUGIN_LOCKS 0)

if(PLUGIN_LOCKS)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_LOCKS 1)
    else()
        set(BUILD_PLUGIN_LOCKS_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_LOCKS ${BUILD_PLUGIN_LOCKS} PARENT_SCOPE)

if(BUILD_PLUGIN_LOCKS)
    set(PLUGIN_LOCKS_SRC locks.c)
    add_library(locks MODULE ${PLUGIN_LOCKS_SRC})
    target_link_libraries(locks PRIVATE libutils libmetric)
    set_target_properties(locks PROPERTIES PREFIX "")

    add_executable(test_plugin_locks EXCLUDE_FROM_ALL locks_test.c ${PLUGIN_LOCKS_SRC})
    target_link_libraries(test_plugin_locks libtest libutils libmetric -lm)
    add_dependencies(build_tests test_plugin_locks)
    add_test(NAME test_plugin_locks COMMAND test_plugin_locks WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS locks DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-locks.5 ncollectd-locks.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-locks.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
