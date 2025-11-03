plugin_option(PLUGIN_BATTERY "Battery statistics" ON)

set(BUILD_PLUGIN_BATTERY 0)

if(PLUGIN_BATTERY)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_BATTERY 1)
    elseif(BUILD_DARWIN)
        find_package(LibIokit)
        if(LIBIOKIT_FOUND)
            set(BUILD_PLUGIN_BATTERY 1)
        else()
            set(BUILD_PLUGIN_BATTERY_REASON "libiokit not found" PARENT_SCOPE)
        endif()
    else()
        set(BUILD_PLUGIN_BATTERY_REASON "unsupported system" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_BATTERY ${BUILD_PLUGIN_BATTERY} PARENT_SCOPE)

if(BUILD_PLUGIN_BATTERY)
    set(PLUGIN_BATTERY_SRC battery.c)
    set(PLUGIN_BATTERY_LIBS libmetric libutils)

    if(BUILD_DARWIN)
        list(APPEND PLUGIN_BATTERY_LIBS LibIokit::LibIokit)
    endif()

    add_library(battery MODULE ${PLUGIN_BATTERY_SRC})
    target_link_libraries(battery PRIVATE ${PLUGIN_BATTERY_LIBS})
    set_target_properties(battery PROPERTIES PREFIX "")

#    add_executable(test_plugin_battery EXCLUDE_FROM_ALL battery_test.c ${PLUGIN_BATTERY_SRC})
#    target_link_libraries(test_plugin_battery libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_battery)
#    add_test(NAME test_plugin_battery COMMAND test_plugin_battery WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS battery DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-battery.5 ncollectd-battery.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-battery.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
