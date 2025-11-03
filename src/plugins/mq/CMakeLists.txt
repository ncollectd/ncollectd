plugin_option(PLUGIN_MQ "IBM MQ stastistics" ON)

set(BUILD_PLUGIN_MQ 0)

if(PLUGIN_MQ)
    find_package(LibMq)
    if (LIBMQ_FOUND)
        set(BUILD_PLUGIN_MQ 1)
    else()
        set(BUILD_PLUGIN_MQ_REASON "mq libs not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_MQ ${BUILD_PLUGIN_MQ} PARENT_SCOPE)

if(BUILD_PLUGIN_MQ)
    set(PLUGIN_MQ_SRC mq.c)
    add_library(mq MODULE ${PLUGIN_MQ_SRC})
    target_link_libraries(mq PRIVATE libmetric libutils LibMq::LibMq)
    set_target_properties(mq PROPERTIES PREFIX "")

#    add_executable(test_plugin_mq EXCLUDE_FROM_ALL mq_test.c ${PLUGIN_MQ_SRC})
#    target_link_libraries(test_plugin_mq libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_mq)
#    add_test(NAME test_plugin_mq COMMAND test_plugin_mq WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS mq DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-mq.5 ncollectd-mq.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-mq.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
