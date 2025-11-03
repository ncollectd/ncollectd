plugin_option(PLUGIN_SLABINFO "Slab statistics" ON)

set(BUILD_PLUGIN_SLABINFO 0)

if(PLUGIN_SLABINFO)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_SLABINFO 1)
    else()
        set(BUILD_PLUGIN_SLABINFO_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_SLABINFO ${BUILD_PLUGIN_SLABINFO} PARENT_SCOPE)

if(BUILD_PLUGIN_SLABINFO)
    set(PLUGIN_SLABINFO_SRC slabinfo.c)
    add_library(slabinfo MODULE ${PLUGIN_SLABINFO_SRC})
    target_link_libraries(slabinfo PRIVATE libmetric libutils)
    set_target_properties(slabinfo PROPERTIES PREFIX "")

    add_executable(test_plugin_slabinfo EXCLUDE_FROM_ALL slabinfo_test.c ${PLUGIN_SLABINFO_SRC})
    target_link_libraries(test_plugin_slabinfo libtest libmetric libutils -lm)
    add_dependencies(build_tests test_plugin_slabinfo)
    add_test(NAME test_plugin_slabinfo COMMAND test_plugin_slabinfo WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS slabinfo DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-slabinfo.5 ncollectd-slabinfo.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-slabinfo.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
