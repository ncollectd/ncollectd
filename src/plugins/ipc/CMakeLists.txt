plugin_option(PLUGIN_IPC "IPC statistics" ON)

set(BUILD_PLUGIN_IPC 0)

if(PLUGIN_IPC)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_IPC 1)
    elseif(BUILD_AIX)
        find_package(LibPerfstat)
        if(LIBPERFSTAT_FOUND)
            set(BUILD_PLUGIN_IPC 1)
        else()
            set(BUILD_PLUGIN_IPC_REASON "perfstat not found" PARENT_SCOPE)
        endif()
    else()
        set(BUILD_PLUGIN_IPC_REASON "unsupported system" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_IPC ${BUILD_PLUGIN_IPC} PARENT_SCOPE)

if(BUILD_PLUGIN_IPC)
    set(PLUGIN_IPC_SRC ipc.c)
    set(PLUGIN_IPC_LIBS libmetric libutils)

    if(BUILD_AIX)
        list(APPEND PLUGIN_IPC_LIBS LibPerfstat::LibPerfstat)
    endif()

    add_library(ipc MODULE ${PLUGIN_IPC_SRC})
    target_link_libraries(ipc PRIVATE ${PLUGIN_IPC_LIBS})
    set_target_properties(ipc PROPERTIES PREFIX "")

#    add_executable(test_plugin_ipc EXCLUDE_FROM_ALL ipc_test.c ${PLUGIN_IPC_SRC})
#    target_link_libraries(test_plugin_ipc libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_ipc)
#    add_test(NAME test_plugin_ipc COMMAND test_plugin_ipc WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS ipc DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-ipc.5 ncollectd-ipc.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-ipc.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
