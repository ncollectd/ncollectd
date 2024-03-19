plugin_option(PLUGIN_DMI "DMI info" ON)

set(BUILD_PLUGIN_DMI 0)

if(PLUGIN_DMI)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_DMI 1)
    else()
        set(BUILD_PLUGIN_DMI_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_DMI ${BUILD_PLUGIN_DMI} PARENT_SCOPE)

if(BUILD_PLUGIN_DMI)
    set(PLUGIN_DMI_SRC dmi.c)
    add_library(dmi MODULE ${PLUGIN_DMI_SRC})
    target_link_libraries(dmi PRIVATE libutils libmetric)
    set_target_properties(dmi PROPERTIES PREFIX "")

#    add_executable(test_plugin_dmi EXCLUDE_FROM_ALL dmi_test.c ${PLUGIN_DMI_SRC})
#    target_link_libraries(test_plugin_dmi libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_dmi)
#    add_test(NAME test_plugin_dmi COMMAND test_plugin_dmi WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS dmi DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-dmi.5 ncollectd-dmi.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-dmi.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
