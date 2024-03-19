include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBODBC QUIET odbc)

find_path(LIBODBC_INCLUDE_DIR NAMES sql.h sqlext.h
          HINTS ${PC_LIBODBC_INCLUDEDIR} ${PC_LIBODBC_INCLUDE_DIRS})
find_library(LIBODBC_LIBRARIES NAMES odbc
             HINTS ${PC_LIBODBC_LIBDIR} ${PC_LIBODBC_LIBRARY_DIRS})

find_package_handle_standard_args(LibOdbc DEFAULT_MSG LIBODBC_LIBRARIES LIBODBC_INCLUDE_DIR)

mark_as_advanced(LIBODBC_INCLUDE_DIR LIBODBC_LIBRARIES)

if(LIBODBC_FOUND AND NOT TARGET LibOdbc::LibOdbc)
    set(LIBODBC_INCLUDE_DIRS "${LIBODBC_INCLUDE_DIR}")
    set(LIBODBC_DEFINITIONS ${PC_LIBODBC_CFLAGS_OTHER})
    add_library(LibOdbc::LibOdbc INTERFACE IMPORTED)
    set_target_properties(LibOdbc::LibOdbc PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBODBC_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBODBC_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBODBC_LIBRARIES}")
endif()
