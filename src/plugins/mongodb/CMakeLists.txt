plugin_option(PLUGIN_MONGODB "MongoDB plugin" ON)

set(BUILD_PLUGIN_MONGODB 0)

if(PLUGIN_MONGODB)
    find_package(LibMongoc)
    find_package(LibBson)
    if (LIBMONGOC_FOUND AND LIBBSON_FOUND)
        set(BUILD_PLUGIN_MONGODB 1)
    else()
        set(BUILD_PLUGIN_MONGODB_REASON "libmongc or libbson not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_MONGODB ${BUILD_PLUGIN_MONGODB} PARENT_SCOPE)

if(BUILD_PLUGIN_MONGODB)
    gperf_generate(mongodb_stats.gperf "${CMAKE_CURRENT_BINARY_DIR}/mongodb_stats.h" MONGODB_STATS_H)
    set(PLUGIN_MONGODB_SRC mongodb.c mongodb_fam.c mongodb_fam.h)
    add_library(mongodb MODULE ${PLUGIN_MONGODB_SRC} ${MONGODB_STATS_H})
    target_link_libraries(mongodb PRIVATE libutils libmetric LibMongoc::LibMongoc LibBson::LibBson)
    set_target_properties(mongodb PROPERTIES PREFIX "")
    install(TARGETS mongodb DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-mongodb.5 ncollectd-mongodb.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-mongodb.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
