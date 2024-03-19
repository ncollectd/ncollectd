plugin_option(PLUGIN_NUMA "NUMA virtual memory statistics" ON)

set(BUILD_PLUGIN_NUMA 0)

if(PLUGIN_NUMA)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_NUMA 1)
    else()
        set(BUILD_PLUGIN_NUMA_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_NUMA ${BUILD_PLUGIN_NUMA} PARENT_SCOPE)

if(BUILD_PLUGIN_NUMA)
    set(PLUGIN_NUMA_SRC numa.c)
    add_library(numa MODULE ${PLUGIN_NUMA_SRC})
    target_link_libraries(numa PRIVATE libutils libmetric)
    set_target_properties(numa PROPERTIES PREFIX "")

    add_executable(test_plugin_numa EXCLUDE_FROM_ALL numa_test.c ${PLUGIN_NUMA_SRC})
    target_link_libraries(test_plugin_numa libtest libutils libmetric -lm)
    add_dependencies(build_tests test_plugin_numa)
    add_test(NAME test_plugin_numa COMMAND test_plugin_numa WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS numa DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-numa.5 ncollectd-numa.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-numa.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
