include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
foreach(LUA_NAME "lua" "lua-5.4" "lua5.4" "lua54" "lua-5.3" "lua5.3" "lua53"
                 "lua-5.2" "lua5.2" "lua52" "lua-5.1" "lua5.1" "lua51" "luajit")
    pkg_check_modules(PC_LIBLUA QUIET "${LUA_NAME}")
    if(PC_LIBLUA_FOUND)
        break()
    endif()
endforeach()

if(NOT PC_LIBLUA_LIBRARIES)
    set(PC_LIBLUA_LIBRARIES lua)
endif()

find_path(LIBLUA_INCLUDE_DIR NAMES lua.h lauxlib.h lualib.h
          HINTS ${PC_LIBLUA_INCLUDEDIR} ${PC_LIBLUA_INCLUDE_DIRS})
      find_library(LIBLUA_LIBRARIES NAMES ${PC_LIBLUA_LIBRARIES}
             HINTS ${PC_LIBLUA_LIBDIR} ${PC_LIBLUA_LIBRARY_DIRS})

find_package_handle_standard_args(LibLua REQUIRED_VARS LIBLUA_LIBRARIES LIBLUA_INCLUDE_DIR)

mark_as_advanced(LIBLUA_INCLUDE_DIR LIBLUA_LIBRARIES)

if(LIBLUA_FOUND AND NOT TARGET LibLua::LibLua)
    set(LIBLUA_INCLUDE_DIRS "${LIBLUA_INCLUDE_DIR}")
    set(LIBLUA_CFLAGS ${PC_LIBLUA_CFLAGS_OTHER})
    add_library(LibLua::LibLua INTERFACE IMPORTED)
    set_target_properties(LibLua::LibLua PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBLUA_CFLAGS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBLUA_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBLUA_LIBRARIES}")
endif()
