include(FindPackageHandleStandardArgs)

find_path(LIBPQOS_INCLUDE_DIR NAMES pqos.h)
find_library(LIBPQOS_LIBRARIES NAMES pqos)

find_package_handle_standard_args(LibPqos DEFAULT_MSG LIBPQOS_LIBRARIES LIBPQOS_INCLUDE_DIR)

mark_as_advanced(LIBPQOS_INCLUDE_DIR LIBPQOS_LIBRARIES)

if(LIBPQOS_FOUND AND NOT TARGET LibPqos::LibPqos)
    set(LIBPQOS_INCLUDE_DIRS "${LIBPQOS_INCLUDE_DIR}")
    add_library(LibPqos::LibPqos INTERFACE IMPORTED)
    set_target_properties(LibPqos::LibPqos PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBPQOS_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBPQOS_LIBRARIES}")
endif()
