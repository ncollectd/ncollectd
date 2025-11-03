plugin_option(PLUGIN_NGINX "Nginx statistics" ON)

set(BUILD_PLUGIN_NGINX 0)

if(PLUGIN_NGINX)
    find_package(LibCurl)
    if(LIBCURL_FOUND)
        set(BUILD_PLUGIN_NGINX 1)
    else()
        set(BUILD_PLUGIN_NGINX_REASON "libcurl not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_NGINX ${BUILD_PLUGIN_NGINX} PARENT_SCOPE)

if(BUILD_PLUGIN_NGINX)
    set(PLUGIN_NGINX_SRC nginx.c)
    add_library(nginx MODULE ${PLUGIN_NGINX_SRC})
    target_link_libraries(nginx PRIVATE libmetric libutils LibCurl::LibCurl)
    set_target_properties(nginx PROPERTIES PREFIX "")

    add_executable(test_plugin_nginx EXCLUDE_FROM_ALL nginx_test.c ${PLUGIN_NGINX_SRC})
    target_link_libraries(test_plugin_nginx libtest libmetric libutils LibCurl::LibCurl -lm -lpthread)
    add_dependencies(build_tests test_plugin_nginx)
    add_test(NAME test_plugin_nginx COMMAND test_plugin_nginx WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS nginx DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-nginx.5 ncollectd-nginx.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-nginx.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
