plugin_option(PLUGIN_RECURSOR "PowerDNS Recursor statistics" ON)

set(BUILD_PLUGIN_RECURSOR 0)

if(PLUGIN_RECURSOR)
    set(BUILD_PLUGIN_RECURSOR 1)
endif()

set(BUILD_PLUGIN_RECURSOR ${BUILD_PLUGIN_RECURSOR} PARENT_SCOPE)

if(BUILD_PLUGIN_RECURSOR)
    gperf_generate(recursor.gperf "${CMAKE_CURRENT_BINARY_DIR}/recursor.h" RECURSOR_H)
    set(PLUGIN_RECURSOR_SRC recursor.c)
    add_library(recursor MODULE ${PLUGIN_RECURSOR_SRC} ${RECURSOR_H})
    target_link_libraries(recursor PRIVATE libmetric libutils)
    set_target_properties(recursor PROPERTIES PREFIX "")

#    add_executable(test_plugin_recursor EXCLUDE_FROM_ALL recursor_test.c ${PLUGIN_RECURSOR_SRC})
#    target_link_libraries(test_plugin_recursor libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_recursor)
#    add_test(NAME test_plugin_recursor COMMAND test_plugin_recursor WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS recursor DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-recursor.5 ncollectd-recursor.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-recursor.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
