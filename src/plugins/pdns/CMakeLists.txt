plugin_option(PLUGIN_PDNS "PowerDNS server statistics" ON)

set(BUILD_PLUGIN_PDNS 0)

if(PLUGIN_PDNS)
    set(BUILD_PLUGIN_PDNS 1)
endif()

set(BUILD_PLUGIN_PDNS ${BUILD_PLUGIN_PDNS} PARENT_SCOPE)

if(BUILD_PLUGIN_PDNS)
    gperf_generate(pdns.gperf "${CMAKE_CURRENT_BINARY_DIR}/pdns.h" PDNS_H)
    set(PLUGIN_PDNS_SRC pdns.c)
    add_library(pdns MODULE ${PLUGIN_PDNS_SRC} ${PDNS_H})
    target_link_libraries(pdns PRIVATE libmetric libutils)
    set_target_properties(pdns PROPERTIES PREFIX "")

#    add_executable(test_plugin_pdns EXCLUDE_FROM_ALL pdns_test.c ${PLUGIN_PDNS_SRC})
#    target_link_libraries(test_plugin_pdns libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_pdns)
#    add_test(NAME test_plugin_pdns COMMAND test_plugin_pdns WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS pdns DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-pdns.5 ncollectd-pdns.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-pdns.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
