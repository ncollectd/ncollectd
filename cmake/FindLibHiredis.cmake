include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBHIREDIS QUIET hiredis)

find_path(LIBHIREDIS_INCLUDE_DIR NAMES hiredis/hiredis.h
          HINTS ${PC_LIBHIREDIS_INCLUDEDIR} ${PC_LIBHIREDIS_INCLUDE_DIRS})
find_library(LIBHIREDIS_LIBRARIES NAMES hiredis
             HINTS ${PC_LIBHIREDIS_LIBDIR} ${PC_LIBHIREDIS_LIBRARY_DIRS})

find_package_handle_standard_args(LibHiredis DEFAULT_MSG LIBHIREDIS_LIBRARIES LIBHIREDIS_INCLUDE_DIR)

mark_as_advanced(LIBHIREDIS_INCLUDE_DIR LIBHIREDIS_LIBRARIES)

if(LIBHIREDIS_FOUND AND NOT TARGET LibHiredis::LibHiredis)
    set(LIBHIREDIS_INCLUDE_DIRS "${LIBHIREDIS_INCLUDE_DIR}")
    set(LIBHIREDIS_DEFINITIONS ${PC_LIBHIREDIS_CFLAGS_OTHER})
    add_library(LibHiredis::LibHiredis INTERFACE IMPORTED)
    set_target_properties(LibHiredis::LibHiredis PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBHIREDIS_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBHIREDIS_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBHIREDIS_LIBRARIES}")
endif()
