plugin_option(PLUGIN_SYNPROXY "Synproxy stats plugin" ON)

set(BUILD_PLUGIN_SYNPROXY 0)

if(PLUGIN_SYNPROXY)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_SYNPROXY 1)
    else()
        set(BUILD_PLUGIN_SYNPROXY_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_SYNPROXY ${BUILD_PLUGIN_SYNPROXY} PARENT_SCOPE)

if(BUILD_PLUGIN_SYNPROXY)
    set(PLUGIN_SYNPROXY_SRC synproxy.c)
    add_library(synproxy MODULE ${PLUGIN_SYNPROXY_SRC})
    target_link_libraries(synproxy PRIVATE libutils libmetric)
    set_target_properties(synproxy PROPERTIES PREFIX "")

#    add_executable(test_plugin_synproxy EXCLUDE_FROM_ALL synproxy_test.c ${PLUGIN_SYNPROXY_SRC})
#    target_link_libraries(test_plugin_synproxy libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_synproxy)
#    add_test(NAME test_plugin_synproxy COMMAND test_plugin_synproxy WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS synproxy DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-synproxy.5 ncollectd-synproxy.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-synproxy.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
