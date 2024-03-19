plugin_option(PLUGIN_MMC "MMC statistics" ON)

set(BUILD_PLUGIN_MMC 0)

if(PLUGIN_MMC)
    if(BUILD_LINUX)
        find_package(LibUdev)
        if (LIBUDEV_FOUND)
            set(BUILD_PLUGIN_MMC 1)
        else()
            set(BUILD_PLUGIN_MMC_REASON "libudev not found" PARENT_SCOPE)
        endif()
    else()
        set(BUILD_PLUGIN_MMC_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_MMC ${BUILD_PLUGIN_MMC} PARENT_SCOPE)

if(BUILD_PLUGIN_MMC)
    set(PLUGIN_MMC_SRC mmc.c)
    add_library(mmc MODULE ${PLUGIN_MMC_SRC})
    target_link_libraries(mmc PRIVATE libutils libmetric LibUdev::LibUdev)
    set_target_properties(mmc PROPERTIES PREFIX "")

#    add_executable(test_plugin_mmc EXCLUDE_FROM_ALL mmc_test.c ${PLUGIN_MMC_SRC})
#    target_link_libraries(test_plugin_mmc libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_mmc)
#    add_test(NAME test_plugin_mmc COMMAND test_plugin_mmc WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS mmc DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-mmc.5 ncollectd-mmc.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-mmc.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
