include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBUDEV QUIET libudev)

find_path(LIBUDEV_INCLUDE_DIR NAMES libudev.h
          HINTS ${PC_LIBUDEV_INCLUDEDIR} ${PC_LIBUDEV_INCLUDE_DIRS})
find_library(LIBUDEV_LIBRARIES NAMES udev
             HINTS ${PC_LIBUDEV_LIBDIR} ${PC_LIBUDEV_LIBRARY_DIRS})

find_package_handle_standard_args(LibUdev DEFAULT_MSG LIBUDEV_LIBRARIES LIBUDEV_INCLUDE_DIR)

mark_as_advanced(LIBUDEV_INCLUDE_DIR LIBUDEV_LIBRARIES)

if(LIBUDEV_FOUND AND NOT TARGET LibUdev::LibUdev)
    set(LIBUDEV_INCLUDE_DIRS "${LIBUDEV_INCLUDE_DIR}")
    set(LIBUDEV_CFLAGS ${PC_LIBUDEV_CFLAGS_OTHER})
    set(LIBUDEV_DEFINITIONS HAVE_LIBUDEV_H)
    add_library(LibUdev::LibUdev INTERFACE IMPORTED)
    set_target_properties(LibUdev::LibUdev PROPERTIES
                          INTERFACE_COMPILE_DEFINITIONS "${LIBUDEV_DEFINITIONS}"
                          INTERFACE_COMPILE_OPTIONS     "${LIBUDEV_CFLAGS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBUDEV_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBUDEV_LIBRARIES}")
endif()
