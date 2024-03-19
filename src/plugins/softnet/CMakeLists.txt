plugin_option(PLUGIN_SOFTNET "softnet statistics" ON)

set(BUILD_PLUGIN_SOFTNET 0)

if(PLUGIN_SOFTNET)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_SOFTNET 1)
    else()
        set(BUILD_PLUGIN_SOFTNET_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_SOFTNET ${BUILD_PLUGIN_SOFTNET} PARENT_SCOPE)

if(BUILD_PLUGIN_SOFTNET)
    set(PLUGIN_SOFTNET_SRC softnet.c)
    add_library(softnet MODULE ${PLUGIN_SOFTNET_SRC})
    target_link_libraries(softnet PRIVATE libutils libmetric)
    set_target_properties(softnet PROPERTIES PREFIX "")

    add_executable(test_plugin_softnet EXCLUDE_FROM_ALL softnet_test.c ${PLUGIN_SOFTNET_SRC})
    target_link_libraries(test_plugin_softnet libtest libutils libmetric -lm)
    add_dependencies(build_tests test_plugin_softnet)
    add_test(NAME test_plugin_softnet COMMAND test_plugin_softnet WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS softnet DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-softnet.5 ncollectd-softnet.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-softnet.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
