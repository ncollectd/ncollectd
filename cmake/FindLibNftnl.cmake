include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBNFTNL QUIET libnftnl)

find_path(LIBNFTNL_INCLUDE_DIR NAMES libnftnl/expr.h libnftnl/rule.h libnftnl/udata.h
          HINTS ${PC_LIBNFTNL_INCLUDEDIR} ${PC_LIBNFTNL_INCLUDE_DIRS})
find_library(LIBNFTNL_LIBRARIES NAMES nftnl
             HINTS ${PC_LIBNFTNL_LIBDIR} ${PC_LIBNFTNL_LIBRARY_DIRS})

find_package_handle_standard_args(LibNftnl DEFAULT_MSG LIBNFTNL_LIBRARIES LIBNFTNL_INCLUDE_DIR)

mark_as_advanced(LIBNFTNL_INCLUDE_DIR LIBNFTNL_LIBRARIES)

if(LIBNFTNL_FOUND AND NOT TARGET LibNftnl::LibNftnl)
    set(LIBNFTNL_INCLUDE_DIRS "${LIBNFTNL_INCLUDE_DIR}")
    set(LIBNFTNL_DEFINITIONS ${PC_LIBNFTNL_CFLAGS_OTHER})
    add_library(LibNftnl::LibNftnl INTERFACE IMPORTED)
    set_target_properties(LibNftnl::LibNftnl PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBNFTNL_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBNFTNL_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBNFTNL_LIBRARIES}")
endif()
