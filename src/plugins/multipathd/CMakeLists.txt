plugin_option(PLUGIN_MULTIPATHD "Multipathd statistics" ON)

set(BUILD_PLUGIN_MULTIPATHD 0)

if(PLUGIN_MULTIPATHD)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_MULTIPATHD 1)
    else()
        set(BUILD_PLUGIN_MULTIPATHD_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_MULTIPATHD ${BUILD_PLUGIN_MULTIPATHD} PARENT_SCOPE)

if(BUILD_PLUGIN_MULTIPATHD)
    set(PLUGIN_MULTIPATHD_SRC multipathd.c)
    add_library(multipathd MODULE ${PLUGIN_MULTIPATHD_SRC})
    target_link_libraries(multipathd PRIVATE libxson libmetric libutils)
    set_target_properties(multipathd PROPERTIES PREFIX "")

#    add_executable(test_plugin_multipathd EXCLUDE_FROM_ALL multipathd_test.c ${PLUGIN_MULTIPATHD_SRC})
#    target_link_libraries(test_plugin_multipathd libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_multipathd)
#    add_test(NAME test_plugin_multipathd COMMAND test_plugin_multipathd WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS multipathd DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-multipathd.5 ncollectd-multipathd.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-multipathd.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
