plugin_option(PLUGIN_IPMI "IPMI sensor statistics" ON)

set(BUILD_PLUGIN_IPMI 0)

if(PLUGIN_IPMI)
    find_package(LibOpenipmi)
    if(LIBOPENIPMI_FOUND)
        set(BUILD_PLUGIN_IPMI 1)
    else()
        set(BUILD_PLUGIN_IPMI_REASON "libopenipmi not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_IPMI ${BUILD_PLUGIN_IPMI} PARENT_SCOPE)

if(BUILD_PLUGIN_IPMI)
    set(PLUGIN_IPMI_SRC ipmi.c)
    add_library(ipmi MODULE ${PLUGIN_IPMI_SRC})
    target_link_libraries(ipmi PRIVATE libmetric libutils LibOpenipmi::LibOpenipmi)
    set_target_properties(ipmi PROPERTIES PREFIX "")

#    add_executable(test_plugin_ipmi EXCLUDE_FROM_ALL ipmi_test.c ${PLUGIN_IPMI_SRC})
#    target_link_libraries(test_plugin_ipmi libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_ipmi)
#    add_test(NAME test_plugin_ipmi COMMAND test_plugin_ipmi WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS ipmi DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-ipmi.5 ncollectd-ipmi.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-ipmi.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
