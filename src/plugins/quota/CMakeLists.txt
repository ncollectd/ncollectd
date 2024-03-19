plugin_option(PLUGIN_QUOTA "Quota statistis" ON)

set(BUILD_PLUGIN_QUOTA 0)

if(PLUGIN_QUOTA)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_QUOTA 1)
    else()
        set(BUILD_PLUGIN_QUOTA_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_QUOTA ${BUILD_PLUGIN_QUOTA} PARENT_SCOPE)

if(BUILD_PLUGIN_QUOTA)
    set(PLUGIN_QUOTA_SRC quota.c)
    add_library(quota MODULE ${PLUGIN_QUOTA_SRC})
    target_link_libraries(quota PRIVATE libutils libmetric)
    set_target_properties(quota PROPERTIES PREFIX "")

#    add_executable(test_plugin_quota EXCLUDE_FROM_ALL quota_test.c ${PLUGIN_QUOTA_SRC})
#    target_link_libraries(test_plugin_quota libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_quota)
#    add_test(NAME test_plugin_quota COMMAND test_plugin_quota WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS quota DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-quota.5 ncollectd-quota.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-quota.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
