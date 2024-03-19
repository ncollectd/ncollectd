include(FindPackageHandleStandardArgs)

find_path(LIBMOSQUITTO_INCLUDE_DIR NAMES mosquitto.h)
find_library(LIBMOSQUITTO_LIBRARIES NAMES mosquitto)

find_package_handle_standard_args(LibMosquitto DEFAULT_MSG LIBMOSQUITTO_LIBRARIES LIBMOSQUITTO_INCLUDE_DIR)

mark_as_advanced(LIBMOSQUITTO_INCLUDE_DIR LIBMOSQUITTO_LIBRARIES)

if(LIBMOSQUITTO_FOUND AND NOT TARGET LibMosquitto::LibMosquitto)
    set(LIBMOSQUITTO_INCLUDE_DIRS "${LIBMOSQUITTO_INCLUDE_DIR}")
    add_library(LibMosquitto::LibMosquitto INTERFACE IMPORTED)
    set_target_properties(LibMosquitto::LibMosquitto PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBMOSQUITTO_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBMOSQUITTO_LIBRARIES}")
endif()
