include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBYAJL QUIET yajl)

find_path(LIBYAJL_INCLUDE_DIR NAMES yajl/yajl_parse.h
          HINTS ${PC_LIBYAJL_INCLUDEDIR} ${PC_LIBYAJL_INCLUDE_DIRS})
find_library(LIBYAJL_LIBRARIES NAMES yajl
             HINTS ${PC_LIBYAJL_LIBDIR} ${PC_LIBYAJL_LIBRARY_DIRS})

find_package_handle_standard_args(LibYajl DEFAULT_MSG LIBYAJL_LIBRARIES LIBYAJL_INCLUDE_DIR)

mark_as_advanced(LIBYAJL_INCLUDE_DIR LIBYAJL_LIBRARIES)

if(LIBYAJL_FOUND AND NOT TARGET LibYajl::LibYajl)
    set(LIBYAJL_INCLUDE_DIRS "${LIBYAJL_INCLUDE_DIR}")
    set(LIBYAJL_DEFINITIONS ${PC_LIBYAJL_CFLAGS_OTHER})
    add_library(LibYajl::LibYajl INTERFACE IMPORTED)
    set_target_properties(LibYajl::LibYajl PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBYAJL_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBYAJL_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBYAJL_LIBRARIES}")
endif()
