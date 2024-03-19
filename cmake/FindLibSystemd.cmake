include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBSYSTEMD QUIET libsystemd)

find_path(LIBSYSTEMD_INCLUDE_DIR NAMES systemd/sd-journal.h
          HINTS ${PC_LIBSYSTEMD_INCLUDEDIR} ${PC_LIBSYSTEMD_INCLUDE_DIRS})
find_library(LIBSYSTEMD_LIBRARIES NAMES systemd
             HINTS ${PC_LIBSYSTEMD_LIBDIR} ${PC_LIBSYSTEMD_LIBRARY_DIRS})

find_package_handle_standard_args(LibSystemd DEFAULT_MSG LIBSYSTEMD_LIBRARIES LIBSYSTEMD_INCLUDE_DIR)

mark_as_advanced(LIBSYSTEMD_INCLUDE_DIR LIBSYSTEMD_LIBRARIES)

if(LIBSYSTEMD_FOUND AND NOT TARGET LibSystemd::LibSystemd)
    set(LIBSYSTEMD_INCLUDE_DIRS "${LIBSYSTEMD_INCLUDE_DIR}")
    set(LIBSYSTEMD_DEFINITIONS ${PC_LIBSYSTEMD_CFLAGS_OTHER})
    add_library(LibSystemd::LibSystemd INTERFACE IMPORTED)
    set_target_properties(LibSystemd::LibSystemd PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBSYSTEMD_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBSYSTEMD_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBSYSTEMD_LIBRARIES}")
endif()
