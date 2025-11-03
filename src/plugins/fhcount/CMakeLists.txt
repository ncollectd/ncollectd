plugin_option(PLUGIN_FHCOUNT "File handles statistics" ON)

check_function_exists(getfhcountavg HAVE_GETFHCOUNTAVG)

set(BUILD_PLUGIN_FHCOUNT 0)

if(PLUGIN_FHCOUNT)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_FHCOUNT 1)
    else()
        set(BUILD_PLUGIN_FHCOUNT_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_FHCOUNT ${BUILD_PLUGIN_FHCOUNT} PARENT_SCOPE)

if(BUILD_PLUGIN_FHCOUNT)
    set(PLUGIN_FHCOUNT_SRC fhcount.c)
    add_library(fhcount MODULE ${PLUGIN_FHCOUNT_SRC})
    target_link_libraries(fhcount PRIVATE libmetric libutils)
    set_target_properties(fhcount PROPERTIES PREFIX "")

    add_executable(test_plugin_fhcount EXCLUDE_FROM_ALL fhcount_test.c ${PLUGIN_FHCOUNT_SRC})
    target_link_libraries(test_plugin_fhcount libtest libmetric libutils)
    add_dependencies(build_tests test_plugin_fhcount)
    add_test(NAME test_plugin_fhcount COMMAND test_plugin_fhcount WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS fhcount DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-fhcount.5 ncollectd-fhcount.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-fhcount.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
