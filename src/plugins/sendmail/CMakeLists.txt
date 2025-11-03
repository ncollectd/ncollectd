plugin_option(PLUGIN_SENDMAIL "Sendmail statistics" ON)

set(BUILD_PLUGIN_SENDMAIL 0)

if(PLUGIN_SENDMAIL)
    set(BUILD_PLUGIN_SENDMAIL 1)
endif()

set(BUILD_PLUGIN_SENDMAIL ${BUILD_PLUGIN_SENDMAIL} PARENT_SCOPE)

if(BUILD_PLUGIN_SENDMAIL)
    set(PLUGIN_SENDMAIL_SRC sendmail.c)
    add_library(sendmail MODULE ${PLUGIN_SENDMAIL_SRC})
    target_link_libraries(sendmail PRIVATE libmetric libutils)
    set_target_properties(sendmail PROPERTIES PREFIX "")

#    add_executable(test_plugin_sendmail EXCLUDE_FROM_ALL sendmail_test.c ${PLUGIN_SENDMAIL_SRC})
#    target_link_libraries(test_plugin_sendmail libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_sendmail)
#    add_test(NAME test_plugin_sendmail COMMAND test_plugin_sendmail WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS sendmail DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-sendmail.5 ncollectd-sendmail.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-sendmail.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
