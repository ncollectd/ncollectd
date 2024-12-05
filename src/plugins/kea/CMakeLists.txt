plugin_option(PLUGIN_KEA "Kea statistics" ON)

set(BUILD_PLUGIN_KEA 0)

if(PLUGIN_KEA)
    set(BUILD_PLUGIN_KEA 1)
endif()

set(BUILD_PLUGIN_KEA ${BUILD_PLUGIN_KEA} PARENT_SCOPE)

if(BUILD_PLUGIN_KEA)
    gperf_generate(kea_stats.gperf "${CMAKE_CURRENT_BINARY_DIR}/kea_stats.h" KEA_STATS_H)
    set(PLUGIN_KEA_SRC kea.c ${KEA_STATS_H})
    add_library(kea MODULE ${PLUGIN_KEA_SRC})
    target_link_libraries(kea PRIVATE libutils libmetric libxson)
    set_target_properties(kea PROPERTIES PREFIX "")

    add_executable(test_plugin_kea EXCLUDE_FROM_ALL kea_test.c ${PLUGIN_KEA_SRC})
    target_link_libraries(test_plugin_kea libtest libutils libmetric libxson -lm -lpthread)
    add_dependencies(build_tests test_plugin_kea)
    add_test(NAME test_plugin_kea COMMAND test_plugin_kea WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS kea DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-kea.5 ncollectd-kea.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-kea.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
