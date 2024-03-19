plugin_option(PLUGIN_BONDING "Bonding statistics" ON)

set(BUILD_PLUGIN_BONDING 0)

if(PLUGIN_BONDING)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_BONDING 1)
    else()
        set(BUILD_PLUGIN_BONDING_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_BONDING ${BUILD_PLUGIN_BONDING} PARENT_SCOPE)

if(BUILD_PLUGIN_BONDING)
    set(PLUGIN_BONDING_SRC bonding.c)
    add_library(bonding MODULE ${PLUGIN_BONDING_SRC})
    target_link_libraries(bonding PRIVATE libutils libmetric)
    set_target_properties(bonding PROPERTIES PREFIX "")

    add_executable(test_plugin_bonding EXCLUDE_FROM_ALL bonding_test.c ${PLUGIN_BONDING_SRC})
    target_link_libraries(test_plugin_bonding libtest libutils libmetric -lm)
    add_dependencies(build_tests test_plugin_bonding)
    add_test(NAME test_plugin_bonding COMMAND test_plugin_bonding WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS bonding DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-bonding.5 ncollectd-bonding.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-bonding.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
