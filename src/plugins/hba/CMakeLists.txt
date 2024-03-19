plugin_option(PLUGIN_HBA "AIX HBA statistics" ON)

set(BUILD_PLUGIN_HBA 0)

if(PLUGIN_HBA)
    if(BUILD_AIX)
        find_package(LibOdm)
        if(LIBODM_FOUND)
            set(BUILD_PLUGIN_HBA 1)
        else()
            set(BUILD_PLUGIN_HBA_REASON "libodm not found" PARENT_SCOPE)
        endif()
    else()
        set(BUILD_PLUGIN_HBA_REASON "AIX only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_HBA ${BUILD_PLUGIN_HBA} PARENT_SCOPE)

if(BUILD_PLUGIN_HBA)
    set(PLUGIN_HBA_SRC hba.c)
    add_library(hba MODULE ${PLUGIN_HBA_SRC})
    target_link_libraries(hba PRIVATE libutils libmetric LibOdm::LibOdm)
    set_target_properties(hba PROPERTIES PREFIX "")

#    add_executable(test_plugin_hba EXCLUDE_FROM_ALL hba_test.c ${PLUGIN_HBA_SRC})
#    target_link_libraries(test_plugin_hba libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_hba)
#    add_test(NAME test_plugin_hba COMMAND test_plugin_hba WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS hba DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-hba.5 ncollectd-hba.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-hba.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
