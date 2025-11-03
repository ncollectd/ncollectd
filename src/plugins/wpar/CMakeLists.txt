plugin_option(PLUGIN_WPAR "AIX Workload Partitions statistics" ON)

set(BUILD_PLUGIN_WPAR 0)

if(PLUGIN_WPAR)
    if(BUILD_AIX)
        find_package(LibPerfstat)
        if(LIBPERFSTAT_FOUND)
            set(BUILD_PLUGIN_WPAR 1)
        else()
            set(BUILD_PLUGIN_WPAR_REASON "perfstat not found" PARENT_SCOPE)
        endif()
    else()
        set(BUILD_PLUGIN_WPAR_REASON "AIX only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_WPAR ${BUILD_PLUGIN_WPAR} PARENT_SCOPE)

if(BUILD_PLUGIN_WPAR)
    set(PLUGIN_WPAR_SRC wpar.c)
    add_library(wpar MODULE ${PLUGIN_WPAR_SRC})
    target_link_libraries(wpar PRIVATE libmetric libutils LibPerfstat::LibPerfstat)
    set_target_properties(wpar PROPERTIES PREFIX "")

#    add_executable(test_plugin_wpar EXCLUDE_FROM_ALL wpar_test.c ${PLUGIN_WPAR_SRC})
#    target_link_libraries(test_plugin_wpar libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_wpar)
#    add_test(NAME test_plugin_wpar COMMAND test_plugin_wpar WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS wpar DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-wpar.5 ncollectd-wpar.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-wpar.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
