include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBIPTC QUIET libiptc)

find_path(LIBIPTC_INCLUDE_DIR NAMES libiptc/libiptc.h libiptc/libip6tc.h
          HINTS ${PC_LIBIPTC_INCLUDEDIR} ${PC_LIBIPTC_INCLUDE_DIRS})
find_library(LIBIPTC_LIBRARIES NAMES ip4tc ip6tc
             HINTS ${PC_LIBIPTC_LIBDIR} ${PC_LIBIPTC_LIBRARY_DIRS})

find_package_handle_standard_args(LibIptc DEFAULT_MSG LIBIPTC_LIBRARIES LIBIPTC_INCLUDE_DIR)

mark_as_advanced(LIBIPTC_INCLUDE_DIR LIBIPTC_LIBRARIES)

if(LIBIPTC_FOUND AND NOT TARGET LibIptc::LibIptc)
    set(LIBIPTC_INCLUDE_DIRS "${LIBIPTC_INCLUDE_DIR}")
    set(LIBIPTC_DEFINITIONS ${PC_LIBIPTC_CFLAGS_OTHER})
    add_library(LibIptc::LibIptc INTERFACE IMPORTED)
    set_target_properties(LibIptc::LibIptc PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBIPTC_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBIPTC_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBIPTC_LIBRARIES}")
endif()
