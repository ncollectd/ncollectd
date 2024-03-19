plugin_option(PLUGIN_EDAC "Error Detection And Correction (EDAC) statistics" ON)

set(BUILD_PLUGIN_EDAC 0)

if(PLUGIN_EDAC)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_EDAC 1)
    else()
        set(BUILD_PLUGIN_EDAC_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_EDAC ${BUILD_PLUGIN_EDAC} PARENT_SCOPE)

if(BUILD_PLUGIN_EDAC)
    set(PLUGIN_EDAC_SRC edac.c)
    add_library(edac MODULE ${PLUGIN_EDAC_SRC})
    target_link_libraries(edac PRIVATE libutils libmetric)
    set_target_properties(edac PROPERTIES PREFIX "")

    add_executable(test_plugin_edac EXCLUDE_FROM_ALL edac_test.c ${PLUGIN_EDAC_SRC})
    target_link_libraries(test_plugin_edac libtest libutils libmetric -lm)
    add_dependencies(build_tests test_plugin_edac)
    add_test(NAME test_plugin_edac COMMAND test_plugin_edac WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS edac DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-edac.5 ncollectd-edac.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-edac.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
