plugin_option(PLUGIN_FCHOST "Fibre Channel statistics" ON)

set(BUILD_PLUGIN_FCHOST 0)

if(PLUGIN_FCHOST)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_FCHOST 1)
    else()
        set(BUILD_PLUGIN_FCHOST_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_FCHOST ${BUILD_PLUGIN_FCHOST} PARENT_SCOPE)

if(BUILD_PLUGIN_FCHOST)
    set(PLUGIN_FCHOST_SRC fchost.c)
    add_library(fchost MODULE ${PLUGIN_FCHOST_SRC})
    target_link_libraries(fchost PRIVATE libmetric libutils)
    set_target_properties(fchost PROPERTIES PREFIX "")

#    add_executable(test_plugin_fchost EXCLUDE_FROM_ALL fchost_test.c ${PLUGIN_FCHOST_SRC})
#    target_link_libraries(test_plugin_fchost libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_fchost)
#    add_test(NAME test_plugin_fchost COMMAND test_plugin_fchost WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS fchost DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-fchost.5 ncollectd-fchost.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-fchost.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
