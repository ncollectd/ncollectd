plugin_option(PLUGIN_CORETEMP "Coretemp stats" ON)

include(CheckIncludeFile)

set(BUILD_PLUGIN_CORETEMP 0)

if(PLUGIN_CORETEMP)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_CORETEMP 1)
    else()
        set(BUILD_PLUGIN_CORETEMP_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_CORETEMP ${BUILD_PLUGIN_CORETEMP} PARENT_SCOPE)

if(BUILD_PLUGIN_CORETEMP)
    set(PLUGIN_CORETEMP_SRC coretemp.c)
    add_library(coretemp MODULE ${PLUGIN_CORETEMP_SRC})
    target_link_libraries(coretemp PRIVATE libmetric libutils)
    set_target_properties(coretemp PROPERTIES PREFIX "")

    add_executable(test_plugin_coretemp EXCLUDE_FROM_ALL coretemp_test.c ${PLUGIN_CORETEMP_SRC})
    target_link_libraries(test_plugin_coretemp libtest libmetric libutils -lm)
    add_dependencies(build_tests test_plugin_coretemp)
    add_test(NAME test_plugin_coretemp COMMAND test_plugin_coretemp WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS coretemp DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-coretemp.5 ncollectd-coretemp.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-coretemp.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
