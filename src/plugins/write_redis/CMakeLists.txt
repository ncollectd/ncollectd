plugin_option(PLUGIN_WRITE_REDIS "Write redis plugin" ON)

set(BUILD_PLUGIN_WRITE_REDIS 0)

if(PLUGIN_WRITE_REDIS)
    find_package(LibHiredis)
    if(LIBHIREDIS_FOUND)
        set(BUILD_PLUGIN_WRITE_REDIS 1)
    else()
        set(BUILD_PLUGIN_WRITE_REDIS_REASON "libhiredis not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_WRITE_REDIS ${BUILD_PLUGIN_WRITE_REDIS} PARENT_SCOPE)

if(BUILD_PLUGIN_WRITE_REDIS)
    set(PLUGIN_WRITE_REDIS_SRC write_redis.c)
    add_library(write_redis MODULE ${PLUGIN_WRITE_REDIS_SRC})
    target_link_libraries(write_redis PRIVATE libutils libmetric LibHiredis::LibHiredis)
    set_target_properties(write_redis PROPERTIES PREFIX "")
    install(TARGETS write_redis DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-write_redis.5 ncollectd-write_redis.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-write_redis.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
