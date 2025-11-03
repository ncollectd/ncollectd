plugin_option(PLUGIN_PRESSURE "Pressure stall statistics" ON)

set(BUILD_PLUGIN_PRESSURE 0)

if(PLUGIN_PRESSURE)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_PRESSURE 1)
    else()
        set(BUILD_PLUGIN_PRESSURE_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_PRESSURE ${BUILD_PLUGIN_PRESSURE} PARENT_SCOPE)

if(BUILD_PLUGIN_PRESSURE)
    set(PLUGIN_PRESSURE_SRC pressure.c)
    add_library(pressure MODULE ${PLUGIN_PRESSURE_SRC})
    target_link_libraries(pressure PRIVATE libmetric libutils)
    set_target_properties(pressure PROPERTIES PREFIX "")

    add_executable(test_plugin_pressure EXCLUDE_FROM_ALL pressure_test.c ${PLUGIN_PRESSURE_SRC})
    target_link_libraries(test_plugin_pressure libtest libmetric libutils -lm)
    add_dependencies(build_tests test_plugin_pressure)
    add_test(NAME test_plugin_pressure COMMAND test_plugin_pressure WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS pressure DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-pressure.5 ncollectd-pressure.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-pressure.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
