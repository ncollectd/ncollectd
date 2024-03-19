plugin_option(PLUGIN_BUDDYINFO "buddyinfo statistics" ON)

set(BUILD_PLUGIN_BUDDYINFO 0)

if(PLUGIN_BUDDYINFO)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_BUDDYINFO 1)
    else()
        set(BUILD_PLUGIN_BUDDYINFO_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_BUDDYINFO ${BUILD_PLUGIN_BUDDYINFO} PARENT_SCOPE)

if(BUILD_PLUGIN_BUDDYINFO)
    set(PLUGIN_BUDDYINFO_SRC buddyinfo.c)
    add_library(buddyinfo MODULE ${PLUGIN_BUDDYINFO_SRC})
    target_link_libraries(buddyinfo PRIVATE libutils libmetric)
    set_target_properties(buddyinfo PROPERTIES PREFIX "")

    add_executable(test_plugin_buddyinfo EXCLUDE_FROM_ALL buddyinfo_test.c ${PLUGIN_BUDDYINFO_SRC})
    target_link_libraries(test_plugin_buddyinfo libtest libutils libmetric -lm)
    add_dependencies(build_tests test_plugin_buddyinfo)
    add_test(NAME test_plugin_buddyinfo COMMAND test_plugin_buddyinfo WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS buddyinfo DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-buddyinfo.5 ncollectd-buddyinfo.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-buddyinfo.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
