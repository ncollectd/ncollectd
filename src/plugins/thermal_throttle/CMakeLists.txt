plugin_option(PLUGIN_THERMAL_THROTTLE "Thermal throttle stats" ON)

include(CheckIncludeFile)

set(BUILD_PLUGIN_THERMAL_THROTTLE 0)

if(PLUGIN_THERMAL_THROTTLE)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_THERMAL_THROTTLE 1)
    else()
        set(BUILD_PLUGIN_THERMAL_THROTTLE_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_THERMAL_THROTTLE ${BUILD_PLUGIN_THERMAL_THROTTLE} PARENT_SCOPE)

if(BUILD_PLUGIN_THERMAL_THROTTLE)
    set(PLUGIN_THERMAL_THROTTLE_SRC thermal_throttle.c)
    add_library(thermal_throttle MODULE ${PLUGIN_THERMAL_THROTTLE_SRC})
    target_link_libraries(thermal_throttle PRIVATE libmetric libutils)
    set_target_properties(thermal_throttle PROPERTIES PREFIX "")

    add_executable(test_plugin_thermal_throttle EXCLUDE_FROM_ALL thermal_throttle_test.c ${PLUGIN_THERMAL_THROTTLE_SRC})
    target_link_libraries(test_plugin_thermal_throttle libtest libmetric libutils -lm)
    add_dependencies(build_tests test_plugin_thermal_throttle)
    add_test(NAME test_plugin_thermal_throttle COMMAND test_plugin_thermal_throttle WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS thermal_throttle DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-thermal_throttle.5 ncollectd-thermal_throttle.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-thermal_throttle.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
