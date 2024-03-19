plugin_option(PLUGIN_KSM "kernel same-page merging statistics" ON)

set(BUILD_PLUGIN_KSM 0)

if(PLUGIN_KSM)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_KSM 1)
    else()
        set(BUILD_PLUGIN_KSM_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_KSM ${BUILD_PLUGIN_KSM} PARENT_SCOPE)

if(BUILD_PLUGIN_KSM)
    set(PLUGIN_KSM_SRC ksm.c)
    add_library(ksm MODULE ${PLUGIN_KSM_SRC})
    target_link_libraries(ksm PRIVATE libutils libmetric)
    set_target_properties(ksm PROPERTIES PREFIX "")

#    add_executable(test_plugin_ksm EXCLUDE_FROM_ALL ksm_test.c ${PLUGIN_KSM_SRC})
#    target_link_libraries(test_plugin_ksm libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_ksm)
#    add_test(NAME test_plugin_ksm COMMAND test_plugin_ksm WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS ksm DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-ksm.5 ncollectd-ksm.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-ksm.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
