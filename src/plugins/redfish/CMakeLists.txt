plugin_option(PLUGIN_REDFISH "Redfish statistics" ON)

set(BUILD_PLUGIN_REDFISH 0)

if(PLUGIN_REDFISH)
    find_package(LibCurl)
    if(LIBCURL_FOUND)
        if (LIBCURL_VERSION VERSION_GREATER_EQUAL "7.66.0")
            set(BUILD_PLUGIN_REDFISH 1)
        else()
            set(BUILD_PLUGIN_REDFISH_REASON "libcurl less than 7.66.0" PARENT_SCOPE)
        endif()
    else()
        set(BUILD_PLUGIN_REDFISH_REASON "libcurl not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_REDFISH ${BUILD_PLUGIN_REDFISH} PARENT_SCOPE)

if(BUILD_PLUGIN_REDFISH)
    set(PLUGIN_REDFISH_SRC redfish.c)
    add_library(redfish MODULE ${PLUGIN_REDFISH_SRC})
    target_link_libraries(redfish PRIVATE libmetric libutils libxson LibCurl::LibCurl)
    set_target_properties(redfish PROPERTIES PREFIX "")

    add_executable(test_plugin_redfish EXCLUDE_FROM_ALL redfish_test.c ${PLUGIN_REDFISH_SRC})
    target_link_libraries(test_plugin_redfish libtest libconfig libmetric libutils libxson LibCurl::LibCurl -lm -lpthread)
    add_dependencies(build_tests test_plugin_redfish)
    add_test(NAME test_plugin_redfish COMMAND test_plugin_redfish WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS redfish DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-redfish.5 ncollectd-redfish.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-redfish.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
