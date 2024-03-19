include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBIPMCTL QUIET libipmctl)

find_path(LIBIPMCTL_INCLUDE_DIR NAMES nvm_types.h nvm_management.h
          HINTS ${PC_LIBIPMCTL_INCLUDEDIR} ${PC_LIBIPMCTL_INCLUDE_DIRS})
find_library(LIBIPMCTL_LIBRARIES NAMES ipmctl
             HINTS ${PC_LIBIPMCTL_LIBDIR} ${PC_LIBIPMCTL_LIBRARY_DIRS})

find_package_handle_standard_args(LibIpmctl DEFAULT_MSG LIBIPMCTL_LIBRARIES LIBIPMCTL_INCLUDE_DIR)

mark_as_advanced(LIBIPMCTL_INCLUDE_DIR LIBIPMCTL_LIBRARIES)

if(LIBIPMCTL_FOUND AND NOT TARGET LibIpmctl::LibIpmctl)
    set(LIBIPMCTL_INCLUDE_DIRS "${LIBIPMCTL_INCLUDE_DIR}")
    set(LIBIPMCTL_DEFINITIONS ${PC_LIBIPMCTL_CFLAGS_OTHER})
    add_library(LibIpmctl::LibIpmctl INTERFACE IMPORTED)
    set_target_properties(LibIpmctl::LibIpmctl PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBIPMCTL_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBIPMCTL_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBIPMCTL_LIBRARIES}")
endif()
