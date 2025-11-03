plugin_option(PLUGIN_LOG_SYSLOG "Syslog logging plugin" ON)

include(CheckFunctionExists)
include(CheckIncludeFile)

set(BUILD_PLUGIN_LOG_SYSLOG 0)

if(PLUGIN_LOG_SYSLOG)
    check_include_file(syslog.h HAVE_SYSLOG_H)
    check_function_exists(syslog HAVE_SYSLOG)
    if(HAVE_SYSLOG)
        set(BUILD_PLUGIN_LOG_SYSLOG 1)
    else()
        set(BUILD_PLUGIN_LOG_SYSLOG_REASON "syslog not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_LOG_SYSLOG ${BUILD_PLUGIN_LOG_SYSLOG} PARENT_SCOPE)

if(BUILD_PLUGIN_LOG_SYSLOG)
    set(PLUGIN_LOG_SYSLOG_SRC log_syslog.c)
    add_library(log_syslog MODULE ${PLUGIN_LOG_SYSLOG_SRC})
    if(HAVE_SYSLOG_H)
        target_compile_definitions(log_syslog PUBLIC HAVE_SYSLOG_H)
    endif()
    target_link_libraries(log_syslog PRIVATE libmetric libutils)
    set_target_properties(log_syslog PROPERTIES PREFIX "")

#    add_executable(test_plugin_log_syslog EXCLUDE_FROM_ALL log_syslog_test.c ${PLUGIN_LOG_SYSLOG_SRC})
#    target_link_libraries(test_plugin_log_syslog libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_log_syslog)
#    add_test(NAME test_plugin_log_syslog COMMAND test_plugin_log_syslog WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS log_syslog DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-log_syslog.5 ncollectd-log_syslog.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-log_syslog.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
