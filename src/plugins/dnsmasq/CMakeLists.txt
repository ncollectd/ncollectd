plugin_option(PLUGIN_DNSMASQ "dnsmaq statistics" ON)

set(BUILD_PLUGIN_DNSMASQ 0)

if(PLUGIN_DNSMASQ)
    set(BUILD_PLUGIN_DNSMASQ 1)
endif()

set(BUILD_PLUGIN_DNSMASQ ${BUILD_PLUGIN_DNSMASQ} PARENT_SCOPE)

if(BUILD_PLUGIN_DNSMASQ)
    set(PLUGIN_DNSMASQ_SRC dnsmasq.c)
    add_library(dnsmasq MODULE ${PLUGIN_DNSMASQ_SRC})
    target_link_libraries(dnsmasq PRIVATE libmetric libutils)
    set_target_properties(dnsmasq PROPERTIES PREFIX "")

#    add_executable(test_plugin_dnsmasq EXCLUDE_FROM_ALL dnsmasq_test.c ${PLUGIN_DNSMASQ_SRC})
#    target_link_libraries(test_plugin_dnsmasq libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_dnsmasq)
#    add_test(NAME test_plugin_dnsmasq COMMAND test_plugin_dnsmasq WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS dnsmasq DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-dnsmasq.5 ncollectd-dnsmasq.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-dnsmasq.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
