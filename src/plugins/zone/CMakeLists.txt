plugin_option(PLUGIN_ZONE "Solaris container statistics" ON)

set(BUILD_PLUGIN_ZONE 0)

if(PLUGIN_ZONE)
    if(BUILD_SOLARIS)
        find_package(LibKstat)
        if(LIBKSTAT_FOUND)
            set(BUILD_PLUGIN_ZONE 1)
        else()
            set(BUILD_PLUGIN_ZONE_REASON "kstat not found" PARENT_SCOPE)
        endif()
    else()
        set(BUILD_PLUGIN_ZONE_REASON "Solaris only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_ZONE ${BUILD_PLUGIN_ZONE} PARENT_SCOPE)

if(BUILD_PLUGIN_ZONE)
    set(PLUGIN_ZONE_SRC zone.c)
    add_library(zone MODULE ${PLUGIN_ZONE_SRC})
    target_link_libraries(zone PRIVATE libmetric libutils LibKstat::LibKstat)
    set_target_properties(zone PROPERTIES PREFIX "")

#    add_executable(test_plugin_zone EXCLUDE_FROM_ALL zone_test.c ${PLUGIN_ZONE_SRC})
#    target_link_libraries(test_plugin_zone libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_zone)
#    add_test(NAME test_plugin_zone COMMAND test_plugin_zone WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS zone DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-zone.5 ncollectd-zone.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-zone.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
