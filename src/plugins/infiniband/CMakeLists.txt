plugin_option(PLUGIN_INFINIBAND "Infiniband statistics" ON)

set(BUILD_PLUGIN_INFINIBAND 0)

if(PLUGIN_INFINIBAND)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_INFINIBAND 1)
    else()
        set(BUILD_PLUGIN_INFINIBAND_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_INFINIBAND ${BUILD_PLUGIN_INFINIBAND} PARENT_SCOPE)

if(BUILD_PLUGIN_INFINIBAND)
    set(PLUGIN_INFINIBAND_SRC infiniband.c)
    add_library(infiniband MODULE ${PLUGIN_INFINIBAND_SRC})
    target_link_libraries(infiniband PRIVATE libutils libmetric)
    set_target_properties(infiniband PROPERTIES PREFIX "")

#    add_executable(test_plugin_infiniband EXCLUDE_FROM_ALL infiniband_test.c ${PLUGIN_INFINIBAND_SRC})
#    target_link_libraries(test_plugin_infiniband libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_infiniband)
#    add_test(NAME test_plugin_infiniband COMMAND test_plugin_infiniband WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS infiniband DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-infiniband.5 ncollectd-infiniband.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-infiniband.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
