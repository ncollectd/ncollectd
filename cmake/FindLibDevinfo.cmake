include(FindPackageHandleStandardArgs)

find_path(LIBDEVINFO_INCLUDE_DIR NAMES devinfo.h)
find_library(LIBDEVINFO_LIBRARIES NAMES devinfo)

find_package_handle_standard_args(LibDevinfo DEFAULT_MSG LIBDEVINFO_LIBRARIES LIBDEVINFO_INCLUDE_DIR)

mark_as_advanced(LIBDEVINFO_INCLUDE_DIR LIBDEVINFO_LIBRARIES)

if(LIBDEVINFO_FOUND AND NOT TARGET LibDevinfo::LibDevinfo)
    set(LIBDEVINFO_INCLUDE_DIRS "${LIBDEVINFO_INCLUDE_DIR}")
    add_library(LibDevinfo::LibDevinfo INTERFACE IMPORTED)
    set_target_properties(LibDevinfo::LibDevinfo PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBDEVINFO_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBDEVINFO_LIBRARIES}")
endif()
