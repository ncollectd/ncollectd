plugin_option(PLUGIN_SIGROK "sigrok acquisition sources" ON)

set(BUILD_PLUGIN_SIGROK 0)

if(PLUGIN_SIGROK)
    find_package(LibSigrok)
    find_package(LibGlib2)
    if(LIBSIGROK_FOUND AND LIBGLIB2_FOUND)
        set(BUILD_PLUGIN_SIGROK 1)
    else()
        set(BUILD_PLUGIN_SIGROK_REASON "libsigrock or libglib2 not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_SIGROK ${BUILD_PLUGIN_SIGROK} PARENT_SCOPE)

if(BUILD_PLUGIN_SIGROK)
    set(PLUGIN_SIGROK_SRC sigrok.c)
    add_library(sigrok MODULE ${PLUGIN_SIGROK_SRC})
    target_link_libraries(sigrok PRIVATE libmetric libutils LibSigrok::LibSigrok LibGlib2::LibGlib2)
    set_target_properties(sigrok PROPERTIES PREFIX "")

#    add_executable(test_plugin_sigrok EXCLUDE_FROM_ALL sigrok_test.c ${PLUGIN_SIGROK_SRC})
#    target_link_libraries(test_plugin_sigrok libtest libmetric libutils m)
#    add_dependencies(build_tests test_plugin_sigrok)
#    add_test(NAME test_plugin_sigrok COMMAND test_plugin_sigrok WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS sigrok DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-sigrok.5 ncollectd-sigrok.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-sigrok.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
