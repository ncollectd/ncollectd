plugin_option(PLUGIN_NTPD "NTPd statistics" ON)

set(BUILD_PLUGIN_NTPD 0)

if(PLUGIN_NTPD)
    set(BUILD_PLUGIN_NTPD 1)
endif()

set(BUILD_PLUGIN_NTPD ${BUILD_PLUGIN_NTPD} PARENT_SCOPE)

if(BUILD_PLUGIN_NTPD)
    set(PLUGIN_NTPD_SRC ntpd.c)
    add_library(ntpd MODULE ${PLUGIN_NTPD_SRC})
    target_link_libraries(ntpd PRIVATE libmetric libutils)
    set_target_properties(ntpd PROPERTIES PREFIX "")

#    add_executable(test_plugin_ntpd EXCLUDE_FROM_ALL ntpd_test.c ${PLUGIN_NTPD_SRC})
#    target_link_libraries(test_plugin_ntpd libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_ntpd)
#    add_test(NAME test_plugin_ntpd COMMAND test_plugin_ntpd WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS ntpd DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-ntpd.5 ncollectd-ntpd.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-ntpd.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
