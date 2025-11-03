plugin_option(PLUGIN_WIRELESS "Wireless statistics" ON)

set(BUILD_PLUGIN_WIRELESS 0)

if(PLUGIN_WIRELESS)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_WIRELESS 1)
    else()
        set(BUILD_PLUGIN_WIRELESS_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_WIRELESS ${BUILD_PLUGIN_WIRELESS} PARENT_SCOPE)

if(BUILD_PLUGIN_WIRELESS)
    set(PLUGIN_WIRELESS_SRC wireless.c)
    add_library(wireless MODULE ${PLUGIN_WIRELESS_SRC})
    target_link_libraries(wireless PRIVATE libmetric libutils)
    set_target_properties(wireless PROPERTIES PREFIX "")

#    add_executable(test_plugin_wireless EXCLUDE_FROM_ALL wireless_test.c ${PLUGIN_WIRELESS_SRC})
#    target_link_libraries(test_plugin_wireless libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_wireless)
#    add_test(NAME test_plugin_wireless COMMAND test_plugin_wireless WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS wireless DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-wireless.5 ncollectd-wireless.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-wireless.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
