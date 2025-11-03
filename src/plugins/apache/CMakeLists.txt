plugin_option(PLUGIN_APACHE "Apache httpd statistics" ON)

set(BUILD_PLUGIN_APACHE 0)

if(PLUGIN_APACHE)
    find_package(LibCurl)
    if(LIBCURL_FOUND)
        set(BUILD_PLUGIN_APACHE 1)
    else()
        set(BUILD_PLUGIN_APACHE_REASON "libcurl not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_APACHE ${BUILD_PLUGIN_APACHE} PARENT_SCOPE)

if(BUILD_PLUGIN_APACHE)
    set(PLUGIN_APACHE_SRC apache.c)
    add_library(apache MODULE ${PLUGIN_APACHE_SRC})
    target_link_libraries(apache PRIVATE libmetric libutils LibCurl::LibCurl)
    set_target_properties(apache PROPERTIES PREFIX "")

    add_executable(test_plugin_apache EXCLUDE_FROM_ALL apache_test.c ${PLUGIN_APACHE_SRC})
    target_link_libraries(test_plugin_apache libtest libmetric libutils LibCurl::LibCurl -lm -lpthread)
    add_dependencies(build_tests test_plugin_apache)
    add_test(NAME test_plugin_apache COMMAND test_plugin_apache WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS apache DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-apache.5 ncollectd-apache.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-apache.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
