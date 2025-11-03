plugin_option(PLUGIN_LVM "LVM statistics" ON)

set(BUILD_PLUGIN_LVM 0)

if(PLUGIN_LVM)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_LVM 1)
    else()
        set(BUILD_PLUGIN_LVM_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_LVM ${BUILD_PLUGIN_LVM} PARENT_SCOPE)

if(BUILD_PLUGIN_LVM)
    set(PLUGIN_LVM_SRC lvm.c)
    add_library(lvm MODULE ${PLUGIN_LVM_SRC})
    target_link_libraries(lvm PRIVATE libmetric libutils libxson)
    set_target_properties(lvm PROPERTIES PREFIX "")

#    add_executable(test_plugin_lvm EXCLUDE_FROM_ALL lvm_test.c ${PLUGIN_LVM_SRC})
#    target_link_libraries(test_plugin_lvm libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_lvm)
#    add_test(NAME test_plugin_lvm COMMAND test_plugin_lvm WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS lvm DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-lvm.5 ncollectd-lvm.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-lvm.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
