plugin_option(PLUGIN_LUA "Lua plugin" ON)

set(BUILD_PLUGIN_LUA 0)

if(PLUGIN_LUA)
    find_package(LibLua)
    if(LIBLUA_FOUND)
        set(BUILD_PLUGIN_LUA 1)
    else()
        set(BUILD_PLUGIN_LUA_REASON "lua not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_LUA ${BUILD_PLUGIN_LUA} PARENT_SCOPE)

if(BUILD_PLUGIN_LUA)
    set(PLUGIN_LUA_SRC lua.c utils.c utils.h)
    add_library(lua MODULE ${PLUGIN_LUA_SRC})
    target_link_libraries(lua PRIVATE libutils libmetric LibLua::LibLua)
    set_target_properties(lua PROPERTIES PREFIX "")

    add_executable(test_plugin_lua EXCLUDE_FROM_ALL lua_test.c ${PLUGIN_LUA_SRC})
    target_link_libraries(test_plugin_lua libtest libutils libmetric LibLua::LibLua)
    add_dependencies(build_tests test_plugin_lua)
    add_test(NAME test_plugin_lua COMMAND test_plugin_lua WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS lua DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-lua.5 ncollectd-lua.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-lua.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
