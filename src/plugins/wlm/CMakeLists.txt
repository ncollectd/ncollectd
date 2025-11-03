plugin_option(PLUGIN_WLM "AIX Workload Manager statistics" ON)

include(CheckStructHasMember)

set(BUILD_PLUGIN_WLM 0)

if(PLUGIN_WLM)
    if(BUILD_AIX)
        find_package(LibWlm)
        if(LIBWLM_FOUND)
            set(BUILD_PLUGIN_WLM 1)
        else()
            set(BUILD_PLUGIN_WLM_REASON "wlm not found" PARENT_SCOPE)
        endif()
    else()
        set(BUILD_PLUGIN_WLM_REASON "AIX only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_WLM ${BUILD_PLUGIN_WLM} PARENT_SCOPE)

if(BUILD_PLUGIN_WLM)
    set(PLUGIN_WLM_SRC wlm.c)
    add_library(wlm MODULE ${PLUGIN_WLM_SRC})
    target_link_libraries(wlm PRIVATE libmetric libutils LibWlm::LibWlm)
    set_target_properties(wlm PROPERTIES PREFIX "")

#    add_executable(test_plugin_wlm EXCLUDE_FROM_ALL wlm_test.c ${PLUGIN_WLM_SRC})
#    target_link_libraries(test_plugin_wlm libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_wlm)
#    add_test(NAME test_plugin_wlm COMMAND test_plugin_wlm WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS wlm DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-wlm.5 ncollectd-wlm.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-wlm.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
