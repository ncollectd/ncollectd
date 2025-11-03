plugin_option(PLUGIN_FSCACHE "fscache statistics" ON)

set(BUILD_PLUGIN_FSCACHE 0)

if(PLUGIN_FSCACHE)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_FSCACHE 1)
    else()
        set(BUILD_PLUGIN_FSCACHE_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_FSCACHE ${BUILD_PLUGIN_FSCACHE} PARENT_SCOPE)

if(BUILD_PLUGIN_FSCACHE)
    gperf_generate(fscache.gperf "${CMAKE_CURRENT_BINARY_DIR}/fscache.h" FSCACHE_H)
    set(PLUGIN_FSCACHE_SRC fscache.c)
    add_library(fscache MODULE ${PLUGIN_FSCACHE_SRC} ${FSCACHE_H})
    target_link_libraries(fscache PRIVATE libmetric libutils)
    set_target_properties(fscache PROPERTIES PREFIX "")

#    add_executable(test_plugin_fscache EXCLUDE_FROM_ALL fscache_test.c ${PLUGIN_FSCACHE_SRC})
#    target_link_libraries(test_plugin_fscache libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_fscache)
#    add_test(NAME test_plugin_fscache COMMAND test_plugin_fscache WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS fscache DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-fscache.5 ncollectd-fscache.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-fscache.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
