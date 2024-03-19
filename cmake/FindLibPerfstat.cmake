include(FindPackageHandleStandardArgs)

find_path(LIBPERFSTAT_INCLUDE_DIR NAMES libperfstat.h)
find_library(LIBPERFSTAT_LIBRARIES NAMES perfstat)

find_package_handle_standard_args(LibPerfstat DEFAULT_MSG LIBPERFSTAT_LIBRARIES LIBPERFSTAT_INCLUDE_DIR)

mark_as_advanced(LIBPERFSTAT_INCLUDE_DIR LIBPERFSTAT_LIBRARIES)

if(LIBPERFSTAT_FOUND AND NOT TARGET LibPerfstat::LibPerfstat)
    set(LIBPERFSTAT_INCLUDE_DIRS "${LIBPERFSTAT_INCLUDE_DIR}")
    set(LIBPERFSTAT_DEFINITIONS HAVE_PERFSTAT)
    add_library(LibPerfstat::LibPerfstat INTERFACE IMPORTED)
    set_target_properties(LibPerfstat::LibPerfstat PROPERTIES
                          INTERFACE_COMPILE_DEFINITIONS "${LIBPERFSTAT_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBPERFSTAT_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBPERFSTAT_LIBRARIES}")
endif()
