plugin_option(PLUGIN_HUGEPAGES "Hugepages statistics" ON)

set(BUILD_PLUGIN_HUGEPAGES 0)

if(PLUGIN_HUGEPAGES)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_HUGEPAGES 1)
    else()
        set(BUILD_PLUGIN_HUGEPAGES_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_HUGEPAGES ${BUILD_PLUGIN_HUGEPAGES} PARENT_SCOPE)

if(BUILD_PLUGIN_HUGEPAGES)
    set(PLUGIN_HUGEPAGES_SRC hugepages.c)
    add_library(hugepages MODULE ${PLUGIN_HUGEPAGES_SRC})
    target_link_libraries(hugepages PRIVATE libmetric libutils)
    set_target_properties(hugepages PROPERTIES PREFIX "")

    add_executable(test_plugin_hugepages EXCLUDE_FROM_ALL hugepages_test.c ${PLUGIN_HUGEPAGES_SRC})
    target_link_libraries(test_plugin_hugepages libtest libmetric libutils -lm)
    add_dependencies(build_tests test_plugin_hugepages)
    add_test(NAME test_plugin_hugepages COMMAND test_plugin_hugepages WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS hugepages DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-hugepages.5 ncollectd-hugepages.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-hugepages.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
