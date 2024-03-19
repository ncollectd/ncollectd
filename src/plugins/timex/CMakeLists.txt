plugin_option(PLUGIN_TIMEX "Host clock statistics" ON)

set(BUILD_PLUGIN_TIMEX 0)

if(PLUGIN_TIMEX)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_TIMEX 1)
    else()
        set(BUILD_PLUGIN_TIMEX_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_TIMEX ${BUILD_PLUGIN_TIMEX} PARENT_SCOPE)

if(BUILD_PLUGIN_TIMEX)
    set(PLUGIN_TIMEX_SRC timex.c)
    add_library(timex MODULE ${PLUGIN_TIMEX_SRC})
    target_link_libraries(timex PRIVATE libutils libmetric)
    set_target_properties(timex PROPERTIES PREFIX "")

#    add_executable(test_plugin_timex EXCLUDE_FROM_ALL timex_test.c ${PLUGIN_TIMEX_SRC})
#    target_link_libraries(test_plugin_timex libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_timex)
#    add_test(NAME test_plugin_timex COMMAND test_plugin_timex WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS timex DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-timex.5 ncollectd-timex.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-timex.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
