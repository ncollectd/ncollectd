plugin_option(PLUGIN_CONNTRACK "nf_conntrack statistics" ON)

set(BUILD_PLUGIN_CONNTRACK 0)

if(PLUGIN_CONNTRACK)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_CONNTRACK 1)
    else()
        set(BUILD_PLUGIN_CONNTRACK_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_CONNTRACK ${BUILD_PLUGIN_CONNTRACK} PARENT_SCOPE)

if(BUILD_PLUGIN_CONNTRACK)
    set(PLUGIN_CONNTRACK_SRC conntrack.c)
    add_library(conntrack MODULE ${PLUGIN_CONNTRACK_SRC})
    target_link_libraries(conntrack PRIVATE libmetric libutils)
    set_target_properties(conntrack PROPERTIES PREFIX "")

    add_executable(test_plugin_conntrack EXCLUDE_FROM_ALL conntrack_test.c ${PLUGIN_CONNTRACK_SRC})
    target_link_libraries(test_plugin_conntrack libtest libmetric libutils -lm)
    add_dependencies(build_tests test_plugin_conntrack)
    add_test(NAME test_plugin_conntrack COMMAND test_plugin_conntrack WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS conntrack DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-conntrack.5 ncollectd-conntrack.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-conntrack.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
