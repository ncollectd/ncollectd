plugin_option(PLUGIN_ATS "Apache Traffic Server statistics" ON)

set(BUILD_PLUGIN_ATS 0)

if(PLUGIN_ATS)
    find_package(LibCurl)
    if(LIBCURL_FOUND)
        set(BUILD_PLUGIN_ATS 1)
    else()
        set(BUILD_PLUGIN_ATS_REASON "libcurl not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_ATS ${BUILD_PLUGIN_ATS} PARENT_SCOPE)

if(BUILD_PLUGIN_ATS)
    gperf_generate(ats.gperf "${CMAKE_CURRENT_BINARY_DIR}/ats.h" ATS_H)
    set(PLUGIN_ATS_SRC ats.c ${ATS_H})
    add_library(ats MODULE ${PLUGIN_ATS_SRC})
    target_link_libraries(ats PRIVATE libmetric libutils libxson LibCurl::LibCurl)
    set_target_properties(ats PROPERTIES PREFIX "")

    add_executable(test_plugin_ats EXCLUDE_FROM_ALL ats_test.c ${PLUGIN_ATS_SRC})
    target_link_libraries(test_plugin_ats libtest libmetric libutils libxson LibCurl::LibCurl -lm -lpthread)
    add_dependencies(build_tests test_plugin_ats)
    add_test(NAME test_plugin_ats COMMAND test_plugin_ats WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS ats DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-ats.5 ncollectd-ats.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-ats.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
