plugin_option(PLUGIN_THERMAL "Linux ACPI thermal zone statistics" ON)

set(BUILD_PLUGIN_THERMAL 0)

if(PLUGIN_THERMAL)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_THERMAL 1)
    else()
        set(BUILD_PLUGIN_THERMAL_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_THERMAL ${BUILD_PLUGIN_THERMAL} PARENT_SCOPE)

if(BUILD_PLUGIN_THERMAL)
    set(PLUGIN_THERMAL_SRC thermal.c)
    add_library(thermal MODULE ${PLUGIN_THERMAL_SRC})
    target_link_libraries(thermal PRIVATE libutils libmetric)
    set_target_properties(thermal PROPERTIES PREFIX "")

#    add_executable(test_plugin_thermal EXCLUDE_FROM_ALL thermal_test.c ${PLUGIN_THERMAL_SRC})
#    target_link_libraries(test_plugin_thermal libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_thermal)
#    add_test(NAME test_plugin_thermal COMMAND test_plugin_thermal WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS thermal DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-thermal.5 ncollectd-thermal.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-thermal.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
