include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBGPS QUIET libgps)

find_path(LIBGPS_INCLUDE_DIR NAMES gps.h
          HINTS ${PC_LIBGPS_INCLUDEDIR} ${PC_LIBGPS_INCLUDE_DIRS})
find_library(LIBGPS_LIBRARIES NAMES gps
             HINTS ${PC_LIBGPS_LIBDIR} ${PC_LIBGPS_LIBRARY_DIRS})

find_package_handle_standard_args(LibGps DEFAULT_MSG LIBGPS_LIBRARIES LIBGPS_INCLUDE_DIR)

mark_as_advanced(LIBGPS_INCLUDE_DIR LIBGPS_LIBRARIES)

if(LIBGPS_FOUND AND NOT TARGET LibGps::LibGps)
    set(LIBGPS_INCLUDE_DIRS "${LIBGPS_INCLUDE_DIR}")
    set(LIBGPS_DEFINITIONS ${PC_LIBGPS_CFLAGS_OTHER})
    add_library(LibGps::LibGps INTERFACE IMPORTED)
    set_target_properties(LibGps::LibGps PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBGPS_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBGPS_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBGPS_LIBRARIES}")
endif()
