include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBCARES QUIET libcares)

find_path(LIBCARES_INCLUDE_DIR NAMES ares.h
          HINTS ${PC_LIBCARES_INCLUDEDIR} ${PC_LIBCARES_INCLUDE_DIRS})
find_library(LIBCARES_LIBRARIES NAMES cares
             HINTS ${PC_LIBCARES_LIBDIR} ${PC_LIBCARES_LIBRARY_DIRS})

find_package_handle_standard_args(LibCares DEFAULT_MSG LIBCARES_LIBRARIES LIBCARES_INCLUDE_DIR)

mark_as_advanced(LIBCARES_INCLUDE_DIR LIBCARES_LIBRARIES)

if(LIBCARES_FOUND AND NOT TARGET LibCares::LibCares)
    set(LIBCARES_INCLUDE_DIRS "${LIBCARES_INCLUDE_DIR}")
    set(LIBCARES_DEFINITIONS ${PC_LIBCARES_CFLAGS_OTHER} -DCARES_NO_DEPRECATED)
    add_library(LibCares::LibCares INTERFACE IMPORTED)
    set_target_properties(LibCares::LibCares PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBCARES_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBCARES_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBCARES_LIBRARIES}")
endif()
