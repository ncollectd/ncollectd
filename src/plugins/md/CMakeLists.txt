plugin_option(PLUGIN_MD "md (Linux software RAID) devices" ON)

include(CheckIncludeFile)

set(BUILD_PLUGIN_MD 0)

if(PLUGIN_MD)
    if(BUILD_LINUX)
        check_include_file(linux/raid/md_u.h HAVE_LINUX_RAID_MD_U_H)
        if(HAVE_LINUX_RAID_MD_U_H)
            set(BUILD_PLUGIN_MD 1)
        else()
            set(BUILD_PLUGIN_MD_REASON "linux/raid/md_u.h not found" PARENT_SCOPE)
        endif()
    else()
        set(BUILD_PLUGIN_MD_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_MD ${BUILD_PLUGIN_MD} PARENT_SCOPE)

if(BUILD_PLUGIN_MD)
    set(PLUGIN_MD_SRC md.c)
    add_library(md MODULE ${PLUGIN_MD_SRC})
    target_link_libraries(md PRIVATE libmetric libutils)
    set_target_properties(md PROPERTIES PREFIX "")

#    add_executable(test_plugin_md EXCLUDE_FROM_ALL md_test.c ${PLUGIN_MD_SRC})
#    target_link_libraries(test_plugin_md libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_md)
#    add_test(NAME test_plugin_md COMMAND test_plugin_md WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS md DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-md.5 ncollectd-md.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-md.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
