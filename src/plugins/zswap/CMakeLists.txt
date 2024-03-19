plugin_option(PLUGIN_ZSWAP "ZSwap statistics" ON)

set(BUILD_PLUGIN_ZSWAP 0)

if(PLUGIN_ZSWAP)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_ZSWAP 1)
    else()
        set(BUILD_PLUGIN_ZSWAP_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_ZSWAP ${BUILD_PLUGIN_ZSWAP} PARENT_SCOPE)

if(BUILD_PLUGIN_ZSWAP)
    set(PLUGIN_ZSWAP_SRC zswap.c)
    add_library(zswap MODULE ${PLUGIN_ZSWAP_SRC})
    target_link_libraries(zswap PRIVATE libutils libmetric)
    set_target_properties(zswap PROPERTIES PREFIX "")

#    add_executable(test_plugin_zswap EXCLUDE_FROM_ALL zswap_test.c ${PLUGIN_ZSWAP_SRC})
#    target_link_libraries(test_plugin_zswap libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_zswap)
#    add_test(NAME test_plugin_zswap COMMAND test_plugin_zswap WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS zswap DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-zswap.5 ncollectd-zswap.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-zswap.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
