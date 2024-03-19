include(FindPackageHandleStandardArgs)

find_path(LIBKSTAT_INCLUDE_DIR NAMES kstat.h)
find_library(LIBKSTAT_LIBRARIES NAMES kstat)

find_package_handle_standard_args(LibKstat DEFAULT_MSG LIBKSTAT_LIBRARIES LIBKSTAT_INCLUDE_DIR)

mark_as_advanced(LIBKSTAT_INCLUDE_DIR LIBKSTAT_LIBRARIES)

if(LIBKSTAT_FOUND AND NOT TARGET LibKstat::LibKstat)
    set(LIBKSTAT_INCLUDE_DIRS "${LIBKSTAT_INCLUDE_DIR}")
    set(LIBKSTAT_DEFINITIONS HAVE_LIBKSTAT)
    add_library(LibKstat::LibKstat INTERFACE IMPORTED)
    set_target_properties(LibKstat::LibKstat PROPERTIES
                          INTERFACE_COMPILE_DEFINITIONS "${LIBKSTAT_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBKSTAT_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBKSTAT_LIBRARIES}")
endif()
