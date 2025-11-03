plugin_option(PLUGIN_SOFTIRQ "softirq statistics" ON)

set(BUILD_PLUGIN_SOFTIRQ 0)

if(PLUGIN_SOFTIRQ)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_SOFTIRQ 1)
    else()
        set(BUILD_PLUGIN_SOFTIRQ_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_SOFTIRQ ${BUILD_PLUGIN_SOFTIRQ} PARENT_SCOPE)

if(BUILD_PLUGIN_SOFTIRQ)
    set(PLUGIN_SOFTIRQ_SRC softirq.c)
    add_library(softirq MODULE ${PLUGIN_SOFTIRQ_SRC})
    target_link_libraries(softirq PRIVATE libmetric libutils)
    set_target_properties(softirq PROPERTIES PREFIX "")

    add_executable(test_plugin_softirq EXCLUDE_FROM_ALL softirq_test.c ${PLUGIN_SOFTIRQ_SRC})
    target_link_libraries(test_plugin_softirq libtest libmetric libutils -lm)
    add_dependencies(build_tests test_plugin_softirq)
    add_test(NAME test_plugin_softirq COMMAND test_plugin_softirq WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS softirq DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-softirq.5 ncollectd-softirq.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-softirq.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
