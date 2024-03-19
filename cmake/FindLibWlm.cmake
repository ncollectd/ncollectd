include(FindPackageHandleStandardArgs)

find_path(LIBWLM_INCLUDE_DIR NAMES sys/wlm.h)
find_library(LIBWLM_LIBRARIES NAMES wlm)

find_package_handle_standard_args(LibWlm DEFAULT_MSG LIBWLM_LIBRARIES LIBWLM_INCLUDE_DIR)

mark_as_advanced(LIBWLM_INCLUDE_DIR LIBWLM_LIBRARIES)

if(LIBWLM_FOUND AND NOT TARGET LibWlm::LibWlm)
    add_library(LibWlm::LibWlm INTERFACE IMPORTED)
    set_target_properties(LibWlm::LibWlm PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBWLM_INCLUDE_DIR}"
                          INTERFACE_LINK_LIBRARIES      "${LIBWLM_LIBRARIES}")
endif()
