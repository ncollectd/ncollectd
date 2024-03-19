plugin_option(PLUGIN_CPUFREQ "CPU frequency statistics" ON)

set(BUILD_PLUGIN_CPUFREQ 0)

if(PLUGIN_CPUFREQ)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_CPUFREQ 1)
    elseif(BUILD_FREBSD)
        set(BUILD_PLUGIN_CPUFREQ 1)
    else()
        set(BUILD_PLUGIN_CPUFREQ_REASON "unsupported system" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_CPUFREQ ${BUILD_PLUGIN_CPUFREQ} PARENT_SCOPE)

if(BUILD_PLUGIN_CPUFREQ)
    set(PLUGIN_CPUFREQ_SRC cpufreq.c)
    add_library(cpufreq MODULE ${PLUGIN_CPUFREQ_SRC})
    target_link_libraries(cpufreq PRIVATE libutils libmetric)
    set_target_properties(cpufreq PROPERTIES PREFIX "")

#    add_executable(test_plugin_cpufreq EXCLUDE_FROM_ALL cpufreq_test.c ${PLUGIN_CPUFREQ_SRC})
#    target_link_libraries(test_plugin_cpufreq libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_cpufreq)
#    add_test(NAME test_plugin_cpufreq COMMAND test_plugin_cpufreq WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS cpufreq DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-cpufreq.5 ncollectd-cpufreq.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-cpufreq.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
