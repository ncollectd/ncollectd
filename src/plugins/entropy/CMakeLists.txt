plugin_option(PLUGIN_ENTROPY "Entropy statistics" ON)

include(CheckIncludeFile)

set(BUILD_PLUGIN_ENTROPY 0)

if(PLUGIN_ENTROPY)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_ENTROPY 1)
    elseif(BUILD_NETBSD)
        check_include_file(sys/rndio.h HAVE_SYS_RNDIO_H)
        if(HAVE_SYS_RNDIO_H)
            set(BUILD_PLUGIN_ENTROPY 1)
        else()
            set(BUILD_PLUGIN_ENTROPY_REASON "not found sys/rndio.h" PARENT_SCOPE)
        endif()
    else()
        set(BUILD_PLUGIN_ENTROPY_REASON "unsupported system" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_ENTROPY ${BUILD_PLUGIN_ENTROPY} PARENT_SCOPE)

if(BUILD_PLUGIN_ENTROPY)
    set(PLUGIN_ENTROPY_SRC entropy.c)
    add_library(entropy MODULE ${PLUGIN_ENTROPY_SRC})

    if(BUILD_NETBSD)
        target_compile_definitions(entropy PUBLIC HAVE_SYS_RNDIO_H)
    endif()

    target_link_libraries(entropy PRIVATE libutils libmetric)
    set_target_properties(entropy PROPERTIES PREFIX "")

    if(BUILD_LINUX)
        add_executable(test_plugin_entropy EXCLUDE_FROM_ALL entropy_linux_test.c ${PLUGIN_ENTROPY_SRC})
        target_link_libraries(test_plugin_entropy libtest libutils libmetric m)
        add_dependencies(build_tests test_plugin_entropy)
        add_test(NAME test_plugin_entropy COMMAND test_plugin_entropy WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    endif()

    install(TARGETS entropy DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-entropy.5 ncollectd-entropy.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-entropy.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
