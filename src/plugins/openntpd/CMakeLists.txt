plugin_option(PLUGIN_OPENNTPD "OpenNTPD statistics" ON)

set(BUILD_PLUGIN_OPENNTPD 0)

if(PLUGIN_OPENNTPD)
    set(BUILD_PLUGIN_OPENNTPD 1)
endif()

set(BUILD_PLUGIN_OPENNTPD ${BUILD_PLUGIN_OPENNTPD} PARENT_SCOPE)

if(BUILD_PLUGIN_OPENNTPD)
    set(PLUGIN_OPENNTPD_SRC openntpd.c imsg-buffer.c imsg.c imsg.h recallocarray.c freezero.c)
    add_library(openntpd MODULE ${PLUGIN_OPENNTPD_SRC})
    target_link_libraries(openntpd PRIVATE libmetric libutils)
    set_target_properties(openntpd PROPERTIES PREFIX "")

#    add_executable(test_plugin_openntpd EXCLUDE_FROM_ALL openntpd_test.c ${PLUGIN_OPENNTPD_SRC})
#    target_link_libraries(test_plugin_openntpd libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_openntpd)
#    add_test(NAME test_plugin_openntpd COMMAND test_plugin_openntpd WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS openntpd DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-openntpd.5 ncollectd-openntpd.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-openntpd.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
