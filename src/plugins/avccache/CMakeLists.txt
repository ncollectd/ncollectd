plugin_option(PLUGIN_AVCCACHE "AVC cache statistics" ON)

set(BUILD_PLUGIN_AVCCACHE 0)

if(PLUGIN_AVCCACHE)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_AVCCACHE 1)
    else()
        set(BUILD_PLUGIN_AVCCACHE_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_AVCCACHE ${BUILD_PLUGIN_AVCCACHE} PARENT_SCOPE)

if(BUILD_PLUGIN_AVCCACHE)
    set(PLUGIN_AVCCACHE_SRC avccache.c)
    add_library(avccache MODULE ${PLUGIN_AVCCACHE_SRC})
    target_link_libraries(avccache PRIVATE libmetric libutils)
    set_target_properties(avccache PROPERTIES PREFIX "")

    add_executable(test_plugin_avccache EXCLUDE_FROM_ALL avccache_test.c ${PLUGIN_AVCCACHE_SRC})
    target_link_libraries(test_plugin_avccache libtest libmetric libutils -lm)
    add_dependencies(build_tests test_plugin_avccache)
    add_test(NAME test_plugin_avccache COMMAND test_plugin_avccache WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS avccache DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-avccache.5 ncollectd-avccache.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-avccache.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
