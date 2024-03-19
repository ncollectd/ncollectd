include(FindPackageHandleStandardArgs)

find_path(ODM_INCLUDE_DIR NAMES odmi.h)
find_path(CFG_INCLUDE_DIR NAMES sys/cfgodm.h)
if(ODM_INCLUDE_DIR AND CFG_INCLUDE_DIR)
    set(LIBODM_INCLUDE_DIRS ${ODM_INCLUDE_DIR} ${CFG_INCLUDE_DIR})
endif()

find_library(ODM_LIBRARY NAMES odm)
find_library(CFG_LIBRARY NAMES cfg)
if(ODM_LIBRARY AND CFG_LIBRARY)
    set(LIBODM_LIBRARIES ${ODM_LIBRARY} ${CFG_LIBRARY})
endif()

find_package_handle_standard_args(LibOdm DEFAULT_MSG LIBODM_LIBRARIES LIBODM_INCLUDE_DIRS)

mark_as_advanced(LIBODM_INCLUDE_DIR LIBODM_LIBRARIES)

if(LIBODM_FOUND AND NOT TARGET LibOdm::LibOdm)
    add_library(LibOdm::LibOdm INTERFACE IMPORTED)
    set_target_properties(LibOdm::LibOdm PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBODM_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBODM_LIBRARIES}")
endif()
