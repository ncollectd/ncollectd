include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBSYSMAN QUIET level-zero)

find_path(LIBSYSMAN_INCLUDE_DIR NAMES level_zero/ze_api.h level_zero/zes_api.h
          HINTS ${PC_LIBSYSMAN_INCLUDEDIR} ${PC_LIBSYSMAN_INCLUDE_DIRS})
find_library(LIBSYSMAN_LIBRARIES NAMES ze_loader
             HINTS ${PC_LIBSYSMAN_LIBDIR} ${PC_LIBSYSMAN_LIBRARY_DIRS})

find_package_handle_standard_args(LibSysman DEFAULT_MSG LIBSYSMAN_LIBRARIES LIBSYSMAN_INCLUDE_DIR)

mark_as_advanced(LIBSYSMAN_INCLUDE_DIR LIBSYSMAN_LIBRARIES)

if(LIBSYSMAN_FOUND AND NOT TARGET LibSysman::LibSysman)
    set(LIBSYSMAN_INCLUDE_DIRS "${LIBSYSMAN_INCLUDE_DIR}")
    set(LIBSYSMAN_DEFINITIONS ${PC_LIBSYSMAN_CFLAGS_OTHER})
    add_library(LibSysman::LibSysman INTERFACE IMPORTED)
    set_target_properties(LibSysman::LibSysman PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBSYSMAN_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBSYSMAN_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBSYSMAN_LIBRARIES}")
endif()
