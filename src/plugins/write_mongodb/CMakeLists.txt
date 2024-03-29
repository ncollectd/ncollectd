plugin_option(PLUGIN_WRITE_MONGODB "Write MongoDB plugin" ON)

set(BUILD_PLUGIN_WRITE_MONGODB 0)

if(PLUGIN_WRITE_MONGODB)
    find_package(LibMongoc)
    find_package(LibBson)
    if (LIBMONGOC_FOUND AND LIBBSON_FOUND)
        set(BUILD_PLUGIN_WRITE_MONGODB 1)
    else()
        set(BUILD_PLUGIN_WRITE_MONGODB_REASON "libmongc or libbson not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_WRITE_MONGODB ${BUILD_PLUGIN_WRITE_MONGODB} PARENT_SCOPE)

if(BUILD_PLUGIN_WRITE_MONGODB)
    set(PLUGIN_WRITE_MONGODB_SRC write_mongodb.c)
    add_library(write_mongodb MODULE ${PLUGIN_WRITE_MONGODB_SRC} ${MONGODB_STATS_H})
    target_link_libraries(write_mongodb PRIVATE libutils libmetric LibMongoc::LibMongoc LibBson::LibBson)
    set_target_properties(write_mongodb PROPERTIES PREFIX "")
    install(TARGETS write_mongodb DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-write_mongodb.5 ncollectd-write_mongodb.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-write_mongodb.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
