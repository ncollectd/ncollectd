plugin_option(PLUGIN_UBI "UBIFS statistics" ON)

set(BUILD_PLUGIN_UBI 0)

if(PLUGIN_UBI)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_UBI 1)
    else()
        set(BUILD_PLUGIN_UBI_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_UBI ${BUILD_PLUGIN_UBI} PARENT_SCOPE)

if(BUILD_PLUGIN_UBI)
    set(PLUGIN_UBI_SRC ubi.c)
    add_library(ubi MODULE ${PLUGIN_UBI_SRC})
    target_link_libraries(ubi PRIVATE libmetric libutils)
    set_target_properties(ubi PROPERTIES PREFIX "")

#    add_executable(test_plugin_ubi EXCLUDE_FROM_ALL ubi_test.c ${PLUGIN_UBI_SRC})
#    target_link_libraries(test_plugin_ubi libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_ubi)
#    add_test(NAME test_plugin_ubi COMMAND test_plugin_ubi WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS ubi DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-ubi.5 ncollectd-ubi.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-ubi.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
