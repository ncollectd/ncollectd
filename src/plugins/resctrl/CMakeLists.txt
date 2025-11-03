plugin_option(PLUGIN_RESCTRL "Error Detection And Correction (RESCTRL) statistics" ON)

set(BUILD_PLUGIN_RESCTRL 0)

if(PLUGIN_RESCTRL)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_RESCTRL 1)
    else()
        set(BUILD_PLUGIN_RESCTRL_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_RESCTRL ${BUILD_PLUGIN_RESCTRL} PARENT_SCOPE)

if(BUILD_PLUGIN_RESCTRL)
    set(PLUGIN_RESCTRL_SRC resctrl.c)
    add_library(resctrl MODULE ${PLUGIN_RESCTRL_SRC})
    target_link_libraries(resctrl PRIVATE libmetric libutils)
    set_target_properties(resctrl PROPERTIES PREFIX "")

    add_executable(test_plugin_resctrl EXCLUDE_FROM_ALL resctrl_test.c ${PLUGIN_RESCTRL_SRC})
    target_link_libraries(test_plugin_resctrl libtest libmetric libutils -lm)
    add_dependencies(build_tests test_plugin_resctrl)
    add_test(NAME test_plugin_resctrl COMMAND test_plugin_resctrl WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS resctrl DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-resctrl.5 ncollectd-resctrl.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-resctrl.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
