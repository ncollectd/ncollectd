plugin_option(PLUGIN_ZONEINFO "Zoneinfo statistics" ON)

set(BUILD_PLUGIN_ZONEINFO 0)

if(PLUGIN_ZONEINFO)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_ZONEINFO 1)
    else()
        set(BUILD_PLUGIN_ZONEINFO_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_ZONEINFO ${BUILD_PLUGIN_ZONEINFO} PARENT_SCOPE)

if(BUILD_PLUGIN_ZONEINFO)
    gperf_generate(zoneinfo.gperf "${CMAKE_CURRENT_BINARY_DIR}/zoneinfo.h" ZONEINFO_H)
    set(PLUGIN_ZONEINFO_SRC zoneinfo.c)
    add_library(zoneinfo MODULE ${PLUGIN_ZONEINFO_SRC} ${ZONEINFO_H})
    target_link_libraries(zoneinfo PRIVATE libmetric libutils)
    set_target_properties(zoneinfo PROPERTIES PREFIX "")

    add_executable(test_plugin_zoneinfo EXCLUDE_FROM_ALL zoneinfo_test.c ${PLUGIN_ZONEINFO_SRC})
    target_link_libraries(test_plugin_zoneinfo libtest libmetric libutils -lm)
    add_dependencies(build_tests test_plugin_zoneinfo)
    add_test(NAME test_plugin_zoneinfo COMMAND test_plugin_zoneinfo WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS zoneinfo DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-zoneinfo.5 ncollectd-zoneinfo.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-zoneinfo.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
