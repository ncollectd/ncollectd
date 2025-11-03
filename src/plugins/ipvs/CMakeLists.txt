plugin_option(PLUGIN_IPVS "IPVS connection statistics" ON)

include(CheckIncludeFile)

set(BUILD_PLUGIN_IPVS 0)

if(PLUGIN_IPVS)
    if(BUILD_LINUX)
        check_include_file(linux/ip_vs.h HAVE_LINUX_IP_VS_H)
        if(HAVE_LINUX_IP_VS_H)
            set(BUILD_PLUGIN_IPVS 1)
        else()
            set(BUILD_PLUGIN_IPVS_REASON "linux/ip_vs.h not found" PARENT_SCOPE)
        endif()
    else()
        set(BUILD_PLUGIN_IPVS_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_IPVS ${BUILD_PLUGIN_IPVS} PARENT_SCOPE)

if(BUILD_PLUGIN_IPVS)
    set(PLUGIN_IPVS_SRC ipvs.c)
    add_library(ipvs MODULE ${PLUGIN_IPVS_SRC})
    target_link_libraries(ipvs PRIVATE libmetric libutils)
    set_target_properties(ipvs PROPERTIES PREFIX "")

#    add_executable(test_plugin_ipvs EXCLUDE_FROM_ALL ipvs_test.c ${PLUGIN_IPVS_SRC})
#    target_link_libraries(test_plugin_ipvs libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_ipvs)
#    add_test(NAME test_plugin_ipvs COMMAND test_plugin_ipvs WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS ipvs DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-ipvs.5 ncollectd-ipvs.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-ipvs.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
