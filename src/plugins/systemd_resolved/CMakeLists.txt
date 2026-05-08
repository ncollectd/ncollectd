plugin_option(PLUGIN_SYSTEMD_RESOLVED "Systemd-resolved stats" ON)

set(BUILD_PLUGIN_SYSTEMD_RESOLVED 0)

if(PLUGIN_SYSTEMD_RESOLVED)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_SYSTEMD_RESOLVED 1)
    endif()
endif()

set(BUILD_PLUGIN_SYSTEMD_RESOLVED ${BUILD_PLUGIN_SYSTEMD_RESOLVED} PARENT_SCOPE)

if(BUILD_PLUGIN_SYSTEMD_RESOLVED)
    gperf_generate(sd_resolved.gperf "${CMAKE_CURRENT_BINARY_DIR}/sd_resolved.h" SD_RESOLVE_H)
    set(PLUGIN_SYSTEMD_RESOLVED_SRC sd_resolved.c ${SD_RESOLVE_H})
    add_library(systemd_resolved MODULE ${PLUGIN_SYSTEMD_RESOLVED_SRC})
    target_link_libraries(systemd_resolved PRIVATE libmetric libutils libxson)
    set_target_properties(systemd_resolved PROPERTIES PREFIX "")

#    add_executable(test_plugin_sd_resolved EXCLUDE_FROM_ALL sd_resolved_linux_test.c ${PLUGIN_SYSTEMD_RESOLVED_SRC})
#    target_link_libraries(test_plugin_sd_resolved libtest libmetric libutils libxson -lm)
#    add_dependencies(build_tests test_plugin_sd_resolved)
#    add_test(NAME test_plugin_sd_resolved COMMAND test_plugin_sd_resolved WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS systemd_resolved DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-systemd_resolved.5 ncollectd-systemd_resolved.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-systemd_resolved.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
