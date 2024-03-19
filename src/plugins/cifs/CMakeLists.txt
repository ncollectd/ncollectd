plugin_option(PLUGIN_CIFS "CIFS client statistics" ON)

set(BUILD_PLUGIN_CIFS 0)

if(PLUGIN_CIFS)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_CIFS 1)
    else()
        set(BUILD_PLUGIN_CIFS_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_CIFS ${BUILD_PLUGIN_CIFS} PARENT_SCOPE)

if(BUILD_PLUGIN_CIFS)
    set(PLUGIN_CIFS_SRC cifs.c)
    add_library(cifs MODULE ${PLUGIN_CIFS_SRC})
    target_link_libraries(cifs PRIVATE libutils libmetric)
    set_target_properties(cifs PROPERTIES PREFIX "")

    add_executable(test_plugin_cifs EXCLUDE_FROM_ALL cifs_test.c ${PLUGIN_CIFS_SRC})
    target_link_libraries(test_plugin_cifs libtest libutils libmetric -lm)
    add_dependencies(build_tests test_plugin_cifs)
    add_test(NAME test_plugin_cifs COMMAND test_plugin_cifs WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS cifs DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-cifs.5 ncollectd-cifs.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-cifs.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
