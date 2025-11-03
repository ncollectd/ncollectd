plugin_option(PLUGIN_ZRAM "ZRam statistics" ON)

set(BUILD_PLUGIN_ZRAM 0)

if(PLUGIN_ZRAM)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_ZRAM 1)
    else()
        set(BUILD_PLUGIN_ZRAM_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_ZRAM ${BUILD_PLUGIN_ZRAM} PARENT_SCOPE)

if(BUILD_PLUGIN_ZRAM)
    set(PLUGIN_ZRAM_SRC zram.c)
    add_library(zram MODULE ${PLUGIN_ZRAM_SRC})
    target_link_libraries(zram PRIVATE libmetric libutils)
    set_target_properties(zram PROPERTIES PREFIX "")

#    add_executable(test_plugin_zram EXCLUDE_FROM_ALL zram_test.c ${PLUGIN_ZRAM_SRC})
#    target_link_libraries(test_plugin_zram libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_zram)
#    add_test(NAME test_plugin_zram COMMAND test_plugin_zram WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS zram DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-zram.5 ncollectd-zram.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-zram.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
