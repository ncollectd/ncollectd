plugin_option(PLUGIN_ISCSI "ISCSI target statistics" ON)

set(BUILD_PLUGIN_ISCSI 0)

if(PLUGIN_ISCSI)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_ISCSI 1)
    else()
        set(BUILD_PLUGIN_ISCSI_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_ISCSI ${BUILD_PLUGIN_ISCSI} PARENT_SCOPE)

if(BUILD_PLUGIN_ISCSI)
    set(PLUGIN_ISCSI_SRC iscsi.c)
    add_library(iscsi MODULE ${PLUGIN_ISCSI_SRC})
    target_link_libraries(iscsi PRIVATE libutils libmetric)
    set_target_properties(iscsi PROPERTIES PREFIX "")

    add_executable(test_plugin_iscsi EXCLUDE_FROM_ALL iscsi_test.c ${PLUGIN_ISCSI_SRC})
    target_link_libraries(test_plugin_iscsi libtest libutils libmetric -lm)
    add_dependencies(build_tests test_plugin_iscsi)
    add_test(NAME test_plugin_iscsi COMMAND test_plugin_iscsi WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS iscsi DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-iscsi.5 ncollectd-iscsi.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-iscsi.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
