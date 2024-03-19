plugin_option(PLUGIN_ETHSTAT "Stats from NIC driver" ON)

include(CheckIncludeFile)

set(BUILD_PLUGIN_ETHSTAT 0)

if(PLUGIN_ETHSTAT)
    if(BUILD_LINUX)
        check_include_file(linux/sockios.h HAVE_LINUX_SOCKIOS_H)
        check_include_file(linux/ethtool.h HAVE_LINUX_ETHTOOL_H)
        if(HAVE_LINUX_SOCKIOS_H AND HAVE_LINUX_ETHTOOL_H)
            set(BUILD_PLUGIN_ETHSTAT 1)
        else()
            set(BUILD_PLUGIN_ETHSTAT_REASON "not found linux/sockios.h or linux/ethtool.h" PARENT_SCOPE)
        endif()
    else()
        set(BUILD_PLUGIN_ETHSTAT_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_ETHSTAT ${BUILD_PLUGIN_ETHSTAT} PARENT_SCOPE)

if(BUILD_PLUGIN_ETHSTAT)
    set(PLUGIN_ETHSTAT_SRC ethstat.c)
    add_library(ethstat MODULE ${PLUGIN_ETHSTAT_SRC})

    if(BUILD_LINUX)
        target_compile_definitions(ethstat PUBLIC HAVE_LINUX_SOCKIOS_H)
        target_compile_definitions(ethstat PUBLIC HAVE_LINUX_ETHTOOL_H)
    endif()

    target_link_libraries(ethstat PRIVATE libutils libmetric)
    set_target_properties(ethstat PROPERTIES PREFIX "")

#    add_executable(test_plugin_ethstat EXCLUDE_FROM_ALL ethstat_test.c ${PLUGIN_ETHSTAT_SRC})
#    target_link_libraries(test_plugin_ethstat libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_ethstat)
#    add_test(NAME test_plugin_ethstat COMMAND test_plugin_ethstat WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS ethstat DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-ethstat.5 ncollectd-ethstat.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-ethstat.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
