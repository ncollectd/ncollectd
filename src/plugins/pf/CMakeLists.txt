plugin_option(PLUGIN_PF "BSD packet filter (PF) statistics" ON)

include(CheckIncludeFiles)

set(BUILD_PLUGIN_PF 0)

if(PLUGIN_PF)
    check_include_files("net/if.h;net/pfvar.h" HAVE_NET_PFVAR_H)
    if(BUILD_BSD OR BUILD_DARWIN)
        if(HAVE_NET_PFVAR_H)
            set(BUILD_PLUGIN_PF 1)
        else()
            set(BUILD_PLUGIN_PF_REASON "net/pfvar.h not found" PARENT_SCOPE)
        endif()
    else()
        set(BUILD_PLUGIN_PF_REASON "unsupported system" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_PF ${BUILD_PLUGIN_PF} PARENT_SCOPE)

if(BUILD_PLUGIN_PF)
    set(PLUGIN_PF_SRC pf.c)
    add_library(pf MODULE ${PLUGIN_PF_SRC})
    target_link_libraries(pf PRIVATE libutils libmetric)
    set_target_properties(pf PROPERTIES PREFIX "")

#    add_executable(test_plugin_pf EXCLUDE_FROM_ALL pf_test.c ${PLUGIN_PF_SRC})
#    target_link_libraries(test_plugin_pf libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_pf)
#    add_test(NAME test_plugin_pf COMMAND test_plugin_pf WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS pf DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-pf.5 ncollectd-pf.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-pf.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
