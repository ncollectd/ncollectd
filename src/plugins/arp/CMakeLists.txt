plugin_option(PLUGIN_ARP "ARP statistics" ON)

set(BUILD_PLUGIN_ARP 0)

if(PLUGIN_ARP)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_ARP 1)
    else()
        set(BUILD_PLUGIN_ARP_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_ARP ${BUILD_PLUGIN_ARP} PARENT_SCOPE)

if(BUILD_PLUGIN_ARP)
    set(PLUGIN_ARP_SRC arp.c)
    add_library(arp MODULE ${PLUGIN_ARP_SRC})
    target_link_libraries(arp PRIVATE libmetric libutils)
    set_target_properties(arp PROPERTIES PREFIX "")

    add_executable(test_plugin_arp EXCLUDE_FROM_ALL arp_test.c ${PLUGIN_ARP_SRC})
    target_link_libraries(test_plugin_arp libtest libmetric libutils -lm)
    add_dependencies(build_tests test_plugin_arp)
    add_test(NAME test_plugin_arp COMMAND test_plugin_arp WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS arp DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-arp.5 ncollectd-arp.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-arp.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
