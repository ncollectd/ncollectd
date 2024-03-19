plugin_option(PLUGIN_INTEL_RDT "Intel RDT monitor plugin" ON)

set(BUILD_PLUGIN_INTEL_RDT 0)

if(PLUGIN_INTEL_RDT)
    find_package(LibPqos)
    if(LIBPQOS_FOUND)
        set(BUILD_PLUGIN_INTEL_RDT 1)
    else()
        set(BUILD_PLUGIN_INTEL_RDT_REASON "libpqos not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_INTEL_RDT ${BUILD_PLUGIN_INTEL_RDT} PARENT_SCOPE)

if(BUILD_PLUGIN_INTEL_RDT)
    set(PLUGIN_INTEL_RDT_SRC intel_rdt.c proc_pids.c proc_pids.h config_cores.h config_cores.c)
    add_library(intel_rdt MODULE ${PLUGIN_INTEL_RDT_SRC})
    target_link_libraries(intel_rdt PRIVATE libutils libmetric LibPqos::LibPqos)
    set_target_properties(intel_rdt PROPERTIES PREFIX "")

#    add_executable(test_plugin_intel_rdt EXCLUDE_FROM_ALL intel_rdt_test.c ${PLUGIN_INTEL_RDT_SRC})
#    target_link_libraries(test_plugin_intel_rdt libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_intel_rdt)
#    add_test(NAME test_plugin_intel_rdt COMMAND test_plugin_intel_rdt WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS intel_rdt DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-intel_rdt.5 ncollectd-intel_rdt.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-intel_rdt.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
