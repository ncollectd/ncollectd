include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBMNL QUIET libmnl)

find_path(LIBMNL_INCLUDE_DIR NAMES libmnl/libmnl.h
          HINTS ${PC_LIBMNL_INCLUDEDIR} ${PC_LIBMNL_INCLUDE_DIRS})
find_library(LIBMNL_LIBRARIES NAMES mnl
             HINTS ${PC_LIBMNL_LIBDIR} ${PC_LIBMNL_LIBRARY_DIRS})

find_package_handle_standard_args(LibMnl DEFAULT_MSG LIBMNL_LIBRARIES LIBMNL_INCLUDE_DIR)

mark_as_advanced(LIBMNL_INCLUDE_DIR LIBMNL_LIBRARIES)

if(LIBMNL_FOUND AND NOT TARGET LibMnl::LibMnl)
    set(LIBMNL_INCLUDE_DIRS "${LIBMNL_INCLUDE_DIR}")
    set(LIBMNL_DEFINITIONS ${PC_LIBMNL_CFLAGS_OTHER})
    add_library(LibMnl::LibMnl INTERFACE IMPORTED)
    set_target_properties(LibMnl::LibMnl PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBMNL_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBMNL_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBMNL_LIBRARIES}")
endif()
