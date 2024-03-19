include(FindPackageHandleStandardArgs)

find_path(LIBREDFISH_INCLUDE_DIR NAMES redfish.h)
find_library(LIBREDFISH_LIBRARIES NAMES redfish)

find_package_handle_standard_args(LibRedfish DEFAULT_MSG LIBREDFISH_LIBRARIES LIBREDFISH_INCLUDE_DIR)

mark_as_advanced(LIBREDFISH_INCLUDE_DIR LIBREDFISH_LIBRARIES)

if(LIBREDFISH_FOUND AND NOT TARGET LibRedfish::LibRedfish)
    set(LIBREDFISH_INCLUDE_DIRS "${LIBREDFISH_INCLUDE_DIR}")
    add_library(LibRedfish::LibRedfish INTERFACE IMPORTED)
    set_target_properties(LibRedfish::LibRedfish PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBREDFISH_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBREDFISH_LIBRARIES}")
endif()
