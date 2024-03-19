include(FindPackageHandleStandardArgs)

find_path(LIBDB2_INCLUDE_DIR NAMES sqlcli1.h)
find_library(LIBDB2_LIBRARIES NAMES db2)

find_package_handle_standard_args(LibDb2 DEFAULT_MSG LIBDB2_LIBRARIES LIBDB2_INCLUDE_DIR)

mark_as_advanced(LIBDB2_INCLUDE_DIR LIBDB2_LIBRARIES)

if(LIBDB2_FOUND AND NOT TARGET LibDb2::LibDb2)
    set(LIBDB2_INCLUDE_DIRS "${LIBDB2_INCLUDE_DIR}")
    add_library(LibDb2::LibDb2 INTERFACE IMPORTED)
    set_target_properties(LibDb2::LibDb2 PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBDB2_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBDB2_LIBRARIES}")
endif()
