plugin_option(PLUGIN_VMEM "Virtual memory statistics" ON)

set(BUILD_PLUGIN_VMEM 0)

if(PLUGIN_VMEM)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_VMEM 1)
    else()
        set(BUILD_PLUGIN_VMEM_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_VMEM ${BUILD_PLUGIN_VMEM} PARENT_SCOPE)

if(BUILD_PLUGIN_VMEM)
    gperf_generate(vmem.gperf "${CMAKE_CURRENT_BINARY_DIR}/vmem.h" VMEM_H)
    set(PLUGIN_VMEM_SRC vmem.c)
    add_library(vmem MODULE ${PLUGIN_VMEM_SRC} ${VMEM_H})
    target_link_libraries(vmem PRIVATE libutils libmetric)
    set_target_properties(vmem PROPERTIES PREFIX "")

    add_executable(test_plugin_vmem EXCLUDE_FROM_ALL vmem_test.c ${PLUGIN_VMEM_SRC})
    target_link_libraries(test_plugin_vmem libtest libutils libmetric -lm)
    add_dependencies(build_tests test_plugin_vmem)
    add_test(NAME test_plugin_vmem COMMAND test_plugin_vmem WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS vmem DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-vmem.5 ncollectd-vmem.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-vmem.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
