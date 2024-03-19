plugin_option(PLUGIN_XFRM "XFRM statistics" ON)

set(BUILD_PLUGIN_XFRM 0)

if(PLUGIN_XFRM)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_XFRM 1)
    else()
        set(BUILD_PLUGIN_XFRM_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_XFRM ${BUILD_PLUGIN_XFRM} PARENT_SCOPE)

if(BUILD_PLUGIN_XFRM)
    gperf_generate(xfrm_stat.gperf "${CMAKE_CURRENT_BINARY_DIR}/xfrm_stat.h" XFRM_STAT_H)
    set(PLUGIN_XFRM_SRC xfrm.c)
    add_library(xfrm MODULE ${PLUGIN_XFRM_SRC} ${XFRM_STAT_H})
    target_link_libraries(xfrm PRIVATE libutils libmetric)
    set_target_properties(xfrm PROPERTIES PREFIX "")

    add_executable(test_plugin_xfrm EXCLUDE_FROM_ALL xfrm_test.c ${PLUGIN_XFRM_SRC})
    target_link_libraries(test_plugin_xfrm libtest libutils libmetric)
    add_dependencies(build_tests test_plugin_xfrm)
    add_test(NAME test_plugin_xfrm COMMAND test_plugin_xfrm WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS xfrm DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-xfrm.5 ncollectd-xfrm.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-xfrm.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
