include(FindPackageHandleStandardArgs)

find_path(LIBSENSORS_INCLUDE_DIR NAMES sensors/sensors.h)
find_library(LIBSENSORS_LIBRARIES NAMES sensors)

#FIXME check versions

find_package_handle_standard_args(LibSensors DEFAULT_MSG LIBSENSORS_LIBRARIES LIBSENSORS_INCLUDE_DIR)

mark_as_advanced(LIBSENSORS_INCLUDE_DIR LIBSENSORS_LIBRARIES)

if(LIBSENSORS_FOUND AND NOT TARGET LibSensors::LibSensors)
    set(LIBSENSORS_INCLUDE_DIRS "${LIBSENSORS_INCLUDE_DIR}")
    add_library(LibSensors::LibSensors INTERFACE IMPORTED)
    set_target_properties(LibSensors::LibSensors PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBSENSORS_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBSENSORS_LIBRARIES}")
endif()
