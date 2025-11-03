plugin_option(PLUGIN_BTRFS "Btrfs statistics" ON)

include(CheckIncludeFile)

set(BUILD_PLUGIN_BTRFS 0)

if(PLUGIN_BTRFS)
    if(BUILD_LINUX)
        check_include_file(btrfs/ioctl.h HAVE_BTRFS_IOCTL_H)
        if(HAVE_BTRFS_IOCTL_H)
            set(BUILD_PLUGIN_BTRFS 1)
        else()
        set(BUILD_PLUGIN_BTRFS_REASON "btrfs/ioctl.h no found" PARENT_SCOPE)
        endif()
    else()
        set(BUILD_PLUGIN_BTRFS_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_BTRFS ${BUILD_PLUGIN_BTRFS} PARENT_SCOPE)

if(BUILD_PLUGIN_BTRFS)
    set(PLUGIN_BTRFS_SRC btrfs.c)
    add_library(btrfs MODULE ${PLUGIN_BTRFS_SRC})
    target_link_libraries(btrfs PRIVATE libmetric libutils)
    set_target_properties(btrfs PROPERTIES PREFIX "")

#    add_executable(test_plugin_btrfs EXCLUDE_FROM_ALL btrfs_test.c ${PLUGIN_BTRFS_SRC})
#    target_link_libraries(test_plugin_btrfs libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_btrfs)
#    add_test(NAME test_plugin_btrfs COMMAND test_plugin_btrfs WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS btrfs DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-btrfs.5 ncollectd-btrfs.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-btrfs.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
