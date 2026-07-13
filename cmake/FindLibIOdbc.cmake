include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBIODBC QUIET libiodbc)

find_path(LIBIODBC_INCLUDE_DIR NAMES sql.h sqlext.h
          HINTS ${PC_LIBIODBC_INCLUDEDIR} ${PC_LIBIODBC_INCLUDE_DIRS})
find_library(LIBIODBC_LIBRARIES NAMES iodbc
             HINTS ${PC_LIBIODBC_LIBDIR} ${PC_LIBIODBC_LIBRARY_DIRS})

find_package_handle_standard_args(LibIOdbc DEFAULT_MSG LIBIODBC_LIBRARIES LIBIODBC_INCLUDE_DIR)

mark_as_advanced(LIBIODBC_INCLUDE_DIR LIBIODBC_LIBRARIES)

if(LIBIODBC_FOUND AND NOT TARGET LibIOdbc::LibIOdbc)
    set(LIBIODBC_INCLUDE_DIRS "${LIBIODBC_INCLUDE_DIR}")
    set(LIBIODBC_DEFINITIONS ${PC_LIBIODBC_CFLAGS_OTHER})
    add_library(LibIOdbc::LibIOdbc INTERFACE IMPORTED)
    set_target_properties(LibIOdbc::LibIOdbc PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBIODBC_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBIODBC_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBIODBC_LIBRARIES}")
endif()
